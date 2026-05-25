/*
 * FreeRTOS+TCP V4.3.3
 * 版权所有 (C) 2022 Amazon.com, Inc. 或其附属公司。保留所有权利。
 *
 * SPDX-License-Identifier: MIT
 *
 * 特此免费授予获得本软件及相关文档文件（“软件”）副本的任何人不受限制地处理本软件的权利，
 * 包括但不限于使用、复制、修改、合并、发布、分发、再授权和/或销售本软件副本的权利，
 * 以及允许向其提供本软件的人员在遵守以下条件的前提下行使上述权利：
 *
 * 上述版权声明和本许可声明应包含在本软件的所有副本或主要部分中。
 *
 * 本软件按“原样”提供，不作任何明示或暗示的保证，包括但不限于适销性、特定用途适用性和
 * 非侵权性的保证。在任何情况下，作者或版权持有人均不对因本软件或本软件的使用或其他交易
 * 引起的任何索赔、损害或其他责任承担责任，无论是在合同诉讼、侵权行为还是其他方面。
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/**
 * @file FreeRTOS_IP.c
 * @brief 实现 FreeRTOS+TCP 网络协议栈的基本功能。
 */

/* 标准库头文件包含。 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* FreeRTOS 内核头文件包含。 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP 协议栈头文件包含。 */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_ICMP.h"
#include "FreeRTOS_IP_Timers.h"
#include "FreeRTOS_IP_Utils.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_ARP.h"
#include "FreeRTOS_ND.h"
#include "FreeRTOS_UDP_IP.h"
#include "FreeRTOS_DHCP.h"
#if ( ipconfigUSE_DHCPv6 == 1 )
    #include "FreeRTOS_DHCPv6.h"
#endif
#include "NetworkInterface.h"
#include "NetworkBufferManagement.h"
#include "FreeRTOS_DNS.h"
#include "FreeRTOS_Routing.h"

/** @brief 尝试初始化网络硬件的重试延迟时间。 */
#ifndef ipINITIALISATION_RETRY_DELAY
    #define ipINITIALISATION_RETRY_DELAY    ( pdMS_TO_TICKS( 3000U ) )
#endif

#if ( ipconfigUSE_TCP_MEM_STATS != 0 )
    #include "tcp_mem_stats.h"
#endif

/** @brief 在持有数据包时，等待 ARP 解析的最大延迟时间。 */
#ifndef ipARP_RESOLUTION_MAX_DELAY
    #define ipARP_RESOLUTION_MAX_DELAY    ( pdMS_TO_TICKS( 2000U ) )
#endif

/** @brief 在持有数据包时，等待 ND（邻居发现）解析的最大延迟时间。 */
#ifndef ipND_RESOLUTION_MAX_DELAY
    #define ipND_RESOLUTION_MAX_DELAY    ( pdMS_TO_TICKS( 2000U ) )
#endif

/** @brief 定义 ARP 解析定时器回调函数的执行频率。
 * 在 Windows 模拟器中时间较短，因为模拟时间不是真实时间。 */
#ifndef ipARP_TIMER_PERIOD_MS
    #ifdef _WINDOWS_
        #define ipARP_TIMER_PERIOD_MS    ( 500U ) /* 用于 Windows 模拟器构建。 */
    #else
        #define ipARP_TIMER_PERIOD_MS    ( 10000U )
    #endif
#endif

/** @brief 定义 ND 解析定时器回调函数的执行频率。
 * 在 Windows 模拟器中时间较短，因为模拟时间不是真实时间。 */
#ifndef ipND_TIMER_PERIOD_MS
    #ifdef _WINDOWS_
        #define ipND_TIMER_PERIOD_MS    ( 500U ) /* 用于 Windows 模拟器构建。 */
    #else
        #define ipND_TIMER_PERIOD_MS    ( 10000U )
    #endif
#endif

#if ( ( ipconfigUSE_TCP == 1 ) && !defined( ipTCP_TIMER_PERIOD_MS ) )
    /** @brief 初始化 TCP 定时器时，给它 1 秒的初始超时时间。 */
    #define ipTCP_TIMER_PERIOD_MS    ( 1000U )
#endif

#ifndef iptraceIP_TASK_STARTING
    #define iptraceIP_TASK_STARTING()    do {} while( ipFALSE_BOOL ) /**< 如果未定义 iptraceIP_TASK_STARTING，则提供空定义。 */
#endif

/** @brief 以太网头中的帧类型字段值必须大于 0x0600。
 * 如果配置选项 ipconfigFILTER_OUT_NON_ETHERNET_II_FRAMES 被启用，协议栈将丢弃帧类型值
 * 小于或等于 0x0600 的数据包。但如果禁用此选项，协议栈将继续处理这些数据包。 */
#define ipIS_ETHERNET_FRAME_TYPE_INVALID( usFrameType )    ( ( usFrameType ) <= 0x0600U )

static void prvCallDHCP_RA_Handler( NetworkEndPoint_t * pxEndPoint );

static void prvIPTask_Initialise( void );

static void prvIPTask_CheckPendingEvents( void );

/*-----------------------------------------------------------*/

/** @brief 指向等待 ARP 解析的数据包缓冲区的指针。 */
#if ipconfigIS_ENABLED( ipconfigUSE_IPv4 )
    NetworkBufferDescriptor_t * pxARPWaitingNetworkBuffer = NULL;
#endif

/** @brief 指向等待 ND 解析的数据包缓冲区的指针。 */
#if ipconfigIS_ENABLED( ipconfigUSE_IPv6 )
    NetworkBufferDescriptor_t * pxNDWaitingNetworkBuffer = NULL;
#endif

/*-----------------------------------------------------------*/

static void prvProcessIPEventsAndTimers( void );

/*
 * 主 TCP/IP 协议栈处理任务。此任务接收来自网络硬件驱动程序和使用套接字的任务的
 * 命令/事件。它还维护一组协议定时器。
 */
static void prvIPTask( void * pvParameters );

/*
 * 当网络接口有新数据可用时调用。
 */
static void prvProcessEthernetPacket( NetworkBufferDescriptor_t * const pxNetworkBuffer );

#if ( ipconfigPROCESS_CUSTOM_ETHERNET_FRAMES != 0 )

/*
 * 协议栈将为它不支持的以太网帧调用此用户钩子函数，即除 IPv4、IPv6 和 ARP 之外的帧（目前）。
 * 如果此钩子返回 eReleaseBuffer 或 eProcessBuffer，协议栈将释放并重用该网络缓冲区。
 * 如果此钩子返回 eReturnEthernetFrame，则表示用户代码已重用该网络缓冲区来生成响应，
 * 协议栈将发送该响应。
 * 如果此钩子返回 eFrameConsumed，则用户代码拥有该网络缓冲区的所有权，并在完成后负责释放它。
 */
    extern eFrameProcessingResult_t eApplicationProcessCustomFrameHook( NetworkBufferDescriptor_t * const pxNetworkBuffer );
#endif /* ( ipconfigPROCESS_CUSTOM_ETHERNET_FRAMES != 0 ) */

/*
 * 处理传入的 IP 数据包。
 */
static eFrameProcessingResult_t prvProcessIPPacket( const IPPacket_t * pxIPPacket,
                                                    NetworkBufferDescriptor_t * const pxNetworkBuffer );

/*
 * 网卡驱动程序已接收到一个数据包。如果它是链接数据包链的一部分，则遍历它以处理每一条消息。
 */
static void prvHandleEthernetPacket( NetworkBufferDescriptor_t * pxBuffer );

/* 处理 'eNetworkTxEvent'：将数据包从应用程序转发到网卡。 */
static void prvForwardTxPacket( NetworkBufferDescriptor_t * pxNetworkBuffer,
                                BaseType_t xReleaseAfterSend );

static eFrameProcessingResult_t prvProcessUDPPacket( NetworkBufferDescriptor_t * const pxNetworkBuffer );

/*-----------------------------------------------------------*/

/** @brief 用于将事件传递给 IP 任务进行处理的队列。 */
QueueHandle_t xNetworkEventQueue = NULL;

/** @brief IP 数据包标识符。 */
uint16_t usPacketIdentifier = 0U;

/** @brief 为方便起见，定义了一个全为 0xff 的 MAC 地址常量，以便快速引用。 */
const MACAddress_t xBroadcastMACAddress = { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };

/** @brief 用于确保在网络事件队列已满而无法发布网络断开事件时，不会遗漏这些事件。 */
static volatile BaseType_t xNetworkDownEventPending = pdFALSE;

/** @brief 存储处理协议栈的任务句柄。该句柄被某些实用函数（间接）使用，
 * 以确定实用函数是由任务调用的（这种情况下可以阻塞）还是由 IP 任务本身调用的（这种情况下不能阻塞）。 */
static TaskHandle_t xIPTaskHandle = NULL;

/** @brief 当 IP 任务准备好开始处理数据包时，设置为 pdTRUE。 */
static BaseType_t xIPTaskInitialised = pdFALSE;

#if ( ipconfigCHECK_IP_QUEUE_SPACE != 0 )
    /** @brief 跟踪 'xNetworkEventQueue' 中的最小剩余空间。 */
    static UBaseType_t uxQueueMinimumSpace = ipconfigEVENT_QUEUE_LENGTH;
#endif

/*-----------------------------------------------------------*/

/* Coverity 希望将 pvParameters 设为 const，但这会使其不兼容。保持函数签名不变。 */

/**
 * @brief IP 任务处理来自用户应用程序和网络接口的所有请求。
 *        它通过一个名为 'xNetworkEventQueue' 的 FreeRTOS 队列接收消息。
 *        prvIPTask() 是唯一有权访问 IP 协议栈数据的任务，因此它不需要使用互斥锁。
 *
 * @param[in] pvParameters 未使用。
 */

/** @brief 存储接口结构。 */

/* MISRA 参考 8.13.1 [未使用 const 修饰指向 const 参数的指针] */
/* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-813 */
/* coverity[misra_c_2012_rule_8_13_violation] */
static void prvIPTask( void * pvParameters )
{
    /* 仅为了防止编译器警告未使用的参数。 */
    ( void ) pvParameters;

    prvIPTask_Initialise();

    FreeRTOS_debug_printf( ( "prvIPTask 已启动\n" ) );

    /* 循环，处理 IP 事件。 */
    while( ipFOREVER() == pdTRUE )
    {
        prvProcessIPEventsAndTimers();
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief 处理发送给 IP 任务的事件并处理定时器。
 */
static void prvProcessIPEventsAndTimers( void )
{
    IPStackEvent_t xReceivedEvent;
    TickType_t xNextIPSleep;
    FreeRTOS_Socket_t * pxSocket;
    struct freertos_sockaddr xAddress;

    ipconfigWATCHDOG_TIMER();

    /* 检查解析、DHCP 和 TCP 定时器，看是否有需要执行的周期性或超时处理。 */
    vCheckNetworkTimers();

    /* 计算可接受的最大睡眠时间。 */
    xNextIPSleep = xCalculateSleepTime();

    /* 等待直到有事情可做。如果以下调用是由于超时而不是接收到消息而退出，
     * 则设置一个 'NoEvent' 值。 */
    if( xQueueReceive( xNetworkEventQueue, ( void * ) &xReceivedEvent, xNextIPSleep ) == pdFALSE )
    {
        xReceivedEvent.eEventType = eNoEvent;
    }

    #if ( ipconfigCHECK_IP_QUEUE_SPACE != 0 )
    {
        if( xReceivedEvent.eEventType != eNoEvent )
        {
            UBaseType_t uxCount;

            uxCount = uxQueueSpacesAvailable( xNetworkEventQueue );

            if( uxQueueMinimumSpace > uxCount )
            {
                uxQueueMinimumSpace = uxCount;
            }
        }
    }
    #endif /* ipconfigCHECK_IP_QUEUE_SPACE */

    iptraceNETWORK_EVENT_RECEIVED( xReceivedEvent.eEventType );

    switch( xReceivedEvent.eEventType )
    {
        case eNetworkDownEvent:
            /* 尝试建立连接。 */
            prvProcessNetworkDownEvent( ( ( NetworkInterface_t * ) xReceivedEvent.pvData ) );
            break;

        case eNetworkRxEvent:

            /* 网络硬件驱动程序已接收到一个新数据包。指向接收缓冲区的指针
             * 位于接收到的事件结构的 pvData 成员中。 */
            prvHandleEthernetPacket( ( NetworkBufferDescriptor_t * ) xReceivedEvent.pvData );
            break;

        case eNetworkTxEvent:

            /* 发送一个网络数据包。所有权将转移给驱动程序，驱动程序在交付后将释放它。 */
            prvForwardTxPacket( ( ( NetworkBufferDescriptor_t * ) xReceivedEvent.pvData ), pdTRUE );
            break;

        case eARPTimerEvent:
            /* ARP 解析定时器已过期，处理缓存。 */
            #if ipconfigIS_ENABLED( ipconfigUSE_IPv4 )
                vARPAgeCache();
            #endif /* ( ipconfigUSE_IPv4 != 0 ) */
            break;

        case eNDTimerEvent:
            /* ND 解析定时器已过期，处理缓存。 */
            #if ipconfigIS_ENABLED( ipconfigUSE_IPv6 )
                vNDAgeCache();
            #endif /* ( ipconfigUSE_IPv6 != 0 ) */
            break;

        case eSocketBindEvent:

            /* FreeRTOS_bind（一个用户 API）希望 IP 任务将套接字绑定到某个端口。
             * 端口号在套接字字段 usLocalPort 中传递。vSocketBind() 将实际绑定套接字，
             * 并且一旦触发 eSOCKET_BOUND 事件，API 就会解除阻塞。 */
            pxSocket = ( ( FreeRTOS_Socket_t * ) xReceivedEvent.pvData );
            xAddress.sin_len = ( uint8_t ) sizeof( xAddress );

            switch( pxSocket->bits.bIsIPv6 ) /* LCOV_EXCL_BR_LINE */
            {
                #if ( ipconfigUSE_IPv4 != 0 )
                    case pdFALSE_UNSIGNED:
                        xAddress.sin_family = FREERTOS_AF_INET;
                        xAddress.sin_address.ulIP_IPv4 = FreeRTOS_htonl( pxSocket->xLocalAddress.ulIP_IPv4 );
                        /* 'ulLocalAddress' 将由 vSocketBind() 再次设置。 */
                        pxSocket->xLocalAddress.ulIP_IPv4 = 0;
                        break;
                #endif /* ( ipconfigUSE_IPv4 != 0 ) */

                #if ( ipconfigUSE_IPv6 != 0 )
                    case pdTRUE_UNSIGNED:
                        xAddress.sin_family = FREERTOS_AF_INET6;
                        ( void ) memcpy( xAddress.sin_address.xIP_IPv6.ucBytes, pxSocket->xLocalAddress.xIP_IPv6.ucBytes, sizeof( xAddress.sin_address.xIP_IPv6.ucBytes ) );
                        /* 'ulLocalAddress' 将由 vSocketBind() 再次设置。 */
                        ( void ) memset( pxSocket->xLocalAddress.xIP_IPv6.ucBytes, 0, sizeof( pxSocket->xLocalAddress.xIP_IPv6.ucBytes ) );
                        break;
                #endif /* ( ipconfigUSE_IPv6 != 0 ) */

                default:
                    /* MISRA 16.4 合规性 */
                    break;
            }

            xAddress.sin_port = FreeRTOS_ntohs( pxSocket->usLocalPort );
            /* 'usLocalPort' 将由 vSocketBind() 再次设置。 */
            pxSocket->usLocalPort = 0U;
            ( void ) vSocketBind( pxSocket, &xAddress, sizeof( xAddress ), pdFALSE );

            /* 在发送 'eSocketBindEvent' 之前，已经测试了 ( xEventGroup != NULL )，
             * 所以现在可以使用它来唤醒用户。 */
            pxSocket->xEventBits |= ( EventBits_t ) eSOCKET_BOUND;
            vSocketWakeUpUser( pxSocket );
            break;

        case eSocketCloseEvent:

            /* 用户 API FreeRTOS_closesocket() 已向 IP 任务发送消息以实际关闭套接字。
             * 这在 vSocketClose() 中处理。由于套接字被关闭，无法向 API 报告结果，
             * 因此 API 不会等待结果。 */
            ( void ) vSocketClose( ( ( FreeRTOS_Socket_t * ) xReceivedEvent.pvData ) );
            break;

        case eStackTxEvent:

            /* 网络协议栈已生成要发送的数据包。指向生成缓冲区的指针
             * 位于接收到的事件结构的 pvData 成员中。 */
            vProcessGeneratedUDPPacket( ( NetworkBufferDescriptor_t * ) xReceivedEvent.pvData );
            break;

        case eDHCPEvent:
            prvCallDHCP_RA_Handler( ( ( NetworkEndPoint_t * ) xReceivedEvent.pvData ) );
            break;

        case eSocketSelectEvent:

            /* FreeRTOS_select() 已被套接字事件解除阻塞，
             * vSocketSelect() 将检查哪些套接字实际发生了事件，
             * 并更新套接字字段 xSocketBits。 */
            #if ( ipconfigSUPPORT_SELECT_FUNCTION == 1 )
            #if ( ipconfigSELECT_USES_NOTIFY != 0 )
                {
                    SocketSelectMessage_t * pxMessage = ( ( SocketSelectMessage_t * ) xReceivedEvent.pvData );
                    vSocketSelect( pxMessage->pxSocketSet );
                    ( void ) xTaskNotifyGive( pxMessage->xTaskhandle );
                }
            #else
                {
                    vSocketSelect( ( ( SocketSelect_t * ) xReceivedEvent.pvData ) );
                }
            #endif /* ( ipconfigSELECT_USES_NOTIFY != 0 ) */
            #endif /* ipconfigSUPPORT_SELECT_FUNCTION == 1 */
            break;

        case eSocketSignalEvent:
            #if ( ipconfigSUPPORT_SIGNALS != 0 )

                /* 某个任务想要向此套接字的用户发出信号，以中断对 recv() 或 select() 的调用。 */
                ( void ) FreeRTOS_SignalSocket( ( Socket_t ) xReceivedEvent.pvData );
            #endif /* ipconfigSUPPORT_SIGNALS */
            break;

        case eTCPTimerEvent:
            #if ( ipconfigUSE_TCP == 1 )

                /* 只需将 TCP 定时器标记为已过期，以便在下次调用 prvCheckNetworkTimers() 时进行处理。 */
                vIPSetTCPTimerExpiredState( pdTRUE );
            #endif /* ipconfigUSE_TCP */
            break;

        case eTCPAcceptEvent:

            /* API FreeRTOS_accept() 被调用，IP 任务现在将检查监听套接字（在 pvData 中传递）
             * 是否确实接收到了新连接。 */
            #if ( ipconfigUSE_TCP == 1 )
                pxSocket = ( ( FreeRTOS_Socket_t * ) xReceivedEvent.pvData );

                if( xTCPCheckNewClient( pxSocket ) != pdFALSE )
                {
                    pxSocket->xEventBits |= ( EventBits_t ) eSOCKET_ACCEPT;
                    vSocketWakeUpUser( pxSocket );
                }
            #endif /* ipconfigUSE_TCP */
            break;

        case eTCPNetStat:

            /* FreeRTOS_netstat() 被调用，让 IP 任务打印所有套接字及其连接的概览 */
            #if ( ( ipconfigUSE_TCP == 1 ) && ( ipconfigHAS_PRINTF == 1 ) )
                vTCPNetStat();
            #endif /* ipconfigUSE_TCP */
            break;

        case eSocketSetDeleteEvent:
            #if ( ipconfigSUPPORT_SELECT_FUNCTION == 1 )
            {
                SocketSelect_t * pxSocketSet = ( SocketSelect_t * ) ( xReceivedEvent.pvData );

                iptraceMEM_STATS_DELETE( pxSocketSet );
                vEventGroupDelete( pxSocketSet->xSelectGroup );
                vPortFree( ( void * ) pxSocketSet );
            }
            #endif /* ipconfigSUPPORT_SELECT_FUNCTION == 1 */
            break;

        case eNoEvent:
            /* xQueueReceive() 由于正常超时返回。 */
            break;

        default:
            /* 不应到达这里。 */
            break;
    }

    prvIPTask_CheckPendingEvents();
}

/*-----------------------------------------------------------*/

/**
 * @brief prvIPTask 的辅助函数，它在启动时执行首次初始化。无参数，无返回类型。
 */
static void prvIPTask_Initialise( void )
{
    NetworkInterface_t * pxInterface;

    /* 设置一些额外任务属性的可能性。 */
    iptraceIP_TASK_STARTING();

    /* 生成一条虚拟消息，表示网络连接已断开。
     * 这将导致此任务初始化网络接口。
     * 在此之后，如果以前连接的网络断开，网络接口硬件驱动程序有责任发送此消息。 */

    vNetworkTimerReload( pdMS_TO_TICKS( ipINITIALISATION_RETRY_DELAY ) );

    for( pxInterface = pxNetworkInterfaces; pxInterface != NULL; pxInterface = pxInterface->pxNext )
    {
        /* 为每个接口发布一个 'eNetworkDownEvent'。 */
        FreeRTOS_NetworkDown( pxInterface );
    }

    #if ( ipconfigUSE_TCP == 1 )
    {
        /* 初始化 TCP 定时器。 */
        vTCPTimerReload( pdMS_TO_TICKS( ipTCP_TIMER_PERIOD_MS ) );
    }
    #endif

    #if ipconfigIS_ENABLED( ipconfigUSE_IPv4 )
        /* 将 ARP 定时器标记为非活动状态，因为目前我们不在等待任何解析。 */
        vIPSetARPResolutionTimerEnableState( pdFALSE );
    #endif

    #if ipconfigIS_ENABLED( ipconfigUSE_IPv6 )
        /* 将 ND 定时器标记为非活动状态，因为目前我们不在等待任何解析。 */
        vIPSetNDResolutionTimerEnableState( pdFALSE );
    #endif

    #if ( ( ipconfigDNS_USE_CALLBACKS != 0 ) && ( ipconfigUSE_DNS != 0 ) )
    {
        /* 以下函数在 FreeRTOS_DNS.c 中声明，并且是该库的 '私有' 函数 */
        vDNSInitialise();
    }
    #endif /* ( ipconfigDNS_USE_CALLBACKS != 0 ) && ( ipconfigUSE_DNS != 0 ) */

    #if ( ( ipconfigUSE_DNS_CACHE != 0 ) && ( ipconfigUSE_DNS != 0 ) )
    {
        /* 仅清除一次 DNS 缓存。 */
        FreeRTOS_dnsclear();
    }
    #endif /* ( ( ipconfigUSE_DNS_CACHE != 0 ) && ( ipconfigUSE_DNS != 0 ) ) */

    /* 初始化完成，现在可以处理事件了。 */
    xIPTaskInitialised = pdTRUE;
}
/*-----------------------------------------------------------*/

/**
 * @brief 检查 'xNetworkDownEventPending' 的值。当非零时，将处理挂起的网络断开事件。
 */
static void prvIPTask_CheckPendingEvents( void )
{
    NetworkInterface_t * pxInterface;

    if( xNetworkDownEventPending != pdFALSE )
    {
        /* 无法将网络断开事件发布到网络事件队列，因为队列已满。
         * 由于此代码在 IP 任务中运行，可以通过直接调用 prvProcessNetworkDownEvent() 来完成。 */
        xNetworkDownEventPending = pdFALSE;

        for( pxInterface = FreeRTOS_FirstNetworkInterface();
             pxInterface != NULL;
             pxInterface = FreeRTOS_NextNetworkInterface( pxInterface ) )
        {
            if( pxInterface->bits.bCallDownEvent != pdFALSE_UNSIGNED )
            {
                prvProcessNetworkDownEvent( pxInterface );
                pxInterface->bits.bCallDownEvent = pdFALSE_UNSIGNED;
            }
        }
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief 调用 DHCP、DHCPv6 或 RA 中被激活的状态机。
 *
 * @param[in] pxEndPoint 将为其调用状态机的端点。
 */
static void prvCallDHCP_RA_Handler( NetworkEndPoint_t * pxEndPoint )
{
    BaseType_t xIsIPv6 = pdFALSE;

    #if ( ( ipconfigUSE_DHCP == 1 ) || ( ipconfigUSE_DHCPv6 == 1 ) || ( ipconfigUSE_RA == 1 ) )
        if( pxEndPoint->bits.bIPv6 != pdFALSE_UNSIGNED )
        {
            xIsIPv6 = pdTRUE;
        }
    #endif
    /* DHCP 状态机需要处理。 */
    #if ( ipconfigUSE_DHCP == 1 )
    {
        if( ( pxEndPoint->bits.bWantDHCP != pdFALSE_UNSIGNED ) && ( xIsIPv6 == pdFALSE ) )
        {
            /* 处理给定端点的 DHCP 消息。 */
            vDHCPProcess( pdFALSE, pxEndPoint );
        }
    }
    #endif /* ipconfigUSE_DHCP */
    #if ( ( ipconfigUSE_DHCPv6 == 1 ) && ( ipconfigUSE_IPv6 != 0 ) )
    {
        if( ( xIsIPv6 == pdTRUE ) && ( pxEndPoint->bits.bWantDHCP != pdFALSE_UNSIGNED ) )
        {
            /* 处理给定端点的 DHCPv6 消息。 */
            vDHCPv6Process( pdFALSE, pxEndPoint );
        }
    }
    #endif /* ipconfigUSE_DHCPv6 */
    #if ( ( ipconfigUSE_RA == 1 ) && ( ipconfigUSE_IPv6 != 0 ) )
    {
        if( ( xIsIPv6 == pdTRUE ) && ( pxEndPoint->bits.bWantRA != pdFALSE_UNSIGNED ) )
        {
            /* 处理给定端点的 RA 消息。 */
            vRAProcess( pdFALSE, pxEndPoint );
        }
    }
    #endif /* ( ( ipconfigUSE_RA == 1 ) && ( ipconfigUSE_IPv6 != 0 ) ) */

    /* 提及 pxEndPoint 和 xIsIPv6，以防它们未被使用。 */
    ( void ) pxEndPoint;
    ( void ) xIsIPv6;
}
/*-----------------------------------------------------------*/

/**
 * @brief 变量 'xIPTaskHandle' 被声明为静态。此函数提供对其的只读访问。
 *
 * @return IP 任务的句柄。
 */
TaskHandle_t FreeRTOS_GetIPTaskHandle( void )
{
    return xIPTaskHandle;
}
/*-----------------------------------------------------------*/

/**
 * @brief 当网络连接时，执行所有必需的任务。
 *
 * @param pxEndPoint 启动的端点。
 */
void vIPNetworkUpCalls( struct xNetworkEndPoint * pxEndPoint )
{
    if( pxEndPoint->bits.bIPv6 == pdTRUE_UNSIGNED )
    {
        /* IPv6 端点具有请求节点地址，需要额外的内务处理。 */
        #if ( ipconfigIS_ENABLED( ipconfigUSE_IPv6 ) )
            vManageSolicitedNodeAddress( pxEndPoint, pdTRUE );
        #endif
    }

    pxEndPoint->bits.bEndPointUp = pdTRUE_UNSIGNED;

    #if ( ipconfigUSE_NETWORK_EVENT_HOOK == 1 )
    #if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 )
        {
            vApplicationIPNetworkEventHook( eNetworkUp );
        }
    #else
        {
            vApplicationIPNetworkEventHook_Multi( eNetworkUp, pxEndPoint );
        }
    #endif
    #endif /* ipconfigUSE_NETWORK_EVENT_HOOK */

    /* 将剩余时间设置为 0，使其立即变为活动状态。 */
    if( pxEndPoint->bits.bIPv6 == pdTRUE_UNSIGNED )
    {
        #if ipconfigIS_ENABLED( ipconfigUSE_IPv6 )
            vNDTimerReload( pdMS_TO_TICKS( ipND_TIMER_PERIOD_MS ) );
        #endif
    }
    else
    {
        #if ipconfigIS_ENABLED( ipconfigUSE_IPv4 )
            vARPTimerReload( pdMS_TO_TICKS( ipARP_TIMER_PERIOD_MS ) );
        #endif
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief 处理传入的以太网数据包。
 *
 * @param[in] pxBuffer 要处理的链接/未链接网络缓冲区描述符。
 */
static void prvHandleEthernetPacket( NetworkBufferDescriptor_t * pxBuffer )
{
    #if ( ipconfigUSE_LINKED_RX_MESSAGES == 0 )
    {
        /* 当 ipconfigUSE_LINKED_RX_MESSAGES 设置为 0 时，一次只发送一个缓冲区。
         * 这是 +TCP 将消息从 MAC 传递到 TCP/IP 协议栈的默认方式。 */
        if( pxBuffer != NULL )
        {
            prvProcessEthernetPacket( pxBuffer );
        }
    }
    #else /* ipconfigUSE_LINKED_RX_MESSAGES */
    {
        NetworkBufferDescriptor_t * pxNextBuffer;

        /* 这在网络流量大时很有用。网络接口可以将接收到的数据包链接起来，
         * 并一次性将它们传递到 IP 任务，而不是将接收到的数据包一次一个地传递到 IP 任务。
         * 数据包使用 pxNextBuffer 成员进行链接。下面的循环遍历链，依次处理链中的每个数据包。 */

        /* 当链中还有另一个数据包时。 */
        while( pxBuffer != NULL )
        {
            /* 存储指向 pxBuffer 之后缓冲区的指针，以备后用。 */
            pxNextBuffer = pxBuffer->pxNextBuffer;

            /* 将其设为 NULL 以避免以后使用它。 */
            pxBuffer->pxNextBuffer = NULL;

            prvProcessEthernetPacket( pxBuffer );
            pxBuffer = pxNextBuffer;
        }
    }
    #endif /* ipconfigUSE_LINKED_RX_MESSAGES */
}
/*-----------------------------------------------------------*/

/**
 * @brief 发送一个网络数据包。
 *
 * @param[in] pxNetworkBuffer 消息缓冲区。
 * @param[in] xReleaseAfterSend 为真时，网络接口将拥有该缓冲区并负责释放它。
 */
static void prvForwardTxPacket( NetworkBufferDescriptor_t * pxNetworkBuffer,
                                BaseType_t xReleaseAfterSend )
{
    iptraceNETWORK_INTERFACE_OUTPUT( pxNetworkBuffer->xDataLength, pxNetworkBuffer->pucEthernetBuffer );

    if( pxNetworkBuffer->pxInterface != NULL )
    {
        ( void ) pxNetworkBuffer->pxInterface->pfOutput( pxNetworkBuffer->pxInterface, pxNetworkBuffer, xReleaseAfterSend );
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief 向 IP 任务发送网络断开事件。如果发布消息失败，失败将被记录在变量
 *        'xNetworkDownEventPending' 中，稍后在执行“网络断开”事件时，它将被执行。
 *
 * @param[in] pxNetworkInterface 宕机的网络接口。
 */
void FreeRTOS_NetworkDown( struct xNetworkInterface * pxNetworkInterface )
{
    IPStackEvent_t xNetworkDownEvent;
    const TickType_t xDontBlock = ( TickType_t ) 0;

    pxNetworkInterface->bits.bInterfaceUp = pdFALSE_UNSIGNED;
    xNetworkDownEvent.eEventType = eNetworkDownEvent;
    xNetworkDownEvent.pvData = pxNetworkInterface;

    /* 只需向网络任务发送相应的事件。 */
    if( xSendEventStructToIPTask( &xNetworkDownEvent, xDontBlock ) != pdPASS )
    {
        /* 无法发送消息，因此它仍处于挂起状态。 */
        pxNetworkInterface->bits.bCallDownEvent = pdTRUE;
        xNetworkDownEventPending = pdTRUE;
    }
    else
    {
        /* 消息已发送，因此不再挂起。 */
        pxNetworkInterface->bits.bCallDownEvent = pdFALSE;
    }

    iptraceNETWORK_DOWN();
}
/*-----------------------------------------------------------*/

/**
 * @brief 实用函数。从中断服务程序 (ISR) 处理网络断开事件。
 *        此函数应该从 ISR 中调用。建议从普通任务调用时使用 'FreeRTOS_NetworkDown()'。
 *
 * @param[in] pxNetworkInterface 宕机的网络接口。
 *
 * @return 如果事件处理成功，则返回 pdTRUE。否则返回 pdFALSE。
 */
BaseType_t FreeRTOS_NetworkDownFromISR( struct xNetworkInterface * pxNetworkInterface )
{
    IPStackEvent_t xNetworkDownEvent;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xNetworkDownEvent.eEventType = eNetworkDownEvent;
    xNetworkDownEvent.pvData = pxNetworkInterface;

    /* 只需向网络任务发送相应的事件。 */
    if( xQueueSendToBackFromISR( xNetworkEventQueue, &xNetworkDownEvent, &xHigherPriorityTaskWoken ) != pdPASS )
    {
        /* 无法发送消息，因此它仍处于挂起状态。 */
        pxNetworkInterface->bits.bCallDownEvent = pdTRUE;
        xNetworkDownEventPending = pdTRUE;
    }
    else
    {
        /* 消息已发送，因此不再挂起。 */
        pxNetworkInterface->bits.bCallDownEvent = pdFALSE;
        xNetworkDownEventPending = pdFALSE;
    }

    iptraceNETWORK_DOWN();

    return xHigherPriorityTaskWoken;
}
/*-----------------------------------------------------------*/

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 )

/**
 * @brief 获取一个足够大的缓冲区以容纳给定大小的 UDP 负载。
 *        注意：保留此函数是为了向后兼容，并且只会分配 IPv4 负载缓冲区。
 *        较新的设计应使用 FreeRTOS_GetUDPPayloadBuffer_Multi()，它可以根据 ucIPType 参数分配 IPv4 或 IPv6 缓冲区。
 *
 * @param[in] uxRequestedSizeBytes UDP 负载的大小。
 * @param[in] uxBlockTimeTicks 此调用可以阻塞的最大时间。此值在内部被限制。
 *
 * @return 如果创建了缓冲区，则返回指向该缓冲区的指针，否则返回 NULL 指针。
 */
    void * FreeRTOS_GetUDPPayloadBuffer( size_t uxRequestedSizeBytes,
                                         TickType_t uxBlockTimeTicks )
    {
        return FreeRTOS_GetUDPPayloadBuffer_Multi( uxRequestedSizeBytes, uxBlockTimeTicks, ipTYPE_IPv4 );
    }
#endif /* if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 ) */
/*-----------------------------------------------------------*/

/**
 * @brief 获取一个足够大的缓冲区以容纳给定大小和给定 IP 类型的 UDP 负载。
 *
 * @param[in] uxRequestedSizeBytes UDP 负载的大小。
 * @param[in] uxBlockTimeTicks 此调用可以阻塞的最大时间。此值在内部被限制。
 * @param[in] ucIPType ipTYPE_IPv4 (0x40) 或 ipTYPE_IPv6 (0x60)
 *
 * @return 如果创建了缓冲区，则返回指向该缓冲区的指针，否则返回 NULL 指针。
 */
void * FreeRTOS_GetUDPPayloadBuffer_Multi( size_t uxRequestedSizeBytes,
                                           TickType_t uxBlockTimeTicks,
                                           uint8_t ucIPType )
{
    NetworkBufferDescriptor_t * pxNetworkBuffer;
    void * pvReturn = NULL;
    TickType_t uxBlockTime = uxBlockTimeTicks;
    size_t uxPayloadOffset = 0U;

    configASSERT( ( ucIPType == ipTYPE_IPv6 ) || ( ucIPType == ipTYPE_IPv4 ) );

    /* 限制阻塞时间。这样做的原因在定义 ipconfigUDP_MAX_SEND_BLOCK_TIME_TICKS 的地方进行了解释
     * （假设使用的是官方的 FreeRTOSIPConfig.h 头文件）。 */
    if( uxBlockTime > ipconfigUDP_MAX_SEND_BLOCK_TIME_TICKS )
    {
        uxBlockTime = ipconfigUDP_MAX_SEND_BLOCK_TIME_TICKS;
    }

    switch( ucIPType ) /* LCOV_EXCL_BR_LINE */
    {
        #if ( ipconfigUSE_IPv4 != 0 )
            case ipTYPE_IPv4:
                uxPayloadOffset = sizeof( UDPPacket_t );
                break;
        #endif /* ( ipconfigUSE_IPv4 != 0 ) */

        #if ( ipconfigUSE_IPv6 != 0 )
            case ipTYPE_IPv6:
                uxPayloadOffset = sizeof( UDPPacket_IPv6_t );
                break;
        #endif /* ( ipconfigUSE_IPv6 != 0 ) */

        default:
            /* 不应到达这里。 */
            /* MISRA 16.4 合规性 */
            break;
    }

    if( uxPayloadOffset != 0U )
    {
        /* 获取具有所需存储量的网络缓冲区。 */
        pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( uxPayloadOffset + uxRequestedSizeBytes, uxBlockTime );

        if( pxNetworkBuffer != NULL )
        {
            uint8_t * pucIPType;
            size_t uxIndex = ipUDP_PAYLOAD_IP_TYPE_OFFSET;
            BaseType_t xPayloadIPTypeOffset = ( BaseType_t ) uxIndex;

            /* 如果返回了更大的缓冲区，则设置实际的数据包大小。 */
            pxNetworkBuffer->xDataLength = uxPayloadOffset + uxRequestedSizeBytes;

            /* 跳过 3 个头部。 */
            pvReturn = ( void * ) &( pxNetworkBuffer->pucEthernetBuffer[ uxPayloadOffset ] );

            /* 稍后使用指向 UDP 负载的指针来检索 NetworkBuffer。
             * 将数据包类型存储在 UDP 负载开始前 48 个字节的位置。 */
            pucIPType = ( uint8_t * ) pvReturn;
            pucIPType = &( pucIPType[ -xPayloadIPTypeOffset ] );

            /* 对于 IPv4 数据包，pucIPType 指向 pucEthernetBuffer 之前的 6 个字节处，
             * 对于 IPv6 数据包，pucIPType 将指向 IP 头的第一个字节：'ucVersionTrafficClass'。 */
            *pucIPType = ucIPType;
        }
    }

    return ( void * ) pvReturn;
}
/*-----------------------------------------------------------*/

/*_RB_ 我们是否应该添加一个错误或断言，如果任务优先级设置导致服务器无法按预期运行？ */

/*_HT_ FreeRTOS_TCP_IP.c 中有一个 bug，只在应用程序优先级过高时才会发生。
 * 由于该 bug 已修复，没有紧急理由发出警告。
 * 不过，最好使用建议的优先级方案。 */

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 ) && ( ipconfigUSE_IPv4 != 0 )

/* 提供与早期仅具有单个网络接口的 FreeRTOS+TCP 的向后兼容性。 */
    BaseType_t FreeRTOS_IPInit( const uint8_t ucIPAddress[ ipIP_ADDRESS_LENGTH_BYTES ],
                                const uint8_t ucNetMask[ ipIP_ADDRESS_LENGTH_BYTES ],
                                const uint8_t ucGatewayAddress[ ipIP_ADDRESS_LENGTH_BYTES ],
                                const uint8_t ucDNSServerAddress[ ipIP_ADDRESS_LENGTH_BYTES ],
                                const uint8_t ucMACAddress[ ipMAC_ADDRESS_LENGTH_BYTES ] )
    {
        static NetworkInterface_t xInterfaces[ 1 ];
        static NetworkEndPoint_t xEndPoints[ 1 ];

        /* 如果以下函数应该在项目链接的 NetworkInterface.c 中声明。 */
        ( void ) pxFillInterfaceDescriptor( 0, &( xInterfaces[ 0 ] ) );
        FreeRTOS_FillEndPoint( &( xInterfaces[ 0 ] ), &( xEndPoints[ 0 ] ), ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
        #if ( ipconfigUSE_DHCP != 0 )
        {
            xEndPoints[ 0 ].bits.bWantDHCP = pdTRUE;
        }
        #endif /* ipconfigUSE_DHCP */
        return FreeRTOS_IPInit_Multi();
    }
#endif /* if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 ) && ( ipconfigUSE_IPv4 != 0 ) */
/*-----------------------------------------------------------*/

/**
 * @brief 初始化 FreeRTOS-Plus-TCP 网络协议栈并初始化 IP 任务。
 *        在调用此函数之前，必须至少设置 1 个接口和 1 个端点。
 */
BaseType_t FreeRTOS_IPInit_Multi( void )
{
    BaseType_t xReturn = pdFALSE;

    /* 必须至少有一个接口和一个端点。 */
    configASSERT( FreeRTOS_FirstNetworkInterface() != NULL );

    /* 检查配置值是否正确，以及 IP 任务是否尚未初始化。 */
    vPreCheckConfigs();

    /* 尝试创建用于与 IP 任务通信的队列。 */
    #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    {
        static StaticQueue_t xNetworkEventStaticQueue;
        static uint8_t ucNetworkEventQueueStorageArea[ ipconfigEVENT_QUEUE_LENGTH * sizeof( IPStackEvent_t ) ];
        xNetworkEventQueue = xQueueCreateStatic( ipconfigEVENT_QUEUE_LENGTH,
                                                 sizeof( IPStackEvent_t ),
                                                 ucNetworkEventQueueStorageArea,
                                                 &xNetworkEventStaticQueue );
    }
    #else
    {
        xNetworkEventQueue = xQueueCreate( ipconfigEVENT_QUEUE_LENGTH, sizeof( IPStackEvent_t ) );
        configASSERT( xNetworkEventQueue != NULL );
    }
    #endif /* configSUPPORT_STATIC_ALLOCATION */

    if( xNetworkEventQueue != NULL )
    {
        #if ( configQUEUE_REGISTRY_SIZE > 0 )
        {
            /* 队列注册表通常用于辅助内核感知调试器。如果正在使用，
             * 那么调试器显示有关网络事件队列的信息将很有帮助。 */
            vQueueAddToRegistry( xNetworkEventQueue, "NetEvnt" );
        }
        #endif /* configQUEUE_REGISTRY_SIZE */

        if( xNetworkBuffersInitialise() == pdPASS )
        {
            /* 准备套接字接口。 */
            vNetworkSocketsInit();

            /* 创建处理以太网和协议栈事件的任务。 */
            #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
            {
                static StaticTask_t xIPTaskBuffer;
                static StackType_t xIPTaskStack[ ipconfigIP_TASK_STACK_SIZE_WORDS ];
                xIPTaskHandle = xTaskCreateStatic( &prvIPTask,
                                                   "IP-Task",
                                                   ipconfigIP_TASK_STACK_SIZE_WORDS,
                                                   NULL,
                                                   ipconfigIP_TASK_PRIORITY,
                                                   xIPTaskStack,
                                                   &xIPTaskBuffer );

                if( xIPTaskHandle != NULL )
                {
                    xReturn = pdTRUE;
                }
            }
            #else /* if ( configSUPPORT_STATIC_ALLOCATION == 1 ) */
            {
                xReturn = xTaskCreate( &prvIPTask,
                                       "IP-task",
                                       ipconfigIP_TASK_STACK_SIZE_WORDS,
                                       NULL,
                                       ipconfigIP_TASK_PRIORITY,
                                       &( xIPTaskHandle ) );
            }
            #endif /* configSUPPORT_STATIC_ALLOCATION */
        }
        else
        {
            FreeRTOS_debug_printf( ( "FreeRTOS_IPInit_Multi: xNetworkBuffersInitialise() 失败\n" ) );

            /* 清理。 */
            vQueueDelete( xNetworkEventQueue );
            xNetworkEventQueue = NULL;
        }
    }
    else
    {
        FreeRTOS_debug_printf( ( "FreeRTOS_IPInit_Multi: 无法创建网络事件队列\n" ) );
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 释放 UDP 负载缓冲区。
 *
 * @param[in] pvBuffer 指向要释放的 UDP 缓冲区的指针。
 */
void FreeRTOS_ReleaseUDPPayloadBuffer( void const * pvBuffer )
{
    NetworkBufferDescriptor_t * pxBuffer;

    pxBuffer = pxUDPPayloadBuffer_to_NetworkBuffer( pvBuffer );
    configASSERT( pxBuffer != NULL );
    vReleaseNetworkBufferAndDescriptor( pxBuffer );
}
/*-----------------------------------------------------------*/

#if ( ipconfigUSE_IPv4 != 0 )

/**
 * @brief 获取当前的 IPv4 地址配置。只有非 NULL 指针才会被填充。pxEndPoint 必须非 NULL。
 *
 * @param[out] pulIPAddress 分配的当前 IP 地址。
 * @param[out] pulNetMask 当前子网使用的子网掩码。
 * @param[out] pulGatewayAddress 网关地址。
 * @param[out] pulDNSServerAddress DNS 服务器地址。
 * @param[in] pxEndPoint 正被查询的端点。
 */
    void FreeRTOS_GetEndPointConfiguration( uint32_t * pulIPAddress,
                                            uint32_t * pulNetMask,
                                            uint32_t * pulGatewayAddress,
                                            uint32_t * pulDNSServerAddress,
                                            const struct xNetworkEndPoint * pxEndPoint )
    {
        if( ENDPOINT_IS_IPv4( pxEndPoint ) )
        {
            /* 将地址配置返回给调用者。 */

            if( pulIPAddress != NULL )
            {
                *pulIPAddress = pxEndPoint->ipv4_settings.ulIPAddress;
            }

            if( pulNetMask != NULL )
            {
                *pulNetMask = pxEndPoint->ipv4_settings.ulNetMask;
            }

            if( pulGatewayAddress != NULL )
            {
                *pulGatewayAddress = pxEndPoint->ipv4_settings.ulGatewayAddress;
            }

            if( pulDNSServerAddress != NULL )
            {
                *pulDNSServerAddress = pxEndPoint->ipv4_settings.ulDNSServerAddresses[ 0 ]; /*_RB_ 仅返回第一个 DNS 服务器的地址。 */
            }
        }
    }
/*-----------------------------------------------------------*/

#endif /* ( ipconfigUSE_IPv4 != 0 ) */

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 ) && ( ipconfigUSE_IPv4 != 0 )

/**
 * @brief 获取第一个端点的当前 IPv4 地址配置。只有非 NULL 指针才会被填充。
 *        注意：保留此函数是为了向后兼容。较新的设计应使用 FreeRTOS_SetEndPointConfiguration()。
 *
 * @param[out] pulIPAddress 分配的当前 IP 地址。
 * @param[out] pulNetMask 当前子网使用的子网掩码。
 * @param[out] pulGatewayAddress 网关地址。
 * @param[out] pulDNSServerAddress DNS 服务器地址。
 */
    void FreeRTOS_GetAddressConfiguration( uint32_t * pulIPAddress,
                                           uint32_t * pulNetMask,
                                           uint32_t * pulGatewayAddress,
                                           uint32_t * pulDNSServerAddress )
    {
        NetworkEndPoint_t * pxEndPoint;

        /* 获取第一个端点。 */
        pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        if( pxEndPoint != NULL )
        {
            FreeRTOS_GetEndPointConfiguration( pulIPAddress, pulNetMask,
                                               pulGatewayAddress, pulDNSServerAddress, pxEndPoint );
        }
    }
#endif /* if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 ) && ( ipconfigUSE_IPv4 != 0 ) */
/*-----------------------------------------------------------*/

#if ( ipconfigUSE_IPv4 != 0 )

/**
 * @brief 设置当前的 IPv4 网络地址配置。只有非 NULL 指针才会被使用。pxEndPoint 必须指向有效的端点。
 *
 * @param[in] pulIPAddress 分配的当前 IP 地址。
 * @param[in] pulNetMask 当前子网使用的子网掩码。
 * @param[in] pulGatewayAddress 网关地址。
 * @param[in] pulDNSServerAddress DNS 服务器地址。
 * @param[in] pxEndPoint 正被查询的端点。
 */
    void FreeRTOS_SetEndPointConfiguration( const uint32_t * pulIPAddress,
                                            const uint32_t * pulNetMask,
                                            const uint32_t * pulGatewayAddress,
                                            const uint32_t * pulDNSServerAddress,
                                            struct xNetworkEndPoint * pxEndPoint )
    {
        /* 更新地址配置。 */

        if( ENDPOINT_IS_IPv4( pxEndPoint ) )
        {
            if( pulIPAddress != NULL )
            {
                pxEndPoint->ipv4_settings.ulIPAddress = *pulIPAddress;
            }

            if( pulNetMask != NULL )
            {
                pxEndPoint->ipv4_settings.ulNetMask = *pulNetMask;
            }

            if( pulGatewayAddress != NULL )
            {
                pxEndPoint->ipv4_settings.ulGatewayAddress = *pulGatewayAddress;
            }

            if( pulDNSServerAddress != NULL )
            {
                pxEndPoint->ipv4_settings.ulDNSServerAddresses[ 0 ] = *pulDNSServerAddress;
            }
        }
    }
/*-----------------------------------------------------------*/

#endif /* ( ipconfigUSE_IPv4 != 0 ) */

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 ) && ( ipconfigUSE_IPv4 != 0 )

/**
 * @brief 设置当前的 IPv4 网络地址配置。只有非 NULL 指针才会被使用。
 *        注意：保留此函数是为了向后兼容。较新的设计应使用 FreeRTOS_SetEndPointConfiguration()。
 *
 * @param[in] pulIPAddress 分配的当前 IP 地址。
 * @param[in] pulNetMask 当前子网使用的子网掩码。
 * @param[in] pulGatewayAddress 网关地址。
 * @param[in] pulDNSServerAddress DNS 服务器地址。
 */
    void FreeRTOS_SetAddressConfiguration( const uint32_t * pulIPAddress,
                                           const uint32_t * pulNetMask,
                                           const uint32_t * pulGatewayAddress,
                                           const uint32_t * pulDNSServerAddress )
    {
        NetworkEndPoint_t * pxEndPoint;

        /* 获取第一个端点。 */
        pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        if( pxEndPoint != NULL )
        {
            FreeRTOS_SetEndPointConfiguration( pulIPAddress, pulNetMask,
                                               pulGatewayAddress, pulDNSServerAddress, pxEndPoint );
        }
    }
#endif /* if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 ) && ( ipconfigUSE_IPv4 != 0 ) */
/*-----------------------------------------------------------*/

#if ( ipconfigUSE_TCP == 1 )

/**
 * @brief 释放之前通过调用带有 'FREERTOS_ZERO_COPY' 标志的 FreeRTOS_recv() 获取的内存。
 *
 * @param[in] xSocket 读取的套接字。
 * @param[in] pvBuffer 在调用 FreeRTOS_recv() 时返回的缓冲区。
 * @param[in] xByteCount 已使用的字节数。
 *
 * @return 如果缓冲区成功释放则返回 pdPASS，否则返回 pdFAIL。
 */
    BaseType_t FreeRTOS_ReleaseTCPPayloadBuffer( Socket_t xSocket,
                                                 void const * pvBuffer,
                                                 BaseType_t xByteCount )
    {
        BaseType_t xByteCountReleased;
        BaseType_t xReturn = pdFAIL;
        uint8_t * pucData;
        size_t uxBytesAvailable = uxStreamBufferGetPtr( xSocket->u.xTCP.rxStream, &( pucData ) );

        /* 确保指针正确。 */
        configASSERT( pucData == ( uint8_t * ) pvBuffer );

        /* 避免释放超过可用的字节数。 */
        configASSERT( uxBytesAvailable >= ( size_t ) xByteCount );

        if( ( pucData == pvBuffer ) && ( uxBytesAvailable >= ( size_t ) xByteCount ) )
        {
            /* 使用 NULL 指针调用 recv 以推进环形缓冲区。 */
            xByteCountReleased = FreeRTOS_recv( xSocket,
                                                NULL,
                                                ( size_t ) xByteCount,
                                                FREERTOS_MSG_DONTWAIT );

            configASSERT( xByteCountReleased == xByteCount );

            if( xByteCountReleased == xByteCount )
            {
                xReturn = pdPASS;
            }
        }

        return xReturn;
    }
#endif /* ( ipconfigUSE_TCP == 1 ) */
/*-----------------------------------------------------------*/

#if ( ipconfigSUPPORT_OUTGOING_PINGS == 1 )

/**
 * @brief 向给定的 IP 地址发送 ping 请求。收到回复后，IP 任务将调用用户提供的函数 'vApplicationPingReplyHook()'。
 *
 * @param[in] ulIPAddress 要发送 ping 的 IP 地址。
 * @param[in] uxNumberOfBytesToSend ping 请求中的字节数。
 * @param[in] uxBlockTimeTicks 最大等待滴答数。
 *
 * @return 如果成功发送到 IP 任务进行处理，则为 ping 数据包的序列号，否则为 pdFAIL。
 */
    BaseType_t FreeRTOS_SendPingRequest( uint32_t ulIPAddress,
                                         size_t uxNumberOfBytesToSend,
                                         TickType_t uxBlockTimeTicks )
    {
        NetworkBufferDescriptor_t * pxNetworkBuffer;
        ICMPHeader_t * pxICMPHeader;
        EthernetHeader_t * pxEthernetHeader;
        BaseType_t xReturn = pdFAIL;
        static uint16_t usSequenceNumber = 0;
        uint8_t * pucChar;
        size_t uxTotalLength;
        BaseType_t xEnoughSpace;
        IPStackEvent_t xStackTxEvent = { eStackTxEvent, NULL };

        uxTotalLength = uxNumberOfBytesToSend + sizeof( ICMPPacket_t );

        if( uxNumberOfBytesToSend < ( ipconfigNETWORK_MTU - ( sizeof( IPHeader_t ) + sizeof( ICMPHeader_t ) ) ) )
        {
            xEnoughSpace = pdTRUE;
        }
        else
        {
            xEnoughSpace = pdFALSE;
        }

        if( ( uxGetNumberOfFreeNetworkBuffers() >= 4U ) && ( uxNumberOfBytesToSend >= 1U ) && ( xEnoughSpace != pdFALSE ) )
        {
            pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( uxTotalLength, uxBlockTimeTicks );

            if( pxNetworkBuffer != NULL )
            {
                pxEthernetHeader = ( ( EthernetHeader_t * ) pxNetworkBuffer->pucEthernetBuffer );
                pxEthernetHeader->usFrameType = ipIPv4_FRAME_TYPE;

                pxICMPHeader = ( ( ICMPHeader_t * ) &( pxNetworkBuffer->pucEthernetBuffer[ ipIP_PAYLOAD_OFFSET ] ) );
                usSequenceNumber++;

                /* 填写基本头部信息。 */
                pxICMPHeader->ucTypeOfMessage = ipICMP_ECHO_REQUEST;
                pxICMPHeader->ucTypeOfService = 0;
                pxICMPHeader->usIdentifier = usSequenceNumber;
                pxICMPHeader->usSequenceNumber = usSequenceNumber;

                /* 找到数据的起始位置。 */
                pucChar = ( uint8_t * ) pxICMPHeader;
                pucChar = &( pucChar[ sizeof( ICMPHeader_t ) ] );

                /* 只需将数据 memset 为固定值。 */
                ( void ) memset( pucChar, ( int ) ipECHO_DATA_FILL_BYTE, uxNumberOfBytesToSend );

                /* 消息已完成，IP 和校验和由 vProcessGeneratedUDPPacket 处理 */
                pxNetworkBuffer->pucEthernetBuffer[ ipSOCKET_OPTIONS_OFFSET ] = FREERTOS_SO_UDPCKSUM_OUT;
                pxNetworkBuffer->xIPAddress.ulIP_IPv4 = ulIPAddress;
                pxNetworkBuffer->usPort = ipPACKET_CONTAINS_ICMP_DATA;
                /* xDataLength 是总数据包的大小，包括以太网头。 */
                pxNetworkBuffer->xDataLength = uxTotalLength;

                /* 发送到协议栈。 */
                xStackTxEvent.pvData = pxNetworkBuffer;

                if( xSendEventStructToIPTask( &( xStackTxEvent ), uxBlockTimeTicks ) != pdPASS )
                {
                    vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );
                    iptraceSTACK_TX_EVENT_LOST( ipSTACK_TX_EVENT );
                }
                else
                {
                    xReturn = ( BaseType_t ) usSequenceNumber;
                }
            }
        }
        else
        {
            /* 请求的字节数将无法容纳网络缓冲区中的可用空间。 */
        }

        return xReturn;
    }

#endif /* ipconfigSUPPORT_OUTGOING_PINGS == 1 */
/*-----------------------------------------------------------*/

/**
 * @brief 向 IP 任务发送事件。它内部调用 'xSendEventStructToIPTask'。
 *
 * @param[in] eEvent 要发送的事件。
 *
 * @return 如果事件已发送（或达到了预期效果）则返回 pdPASS。否则返回 pdFAIL。
 */
BaseType_t xSendEventToIPTask( eIPEvent_t eEvent )
{
    IPStackEvent_t xEventMessage;
    const TickType_t xDontBlock = ( TickType_t ) 0;

    xEventMessage.eEventType = eEvent;
    xEventMessage.pvData = ( void * ) NULL;

    return xSendEventStructToIPTask( &xEventMessage, xDontBlock );
}
/*-----------------------------------------------------------*/

/**
 * @brief 以结构体的形式向 IP 任务发送事件以进行处理。
 *
 * @param[in] pxEvent 要发送的事件。
 * @param[in] uxTimeout 在队列已满时等待的超时时间。0 表示非阻塞调用。
 *
 * @return 如果事件已发送（或达到了预期效果）则返回 pdPASS。否则返回 pdFAIL。
 */
BaseType_t xSendEventStructToIPTask( const IPStackEvent_t * pxEvent,
                                     TickType_t uxTimeout )
{
    BaseType_t xReturn, xSendMessage;
    TickType_t uxUseTimeout = uxTimeout;

    if( ( xIPIsNetworkTaskReady() == pdFALSE ) && ( pxEvent->eEventType != eNetworkDownEvent ) )
    {
        /* 如果 IP 任务尚未准备好，则只允许 eNetworkDownEvent 事件。
         * 不打算尝试发送消息，因此发送失败。 */
        xReturn = pdFAIL;
    }
    else
    {
        xSendMessage = pdTRUE;

        #if ( ipconfigUSE_TCP == 1 )
        {
            if( pxEvent->eEventType == eTCPTimerEvent )
            {
                /* TCP 定时器事件被发送以在 xTCPTimer 过期时唤醒定时器任务，
                 * 但如果 IP 任务已经醒着处理其他消息，则没有必要发送它们。 */
                vIPSetTCPTimerExpiredState( pdTRUE );

                if( uxQueueMessagesWaiting( xNetworkEventQueue ) != 0U )
                {
                    /* 实际上不会发送消息，但这不是失败，因为不需要发送该消息。 */
                    xSendMessage = pdFALSE;
                }
            }
        }
        #endif /* ipconfigUSE_TCP */

        if( xSendMessage != pdFALSE )
        {
            /* IP 任务在等待自己响应时不能阻塞自己。 */
            if( ( xIsCallingFromIPTask() == pdTRUE ) && ( uxUseTimeout > ( TickType_t ) 0U ) )
            {
                uxUseTimeout = ( TickType_t ) 0;
            }

            xReturn = xQueueSendToBack( xNetworkEventQueue, pxEvent, uxUseTimeout );

            if( xReturn == pdFAIL )
            {
                /* 本应向 IP 任务发送消息，但未成功。 */
                FreeRTOS_debug_printf( ( "xSendEventStructToIPTask: 无法添加 %d\n", pxEvent->eEventType ) );
                iptraceSTACK_TX_EVENT_LOST( pxEvent->eEventType );
            }
        }
        else
        {
            /* 没有必要发送消息来处理事件，因此即使消息未发送，调用也是成功的。 */
            xReturn = pdPASS;
        }
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 根据数据包中的 IP 地址决定是否应处理此数据包。
 *
 * @param[in] pucEthernetBuffer 考虑的以太网数据包。
 *
 * @return 枚举值，指示是释放还是处理该数据包。
 */
eFrameProcessingResult_t eConsiderFrameForProcessing( const uint8_t * const pucEthernetBuffer )
{
    eFrameProcessingResult_t eReturn = eReleaseBuffer;

    do
    {
        const EthernetHeader_t * pxEthernetHeader = NULL;
        const NetworkEndPoint_t * pxEndPoint = NULL;
        uint16_t usFrameType;

        /* 首先，检查数据包缓冲区是否非空。 */
        if( pucEthernetBuffer == NULL )
        {
            /* 数据包缓冲区为空 - 释放它。 */
            break;
        }

        /* 将缓冲区映射到以太网头结构体，以便轻松访问字段。 */
        /* MISRA 参考 11.3.1 [未对齐访问] */
        /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
        /* coverity[misra_c_2012_rule_11_3_violation] */
        pxEthernetHeader = ( ( const EthernetHeader_t * ) pucEthernetBuffer );
        usFrameType = pxEthernetHeader->usFrameType;

        /* 其次，基于以太网帧类型进行过滤。 */
        /* 以太网头中的帧类型字段值必须大于 0x0600。 */
        if( ipIS_ETHERNET_FRAME_TYPE_INVALID( FreeRTOS_ntohs( usFrameType ) ) )
        {
            /* 该数据包不是以太网 II 帧 */
            #if ipconfigIS_ENABLED( ipconfigFILTER_OUT_NON_ETHERNET_II_FRAMES )
                /* 过滤已启用 - 释放它。 */
                break;
            #else
                /* 过滤已禁用 - 继续过滤检查。 */
            #endif
        }
        else if( usFrameType == ipARP_FRAME_TYPE )
        {
            /* 该帧是 ARP 类型 */
            #if ipconfigIS_DISABLED( ipconfigUSE_IPv4 )
                /* IPv4 已禁用 - 释放它。 */
                break;
            #else
                /*  IPv4 已启用 - 继续过滤检查。 */
            #endif
        }
        else if( usFrameType == ipIPv4_FRAME_TYPE )
        {
            /* 该帧是 IPv4 类型 */
            #if ipconfigIS_DISABLED( ipconfigUSE_IPv4 )
                /* IPv4 已禁用 - 释放它。 */
                break;
            #else
                /* IPv4 已启用 - 继续过滤检查。 */
            #endif
        }
        else if( usFrameType == ipIPv6_FRAME_TYPE )
        {
            /* 该帧是 IPv6 类型 */
            #if ipconfigIS_DISABLED( ipconfigUSE_IPv6 )
                /* IPv6 已禁用 - 释放它。 */
                break;
            #else
                /* IPv6 已启用 - 继续过滤检查。 */
            #endif
        }
        else
        {
            /* 该帧是不支持的以太网 II 类型 */
            #if ipconfigIS_ENABLED( ipconfigPROCESS_CUSTOM_ETHERNET_FRAMES )

                /* 处理自定义以太网帧已启用。无需进一步测试。
                 * 接受该帧，无论它是单播、多播还是广播。 */
                eReturn = eProcessBuffer;
            #endif
            break;
        }

        /* 第三，基于目标 MAC 地址进行过滤。 */
        pxEndPoint = FreeRTOS_FindEndPointOnMAC( &( pxEthernetHeader->xDestinationAddress ), NULL );

        if( pxEndPoint != NULL )
        {
            /* 找到了目标端点 - 继续过滤检查。 */
        }
        else if( memcmp( xBroadcastMACAddress.ucBytes, pxEthernetHeader->xDestinationAddress.ucBytes, sizeof( MACAddress_t ) ) == 0 )
        {
            /* 该数据包是广播 - 继续过滤检查。 */
        }
        else if( memcmp( xLLMNR_MacAddress.ucBytes, pxEthernetHeader->xDestinationAddress.ucBytes, sizeof( MACAddress_t ) ) == 0 )
        {
            /* 该数据包是使用 IPv4 的 LLMNR 请求 */
            #if ( ipconfigIS_DISABLED( ipconfigUSE_DNS ) || ipconfigIS_DISABLED( ipconfigUSE_LLMNR ) || ipconfigIS_DISABLED( ipconfigUSE_IPv4 ) )
                /* DNS、LLMNR 或 IPv4 已禁用 - 释放它。 */
                break;
            #else
                /* DNS、LLMNR 和 IPv4 已启用 - 继续过滤检查。 */
            #endif
        }
        else if( memcmp( xLLMNR_MacAddressIPv6.ucBytes, pxEthernetHeader->xDestinationAddress.ucBytes, sizeof( MACAddress_t ) ) == 0 )
        {
            /* 该数据包是使用 IPv6 的 LLMNR 请求 */
            #if ( ipconfigIS_DISABLED( ipconfigUSE_DNS ) || ipconfigIS_DISABLED( ipconfigUSE_LLMNR ) || ipconfigIS_DISABLED( ipconfigUSE_IPv6 ) )
                /* DNS、LLMNR 或 IPv6 已禁用 - 释放它。 */
                break;
            #else
                /* DNS、LLMNR 和 IPv6 已启用 - 继续过滤检查。 */
            #endif
        }
        else if( memcmp( xMDNS_MacAddress.ucBytes, pxEthernetHeader->xDestinationAddress.ucBytes, sizeof( MACAddress_t ) ) == 0 )
        {
            /* 该数据包是使用 IPv4 的 MDNS 请求 */
            #if ( ipconfigIS_DISABLED( ipconfigUSE_DNS ) || ipconfigIS_DISABLED( ipconfigUSE_MDNS ) || ipconfigIS_DISABLED( ipconfigUSE_IPv4 ) )
                /* DNS、MDNS 或 IPv4 已禁用 - 释放它。 */
                break;
            #else
                /* DNS、MDNS 和 IPv4 已启用 - 继续过滤检查。 */
            #endif
        }
        else if( memcmp( xMDNS_MacAddressIPv6.ucBytes, pxEthernetHeader->xDestinationAddress.ucBytes, sizeof( MACAddress_t ) ) == 0 )
        {
            /* 该数据包是使用 IPv6 的 MDNS 请求 */
            #if ( ipconfigIS_DISABLED( ipconfigUSE_DNS ) || ipconfigIS_DISABLED( ipconfigUSE_MDNS ) || ipconfigIS_DISABLED( ipconfigUSE_IPv6 ) )
                /* DNS、MDNS 或 IPv6 已禁用 - 释放它。 */
                break;
            #else
                /* DNS、MDNS 和 IPv6 已启用 - 继续过滤检查。 */
            #endif
        }
        else if( ( pxEthernetHeader->xDestinationAddress.ucBytes[ 0 ] == ipMULTICAST_MAC_ADDRESS_IPv4_0 ) &&
                 ( pxEthernetHeader->xDestinationAddress.ucBytes[ 1 ] == ipMULTICAST_MAC_ADDRESS_IPv4_1 ) &&
                 ( pxEthernetHeader->xDestinationAddress.ucBytes[ 2 ] == ipMULTICAST_MAC_ADDRESS_IPv4_2 ) &&
                 ( pxEthernetHeader->xDestinationAddress.ucBytes[ 3 ] <= 0x7fU ) )
        {
            /* 该数据包是 IPv4 多播 */
            #if ipconfigIS_DISABLED( ipconfigUSE_IPv4 )
                /* IPv4 已禁用 - 释放它。 */
                break;
            #else
                /* IPv4 已启用 - 继续过滤检查。 */
            #endif
        }
        else if( ( pxEthernetHeader->xDestinationAddress.ucBytes[ 0 ] == ipMULTICAST_MAC_ADDRESS_IPv6_0 ) &&
                 ( pxEthernetHeader->xDestinationAddress.ucBytes[ 1 ] == ipMULTICAST_MAC_ADDRESS_IPv6_1 ) )
        {
            /* 该数据包是 IPv6 多播 */
            #if ipconfigIS_DISABLED( ipconfigUSE_IPv6 )
                /* IPv6 已禁用 - 释放它。 */
                break;
            #else
                /* IPv6 已启用 - 继续过滤检查。 */
            #endif
        }
        else
        {
            /* 该数据包不是广播，也不是发往本节点的 - 释放它 */
            break;
        }

        /* 所有检查均已通过，处理该数据包。 */
        eReturn = eProcessBuffer;
    } while( ipFALSE_BOOL );

    return eReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 处理以太网数据包。
 *
 * @param[in,out] pxNetworkBuffer 包含以太网数据包的网络缓冲区。如果缓冲区足够大，可能会被重用于发送回复。
 */
static void prvProcessEthernetPacket( NetworkBufferDescriptor_t * const pxNetworkBuffer )
{
    const EthernetHeader_t * pxEthernetHeader;
    eFrameProcessingResult_t eReturned = eReleaseBuffer;

    /* 使用 do{}while(pdFALSE) 以允许使用 break; */
    do
    {
        /* prvHandleEthernetPacket() 已经检查了 ( pxNetworkBuffer != NULL )，
         * 因此跳出 do{}while() 并让此函数的后半部分处理 pxNetworkBuffer 的释放是安全的 */

        if( ( pxNetworkBuffer->pxInterface == NULL ) || ( pxNetworkBuffer->pxEndPoint == NULL ) )
        {
            break;
        }

        /* 在此点之后，
         * ( pxNetworkBuffer != NULL ),
         * ( pxNetworkBuffer->pxInterface != NULL ),
         * ( pxNetworkBuffer->pxEndPoint != NULL ),
         * 此外，FreeRTOS_FillEndPoint() 和 FreeRTOS_FillEndPoint_IPv6() 保证
         * 端点始终分配有有效的接口，因此：
         * ( pxNetworkBuffer->pxEndPoint->pxInterface != NULL )
         * 以上各项在处理传入数据包的代码中都不需要再次检查。 */

        iptraceNETWORK_INTERFACE_INPUT( pxNetworkBuffer->xDataLength, pxNetworkBuffer->pucEthernetBuffer );

        /* 解析以太网帧。 */
        if( pxNetworkBuffer->xDataLength < sizeof( EthernetHeader_t ) )
        {
            break;
        }

        /* 将缓冲区映射到以太网头结构体，以便轻松访问字段。 */

        /* MISRA 参考 11.3.1 [未对齐访问] */
        /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
        /* coverity[misra_c_2012_rule_11_3_violation] */
        pxEthernetHeader = ( ( const EthernetHeader_t * ) pxNetworkBuffer->pucEthernetBuffer );

        /* 条件 "eReturned == eProcessBuffer" 必须为真。 */
        #if ( ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES == 0 )
            if( eConsiderFrameForProcessing( pxNetworkBuffer->pucEthernetBuffer ) == eProcessBuffer )
        #endif
        {
            /* 解析接收到的以太网数据包。 */
            switch( pxEthernetHeader->usFrameType )
            {
                #if ( ipconfigUSE_IPv4 != 0 )
                    case ipARP_FRAME_TYPE:

                        /* 以太网帧包含 ARP 数据包。 */
                        if( pxNetworkBuffer->xDataLength >= sizeof( ARPPacket_t ) )
                        {
                            /* MISRA 参考 11.3.1 [未对齐访问] */
                            /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
                            /* coverity[misra_c_2012_rule_11_3_violation] */
                            eReturned = eARPProcessPacket( pxNetworkBuffer );
                        }
                        else
                        {
                            eReturned = eReleaseBuffer;
                        }
                        break;
                #endif /* ( ipconfigUSE_IPv4 != 0 ) */

                case ipIPv4_FRAME_TYPE:
                case ipIPv6_FRAME_TYPE:

                    /* 以太网帧包含 IP 数据包。 */
                    if( pxNetworkBuffer->xDataLength >= sizeof( IPPacket_t ) )
                    {
                        /* MISRA 参考 11.3.1 [未对齐访问] */
                        /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
                        /* coverity[misra_c_2012_rule_11_3_violation] */
                        eReturned = prvProcessIPPacket( ( ( IPPacket_t * ) pxNetworkBuffer->pucEthernetBuffer ), pxNetworkBuffer );
                    }
                    else
                    {
                        eReturned = eReleaseBuffer;
                    }

                    break;

                default:
                    #if ( ipconfigPROCESS_CUSTOM_ETHERNET_FRAMES != 0 )
                        /* 自定义帧处理程序。 */
                        eReturned = eApplicationProcessCustomFrameHook( pxNetworkBuffer );
                    #else
                        /* 不处理其他数据包类型。无事可做。 */
                        eReturned = eReleaseBuffer;
                    #endif
                    break;
            } /* switch( pxEthernetHeader->usFrameType ) */
        }
    } while( pdFALSE );

    /* 执行由处理以太网帧产生的任何操作。 */
    switch( eReturned )
    {
        case eReturnEthernetFrame:

            /* 以太网帧已更新（可能是解析请求或 PING 请求？），应将其发送回源地址。 */
            vReturnEthernetFrame( pxNetworkBuffer, pdTRUE );

            /* 参数 pdTRUE：帧传输后必须释放缓冲区 */
            break;

        case eFrameConsumed:

            /* 该帧正在某处使用，暂不要释放缓冲区。 */
            break;

        case eWaitingResolution:

            if( ( pxEthernetHeader->usFrameType == ipIPv4_FRAME_TYPE ) || ( pxEthernetHeader->usFrameType == ipARP_FRAME_TYPE ) )
            {
                #if ipconfigIS_ENABLED( ipconfigUSE_IPv4 )
                    if( pxARPWaitingNetworkBuffer == NULL )
                    {
                        pxARPWaitingNetworkBuffer = pxNetworkBuffer;
                        vIPTimerStartARPResolution( ipARP_RESOLUTION_MAX_DELAY );

                        iptraceDELAYED_ARP_REQUEST_STARTED();
                    }
                    else
                #endif /* if ipconfigIS_ENABLED( ipconfigUSE_IPv4 ) */
                {
                    /* 我们已经在等待一个解析。此帧将被丢弃。 */
                    vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );

                    iptraceDELAYED_ARP_BUFFER_FULL();
                }

                break;
            }
            else if( pxEthernetHeader->usFrameType == ipIPv6_FRAME_TYPE )
            {
                #if ipconfigIS_ENABLED( ipconfigUSE_IPv6 )
                    if( pxNDWaitingNetworkBuffer == NULL )
                    {
                        pxNDWaitingNetworkBuffer = pxNetworkBuffer;
                        vIPTimerStartNDResolution( ipND_RESOLUTION_MAX_DELAY );

                        iptraceDELAYED_ND_REQUEST_STARTED();
                    }
                    else
                #endif /* if ipconfigIS_ENABLED( ipconfigUSE_IPv6 ) */
                {
                    /* 我们已经在等待一个解析。此帧将被丢弃。 */
                    vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );

                    iptraceDELAYED_ND_BUFFER_FULL();
                }

                break;
            }
            else
            {
                /* 未知的帧类型，丢弃数据包。 */
                vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );
            }

            break;

        case eReleaseBuffer:
        case eProcessBuffer:
        default:

            /* 该帧未在任何地方使用，包含该帧的 NetworkBufferDescriptor_t 结构应该直接释放回空闲缓冲区列表。 */
            vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );
            break;
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief 检查 UDP 数据包的大小并将其转发到 UDP 模块 ( xProcessReceivedUDPPacket() )
 * @param[in] pxNetworkBuffer 包含 UDP 数据包的网络缓冲区。
 * @return eReleaseBuffer (请释放缓冲区)。
 *         eFrameConsumed (缓冲区现已释放)。
 */

static eFrameProcessingResult_t prvProcessUDPPacket( NetworkBufferDescriptor_t * const pxNetworkBuffer )
{
    eFrameProcessingResult_t eReturn = eReleaseBuffer;
    BaseType_t xIsWaitingResolution = pdFALSE;
    /* IP 数据包包含一个 UDP 帧。 */
    /* MISRA 参考 11.3.1 [未对齐访问] */
    /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
    /* coverity[misra_c_2012_rule_11_3_violation] */
    const UDPPacket_t * pxUDPPacket = ( ( UDPPacket_t * ) pxNetworkBuffer->pucEthernetBuffer );
    const UDPHeader_t * pxUDPHeader = &( pxUDPPacket->xUDPHeader );

    size_t uxMinSize = ipSIZE_OF_ETH_HEADER + ( size_t ) uxIPHeaderSizePacket( pxNetworkBuffer ) + ipSIZE_OF_UDP_HEADER;
    size_t uxLength;
    uint16_t usLength;

    #if ( ipconfigUSE_IPv6 != 0 )
        if( pxUDPPacket->xEthernetHeader.usFrameType == ipIPv6_FRAME_TYPE )
        {
            const ProtocolHeaders_t * pxProtocolHeaders;

            /* MISRA 参考 11.3.1 [未对齐访问] */
            /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
            /* coverity[misra_c_2012_rule_11_3_violation] */
            pxProtocolHeaders = ( ( ProtocolHeaders_t * ) &( pxNetworkBuffer->pucEthernetBuffer[ ipSIZE_OF_ETH_HEADER + ipSIZE_OF_IPv6_HEADER ] ) );
            pxUDPHeader = &( pxProtocolHeaders->xUDPHeader );
        }
    #endif /* ( ipconfigUSE_IPv6 != 0 ) */

    usLength = FreeRTOS_ntohs( pxUDPHeader->usLength );
    uxLength = ( size_t ) usLength;

    /* 请注意，在校验和生成之前所需的头部值，因为校验和伪头部可能会破坏其中一些值。 */
    #if ( ipconfigUSE_IPv4 != 0 )
        if( ( pxUDPPacket->xEthernetHeader.usFrameType == ipIPv4_FRAME_TYPE ) &&
            ( usLength > ( FreeRTOS_ntohs( pxUDPPacket->xIPHeader.usLength ) - uxIPHeaderSizePacket( pxNetworkBuffer ) ) ) )
        {
            eReturn = eReleaseBuffer;
        }
        else
    #endif /* ( ipconfigUSE_IPv4 != 0 ) */

    if( ( pxNetworkBuffer->xDataLength >= uxMinSize ) &&
        ( uxLength >= sizeof( UDPHeader_t ) ) )
    {
        size_t uxPayloadSize_1, uxPayloadSize_2;

        /* 确保下游 UDP 数据包处理使用以下两者中较小的一个：
         * 实际网络缓冲区以太网帧长度，或发送方 UDP 包头负载长度减去 UDP 头的大小。
         *
         * 此实现中的 UDP 数据包结构大小包括以太网头的大小、IP 头的大小和 UDP 头的大小。 */
        uxPayloadSize_1 = pxNetworkBuffer->xDataLength - uxMinSize;
        uxPayloadSize_2 = uxLength - ipSIZE_OF_UDP_HEADER;

        if( uxPayloadSize_1 > uxPayloadSize_2 )
        {
            pxNetworkBuffer->xDataLength = uxPayloadSize_2 + uxMinSize;
        }

        pxNetworkBuffer->usPort = pxUDPHeader->usSourcePort;
        pxNetworkBuffer->xIPAddress.ulIP_IPv4 = pxUDPPacket->xIPHeader.ulSourceIPAddress;

        /* ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM:
         * 在某些情况下，上层校验和已由 NIC 驱动程序计算。 */

        /* 将数据包负载传递给 UDP 套接字实现。 */
        if( xProcessReceivedUDPPacket( pxNetworkBuffer,
                                       pxUDPHeader->usDestinationPort,
                                       &( xIsWaitingResolution ) ) == pdPASS )
        {
            eReturn = eFrameConsumed;
        }
        else
        {
            /* 此数据包是否要搁置等待解析。 */
            if( xIsWaitingResolution == pdTRUE )
            {
                eReturn = eWaitingResolution;
            }
        }
    }
    else
    {
        /* 长度检查失败，缓冲区将被释放。 */
    }

    return eReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 处理一个 IP 数据包。
 *
 * @param[in] pxIPPacket 要处理的 IP 数据包。
 * @param[in] pxNetworkBuffer 具有 IP 数据包的网络缓冲区描述符。
 *
 * @return 一个枚举值，用于指示数据包应被释放/保留/处理等。
 */
static eFrameProcessingResult_t prvProcessIPPacket( const IPPacket_t * pxIPPacket,
                                                    NetworkBufferDescriptor_t * const pxNetworkBuffer )
{
    eFrameProcessingResult_t eReturn;
    UBaseType_t uxHeaderLength = ipSIZE_OF_IPv4_HEADER;
    uint8_t ucProtocol = 0U;

    #if ( ipconfigUSE_IPv6 != 0 )
        const IPHeader_IPv6_t * pxIPHeader_IPv6 = NULL;
    #endif /* ( ipconfigUSE_IPv6 != 0 ) */

    #if ( ipconfigUSE_IPv4 != 0 )
        const IPHeader_t * pxIPHeader = &( pxIPPacket->xIPHeader );
    #endif /* ( ipconfigUSE_IPv4 != 0 ) */

    switch( pxIPPacket->xEthernetHeader.usFrameType )
    {
        #if ( ipconfigUSE_IPv6 != 0 )
            case ipIPv6_FRAME_TYPE:

                if( pxNetworkBuffer->xDataLength < sizeof( IPPacket_IPv6_t ) )
                {
                    /* 数据包大小小于最小 IPv6 数据包。 */
                    eReturn = eReleaseBuffer;
                }
                else
                {
                    /* MISRA 参考 11.3.1 [未对齐访问] */
                    /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
                    /* coverity[misra_c_2012_rule_11_3_violation] */
                    pxIPHeader_IPv6 = ( ( const IPHeader_IPv6_t * ) &( pxNetworkBuffer->pucEthernetBuffer[ ipSIZE_OF_ETH_HEADER ] ) );

                    uxHeaderLength = ipSIZE_OF_IPv6_HEADER;
                    ucProtocol = pxIPHeader_IPv6->ucNextHeader;
                    /* MISRA 参考 11.3.1 [未对齐访问] */
                    /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
                    /* coverity[misra_c_2012_rule_11_3_violation] */
                    eReturn = prvAllowIPPacketIPv6( ( ( const IPHeader_IPv6_t * ) &( pxIPPacket->xIPHeader ) ), pxNetworkBuffer, uxHeaderLength );

                    /* IP 头类型被复制到消息开始前几个字节的一个特殊保留位置。
                     * 在 IPv6 的情况下，此值实际上从未被使用，下面的行可以安全地删除而不会产生不良影响。
                     * 我们只存储它以帮助调试。 */
                    pxNetworkBuffer->pucEthernetBuffer[ 0 - ( BaseType_t ) ipIP_TYPE_OFFSET ] = pxIPHeader_IPv6->ucVersionTrafficClass;
                }
                break;
        #endif /* ( ipconfigUSE_IPv6 != 0 ) */

        #if ( ipconfigUSE_IPv4 != 0 )
            case ipIPv4_FRAME_TYPE:
               {
                   size_t uxLength = ( size_t ) pxIPHeader->ucVersionHeaderLength;

                   /* 检查 IP 头是否可接受以及它是否有我们的目的地。
                    * 'ucVersionHeaderLength' 的最低四位表示以 4 倍数计的 IP 头长度。 */
                   uxHeaderLength = ( size_t ) ( ( uxLength & 0x0FU ) << 2 );

                   if( ( uxHeaderLength > ( pxNetworkBuffer->xDataLength - ipSIZE_OF_ETH_HEADER ) ) ||
                       ( uxHeaderLength < ipSIZE_OF_IPv4_HEADER ) )
                   {
                       eReturn = eReleaseBuffer;
                   }
                   else
                   {
                       ucProtocol = pxIPPacket->xIPHeader.ucProtocol;
                       /* 检查 IP 头是否可接受以及它是否有我们的目的地。 */
                       eReturn = prvAllowIPPacketIPv4( pxIPPacket, pxNetworkBuffer, uxHeaderLength );

                       {
                           /* IP 头类型被复制到消息开始前几个字节的一个特殊保留位置。
                            * 稍后在使用 UDP 负载缓冲区时可能需要它。 */
                           pxNetworkBuffer->pucEthernetBuffer[ 0 - ( BaseType_t ) ipIP_TYPE_OFFSET ] = pxIPHeader->ucVersionHeaderLength;
                       }
                   }

                   break;
               }
        #endif /* ( ipconfigUSE_IPv4 != 0 ) */

        default:
            eReturn = eReleaseBuffer;
            FreeRTOS_debug_printf( ( "prvProcessIPPacket: 未定义的帧类型 \n" ) );
            /* MISRA 16.4 合规性 */
            break;
    }

    /* MISRA 参考 14.3.1 [配置相关的不变量] */
    /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-143 */
    /* coverity[misra_c_2012_rule_14_3_violation] */
    /* coverity[cond_const] */
    if( eReturn == eProcessBuffer )
    {
        /* 是否有 IP 选项。 */
        /* 默认情况永远不会触发，因为在上一步中 eReturn 不是 eProcessBuffer。 */
        switch( pxIPPacket->xEthernetHeader.usFrameType ) /* LCOV_EXCL_BR_LINE */
        {
            #if ( ipconfigUSE_IPv4 != 0 )
                case ipIPv4_FRAME_TYPE:

                    if( uxHeaderLength > ipSIZE_OF_IPv4_HEADER )
                    {
                        /* IP 头的大小大于 20 字节。额外的空间用于 IP 选项。 */
                        eReturn = prvCheckIP4HeaderOptions( pxNetworkBuffer );
                    }
                    break;
            #endif /* ( ipconfigUSE_IPv4 != 0 ) */

            #if ( ipconfigUSE_IPv6 != 0 )
                case ipIPv6_FRAME_TYPE:

                    if( xGetExtensionOrder( ucProtocol, 0U ) > 0 )
                    {
                        eReturn = eHandleIPv6ExtensionHeaders( pxNetworkBuffer, pdTRUE );

                        if( eReturn != eReleaseBuffer )
                        {
                            /* 忽略 `pxIPHeader_IPv6` 的警告。 */
                            ucProtocol = pxIPHeader_IPv6->ucNextHeader;
                        }
                    }
                    break;
            #endif /* ( ipconfigUSE_IPv6 != 0 ) */

            /* 默认情况永远不会触发，因为在上一步中 eReturn 不是 eProcessBuffer。 */
            default:   /* LCOV_EXCL_LINE */
                /* MISRA 16.4 合规性 */
                break; /* LCOV_EXCL_LINE */
        }

        /* MISRA 参考 14.3.1 [配置相关的不变量] */
        /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-143 */
        /* coverity[misra_c_2012_rule_14_3_violation] */
        /* coverity[const] */
        if( eReturn != eReleaseBuffer )
        {
            /* 将 IP 和 MAC 地址添加到缓存中（如果它们尚未存在），否则刷新现有条目的生存期。 */
            if( ucProtocol != ( uint8_t ) ipPROTOCOL_UDP )
            {
                if( xCheckRequiresResolution( pxNetworkBuffer ) == pdTRUE )
                {
                    eReturn = eWaitingResolution;
                }
                else
                {
                    /* 用接收到的数据包的 IP/MAC 地址刷新缓存。对于 UDP 数据包，
                     * 这将在 xProcessReceivedUDPPacket() 中稍后完成，一旦知道消息将被处理。
                     * 这将防止缓存被无用的广播数据包的 IP 地址覆盖。 */
                    /* 默认情况永远不会触发，因为在上一步中 eReturn 不是 eProcessBuffer。 */
                    switch( pxIPPacket->xEthernetHeader.usFrameType ) /* LCOV_EXCL_BR_LINE */
                    {
                        #if ( ipconfigUSE_IPv6 != 0 )
                            case ipIPv6_FRAME_TYPE:
                                vNDRefreshCacheEntry( &( pxIPPacket->xEthernetHeader.xSourceAddress ), &( pxIPHeader_IPv6->xSourceAddress ), pxNetworkBuffer->pxEndPoint );
                                break;
                        #endif /* ( ipconfigUSE_IPv6 != 0 ) */

                        #if ( ipconfigUSE_IPv4 != 0 )
                            case ipIPv4_FRAME_TYPE:
                                /* 因为接收到了数据包，所以刷新此缓存条目的生存期。 */
                                vARPRefreshCacheEntryAge( &( pxIPPacket->xEthernetHeader.xSourceAddress ), pxIPHeader->ulSourceIPAddress );
                                break;
                        #endif /* ( ipconfigUSE_IPv4 != 0 ) */

                        /* 默认情况永远不会触发，因为在上一步中 eReturn 不是 eProcessBuffer。 */
                        default:   /* LCOV_EXCL_LINE */
                            /* MISRA 16.4 合规性 */
                            break; /* LCOV_EXCL_LINE */
                    }
                }
            }

            if( eReturn != eWaitingResolution )
            {
                switch( ucProtocol )
                {
                    #if ( ipconfigUSE_IPv4 != 0 )
                        case ipPROTOCOL_ICMP:

                            /* IP 数据包包含 ICMP 帧。不必费心检查 ICMP 校验和，
                             * 因为如果它错了，返回的数据也会错，ping 的源头将知道出了问题，
                             * 因为它将无法验证其接收到的内容。 */
                            #if ( ipconfigREPLY_TO_INCOMING_PINGS == 1 ) || ( ipconfigSUPPORT_OUTGOING_PINGS == 1 )
                            {
                                eReturn = ProcessICMPPacket( pxNetworkBuffer );
                            }
                            #endif /* ( ipconfigREPLY_TO_INCOMING_PINGS == 1 ) || ( ipconfigSUPPORT_OUTGOING_PINGS == 1 ) */
                            break;
                    #endif /* ( ipconfigUSE_IPv4 != 0 ) */

                    #if ( ipconfigUSE_IPv6 != 0 )
                        case ipPROTOCOL_ICMP_IPv6:
                            eReturn = prvProcessICMPMessage_IPv6( pxNetworkBuffer );
                            break;
                    #endif /* ( ipconfigUSE_IPv6 != 0 ) */

                    case ipPROTOCOL_UDP:
                        /* IP 数据包包含 UDP 帧。 */

                        eReturn = prvProcessUDPPacket( pxNetworkBuffer );
                        break;

                        #if ipconfigUSE_TCP == 1
                            case ipPROTOCOL_TCP:

                                if( xProcessReceivedTCPPacket( pxNetworkBuffer ) == pdPASS )
                                {
                                    eReturn = eFrameConsumed;
                                }
                                break;
                        #endif /* if ipconfigUSE_TCP == 1 */
                    default:
                        /* 不支持的帧类型。 */
                        eReturn = eReleaseBuffer;
                        break;
                }
            }
        }
    }

    return eReturn;
}

/*-----------------------------------------------------------*/

/* 此函数在其他文件中使用，具有外部链接，例如在 FreeRTOS_DNS.c 中。不能设为静态。 */

/**
 * @brief 在检查某些条件后发送以太网帧。
 *
 * @param[in,out] pxNetworkBuffer 要发送的网络缓冲区。
 * @param[in] xReleaseAfterSend 是否在发送后释放此网络缓冲区。
 */
void vReturnEthernetFrame( NetworkBufferDescriptor_t * pxNetworkBuffer,
                           BaseType_t xReleaseAfterSend )
{
    #if ( ipconfigZERO_COPY_TX_DRIVER != 0 )
        NetworkBufferDescriptor_t * pxNewBuffer;
    #endif

    #if ( ipconfigETHERNET_MINIMUM_PACKET_BYTES > 0 )
    {
        if( pxNetworkBuffer->xDataLength < ( size_t ) ipconfigETHERNET_MINIMUM_PACKET_BYTES )
        {
            BaseType_t xIndex;

            FreeRTOS_printf( ( "vReturnEthernetFrame: 长度 %u\n", ( unsigned ) pxNetworkBuffer->xDataLength ) );

            for( xIndex = ( BaseType_t ) pxNetworkBuffer->xDataLength; xIndex < ( BaseType_t ) ipconfigETHERNET_MINIMUM_PACKET_BYTES; xIndex++ )
            {
                pxNetworkBuffer->pucEthernetBuffer[ xIndex ] = 0U;
            }

            pxNetworkBuffer->xDataLength = ( size_t ) ipconfigETHERNET_MINIMUM_PACKET_BYTES;
        }
    }
    #endif /* if( ipconfigETHERNET_MINIMUM_PACKET_BYTES > 0 ) */

    #if ( ipconfigZERO_COPY_TX_DRIVER != 0 )
        if( xReleaseAfterSend == pdFALSE )
        {
            pxNewBuffer = pxDuplicateNetworkBufferWithDescriptor( pxNetworkBuffer, pxNetworkBuffer->xDataLength );

            if( pxNewBuffer != NULL )
            {
                xReleaseAfterSend = pdTRUE;
                /* 不希望向上取整。 */
                pxNewBuffer->xDataLength = pxNetworkBuffer->xDataLength;
            }

            pxNetworkBuffer = pxNewBuffer;
        }

        if( pxNetworkBuffer != NULL )
    #endif /* if ( ipconfigZERO_COPY_TX_DRIVER != 0 ) */
    {
        /* MISRA 参考 11.3.1 [未对齐访问] */
        /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
        /* coverity[misra_c_2012_rule_11_3_violation] */
        IPPacket_t * pxIPPacket = ( ( IPPacket_t * ) pxNetworkBuffer->pucEthernetBuffer );
        /* memcpy() 辅助变量，用于符合 MISRA 规则 21.15 */
        const void * pvCopySource = NULL;
        void * pvCopyDest;

        #if ( ipconfigUSE_IPv4 != 0 )
            MACAddress_t xMACAddress;
            eResolutionLookupResult_t eResult;
            uint32_t ulDestinationIPAddress = 0U;
        #endif /* ( ipconfigUSE_IPv4 != 0 ) */

        /* 发送！ */
        if( pxNetworkBuffer->pxEndPoint == NULL )
        {
            /* _HT_ 我想知道这种临时的端点搜索是否有必要。 */
            FreeRTOS_printf( ( "vReturnEthernetFrame: %x ip 还没有 pxEndPoint？\n", ( unsigned int ) FreeRTOS_ntohl( pxIPPacket->xIPHeader.ulDestinationIPAddress ) ) );

            /* MISRA 参考 11.3.1 [未对齐访问] */
            /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
            /* coverity[misra_c_2012_rule_11_3_violation] */
            switch( ( ( ( EthernetHeader_t * ) pxNetworkBuffer->pucEthernetBuffer ) )->usFrameType )
            {
                #if ( ipconfigUSE_IPv6 != 0 )
                    case ipIPv6_FRAME_TYPE:
                        /* 未找到 IPv6 端点 */
                        break;
                #endif /* ( ipconfigUSE_IPv6 != 0 ) */

                #if ( ipconfigUSE_IPv4 != 0 )
                    case ipIPv4_FRAME_TYPE:
                        pxNetworkBuffer->pxEndPoint = FreeRTOS_FindEndPointOnNetMask( pxIPPacket->xIPHeader.ulDestinationIPAddress );
                        break;
                #endif /* ( ipconfigUSE_IPv4 != 0 ) */

                default:
                    /* MISRA 16.4 合规性 */
                    break;
            }
        }

        if( pxNetworkBuffer->pxEndPoint != NULL )
        {
            NetworkInterface_t * pxInterface = pxNetworkBuffer->pxEndPoint->pxNetworkInterface; /*_RB_ 为什么不直接使用 pxNetworkBuffer->pxNetworkInterface？ */

            /* 解析正在发送的以太网数据包。 */
            switch( pxIPPacket->xEthernetHeader.usFrameType )
            {
                #if ( ipconfigUSE_IPv4 != 0 )
                    case ipIPv4_FRAME_TYPE:
                        ulDestinationIPAddress = pxIPPacket->xIPHeader.ulDestinationIPAddress;

                        /* 尝试查找与目标 IP 地址对应的 MAC 地址。 */
                        eResult = eARPGetCacheEntry( &ulDestinationIPAddress, &xMACAddress, &( pxNetworkBuffer->pxEndPoint ) );

                        if( eResult == eResolutionCacheHit )
                        {
                            /* 最好的情况 - 找到了地址，使用它。 */
                            pvCopySource = &xMACAddress;
                        }
                        else
                        {
                            /* 如果未找到地址，只需交换源和目标 MAC 地址。 */
                            pvCopySource = &( pxIPPacket->xEthernetHeader.xSourceAddress );
                        }
                        break;
                #endif /* ( ipconfigUSE_IPv4 != 0 ) */

                case ipIPv6_FRAME_TYPE:
                case ipARP_FRAME_TYPE:
                default:
                    /* 只需交换源和目标 MAC 地址。 */
                    pvCopySource = &( pxIPPacket->xEthernetHeader.xSourceAddress );
                    break;
            }

            /*
             * 使用 memcpy() 的辅助变量以符合 MISRA 规则 21.15。这些应该会被优化掉。
             */
            pvCopyDest = &( pxIPPacket->xEthernetHeader.xDestinationAddress );
            ( void ) memcpy( pvCopyDest, pvCopySource, sizeof( pxIPPacket->xEthernetHeader.xDestinationAddress ) );

            pvCopySource = pxNetworkBuffer->pxEndPoint->xMACAddress.ucBytes;
            pvCopyDest = &( pxIPPacket->xEthernetHeader.xSourceAddress );
            ( void ) memcpy( pvCopyDest, pvCopySource, ( size_t ) ipMAC_ADDRESS_LENGTH_BYTES );

            /* 发送！ */
            if( xIsCallingFromIPTask() == pdTRUE )
            {
                iptraceNETWORK_INTERFACE_OUTPUT( pxNetworkBuffer->xDataLength, pxNetworkBuffer->pucEthernetBuffer );
                ( void ) pxInterface->pfOutput( pxInterface, pxNetworkBuffer, xReleaseAfterSend );
            }
            else if( xReleaseAfterSend != pdFALSE )
            {
                IPStackEvent_t xSendEvent;

                /* 向 IP 任务发送消息以发送此数据包。 */
                xSendEvent.eEventType = eNetworkTxEvent;
                xSendEvent.pvData = pxNetworkBuffer;

                if( xSendEventStructToIPTask( &xSendEvent, ( TickType_t ) portMAX_DELAY ) == pdFAIL )
                {
                    /* 发送消息失败，因此释放网络缓冲区。 */
                    vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );
                }
            }
            else
            {
                /* 这永远不应到达，或者数据包已经丢失。 */
                configASSERT( pdFALSE );
            }
        }
    }
}
/*-----------------------------------------------------------*/

#if ( ipconfigUSE_IPv4 != 0 )

/**
 * @brief 返回网卡 (NIC) 的 IP 地址。
 *
 * @return 网卡的 IP 地址。
 */
    uint32_t FreeRTOS_GetIPAddress( void )
    {
        NetworkEndPoint_t * pxEndPoint;
        uint32_t ulIPAddress;

        pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        #if ( ipconfigUSE_IPv6 != 0 )
            if( ENDPOINT_IS_IPv6( pxEndPoint ) )
            {
                for( ;
                     pxEndPoint != NULL;
                     pxEndPoint = FreeRTOS_NextEndPoint( NULL, pxEndPoint ) )
                {
                    /* 如果端点是 IPv4 则跳出。 */
                    if( pxEndPoint->bits.bIPv6 == 0U )
                    {
                        break;
                    }
                }
            }
        #endif /* ( ipconfigUSE_IPv6 != 0 ) */

        /* 返回网卡的 IP 地址。 */
        if( pxEndPoint == NULL )
        {
            ulIPAddress = 0U;
        }
        else if( pxEndPoint->ipv4_settings.ulIPAddress != 0U )
        {
            ulIPAddress = pxEndPoint->ipv4_settings.ulIPAddress;
        }
        else
        {
            ulIPAddress = pxEndPoint->ipv4_defaults.ulIPAddress;
        }

        return ulIPAddress;
    }
/*-----------------------------------------------------------*/

#endif /* #if ( ipconfigUSE_IPv4 != 0 ) */

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 ) && ( ipconfigUSE_IPv4 != 0 )

/*
 * 下面的辅助函数假设只有一个接口和一个端点 (ipconfigIPv4_BACKWARD_COMPATIBLE)
 */

/**
 * @brief 设置网卡的 IP 地址。
 *
 * @param[in] ulIPAddress 要设置的网卡的 IP 地址。
 */
    void FreeRTOS_SetIPAddress( uint32_t ulIPAddress )
    {
        /* 设置网卡的 IP 地址。 */
        NetworkEndPoint_t * pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        if( pxEndPoint != NULL )
        {
            pxEndPoint->ipv4_settings.ulIPAddress = ulIPAddress;
        }
    }
/*-----------------------------------------------------------*/

/**
 * @brief 获取子网的网关地址。
 *
 * @return 网关的 IP 地址，如果未使用/定义网关则为零。
 */
    uint32_t FreeRTOS_GetGatewayAddress( void )
    {
        uint32_t ulIPAddress = 0U;
        NetworkEndPoint_t * pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        if( pxEndPoint != NULL )
        {
            ulIPAddress = pxEndPoint->ipv4_settings.ulGatewayAddress;
        }

        return ulIPAddress;
    }
/*-----------------------------------------------------------*/

/**
 * @brief 获取 DNS 服务器地址。
 *
 * @return DNS 服务器的 IP 地址。
 */
    uint32_t FreeRTOS_GetDNSServerAddress( void )
    {
        uint32_t ulIPAddress = 0U;
        NetworkEndPoint_t * pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        if( pxEndPoint != NULL )
        {
            ulIPAddress = pxEndPoint->ipv4_settings.ulDNSServerAddresses[ 0 ];
        }

        return ulIPAddress;
    }
/*-----------------------------------------------------------*/

/**
 * @brief 获取子网的子网掩码。
 *
 * @return 子网的 32 位子网掩码。
 */
    uint32_t FreeRTOS_GetNetmask( void )
    {
        uint32_t ulIPAddress = 0U;
        NetworkEndPoint_t * pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        if( pxEndPoint != NULL )
        {
            ulIPAddress = pxEndPoint->ipv4_settings.ulNetMask;
        }

        return ulIPAddress;
    }
/*-----------------------------------------------------------*/

/**
 * @brief 更新 MAC 地址。
 *
 * @param[in] ucMACAddress 要设置的 MAC 地址。
 */
    void FreeRTOS_UpdateMACAddress( const uint8_t ucMACAddress[ ipMAC_ADDRESS_LENGTH_BYTES ] )
    {
        NetworkEndPoint_t * pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        if( pxEndPoint != NULL )
        {
            /* 在默认数据包头片段的开头复制 MAC 地址。 */
            ( void ) memcpy( pxEndPoint->xMACAddress.ucBytes, ( const void * ) ucMACAddress, ( size_t ) ipMAC_ADDRESS_LENGTH_BYTES );
        }
    }
/*-----------------------------------------------------------*/

/**
 * @brief 获取 MAC 地址。
 *
 * @return 指向 MAC 地址的指针。
 */
    const uint8_t * FreeRTOS_GetMACAddress( void )
    {
        const uint8_t * pucReturn = NULL;
        NetworkEndPoint_t * pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        if( pxEndPoint != NULL )
        {
            /* 在默认数据包头片段的开头复制 MAC 地址。 */
            pucReturn = pxEndPoint->xMACAddress.ucBytes;
        }

        return pucReturn;
    }
/*-----------------------------------------------------------*/

/**
 * @brief 设置子网的子网掩码。
 *
 * @param[in] ulNetmask 子网的 32 位子网掩码。
 */
    void FreeRTOS_SetNetmask( uint32_t ulNetmask )
    {
        NetworkEndPoint_t * pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        if( pxEndPoint != NULL )
        {
            pxEndPoint->ipv4_settings.ulNetMask = ulNetmask;
        }
    }
/*-----------------------------------------------------------*/

/**
 * @brief 设置网关地址。
 *
 * @param[in] ulGatewayAddress 网关地址。
 */
    void FreeRTOS_SetGatewayAddress( uint32_t ulGatewayAddress )
    {
        NetworkEndPoint_t * pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

        if( pxEndPoint != NULL )
        {
            pxEndPoint->ipv4_settings.ulGatewayAddress = ulGatewayAddress;
        }
    }
/*-----------------------------------------------------------*/
#endif /* if ( ipconfigIPv4_BACKWARD_COMPATIBLE == 1 )  && ( ipconfigUSE_IPv4 != 0 ) */

/**
 * @brief 返回 IP 任务是否就绪。
 *
 * @return 如果 IP 任务就绪则返回 pdTRUE，否则返回 pdFALSE。
 */
BaseType_t xIPIsNetworkTaskReady( void )
{
    return xIPTaskInitialised;
}
/*-----------------------------------------------------------*/

/**
 * @brief 返回是否所有端点都已启用。
 *
 * @return 如果所有定义的端点都已启用则返回 pdTRUE。
 */
BaseType_t FreeRTOS_IsNetworkUp( void )
{
    /* IsNetworkUp() 保留是为了向后兼容。 */
    return FreeRTOS_IsEndPointUp( NULL );
}
/*-----------------------------------------------------------*/

/**
 * @brief 变量 'xNetworkDownEventPending' 被声明为静态。此函数提供对其的只读访问。
 *
 * @return 如果有挂起的网络断开事件则返回 pdTRUE。否则返回 pdFALSE。
 */
BaseType_t xIsNetworkDownEventPending( void )
{
    return xNetworkDownEventPending;
}
/*-----------------------------------------------------------*/

/**
 * @brief 返回特定端点是否已启用。
 *
 * @return 如果特定端点已启用则返回 pdTRUE。
 */
BaseType_t FreeRTOS_IsEndPointUp( const struct xNetworkEndPoint * pxEndPoint )
{
    BaseType_t xReturn;

    if( pxEndPoint != NULL )
    {
        /* 这个特定的端点是否已启用？ */
        xReturn = ( BaseType_t ) pxEndPoint->bits.bEndPointUp;
    }
    else
    {
        /* 是否所有端点都已启用？ */
        xReturn = FreeRTOS_AllEndPointsUp( NULL );
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 如果属于给定接口的所有端点都已启用，则返回 pdTRUE。当 pxInterface 为空时，将检查所有端点。
 *
 * @param[in] pxInterface 感兴趣的网络接口，或 NULL 以检查所有端点。
 *
 * @return 如果所有端点都已启用则返回 pdTRUE，否则返回 pdFALSE；
 */
BaseType_t FreeRTOS_AllEndPointsUp( const struct xNetworkInterface * pxInterface )
{
    BaseType_t xResult = pdTRUE;
    const NetworkEndPoint_t * pxEndPoint = pxNetworkEndPoints;

    while( pxEndPoint != NULL )
    {
        if( ( pxInterface == NULL ) ||
            ( pxEndPoint->pxNetworkInterface == pxInterface ) )

        {
            if( pxEndPoint->bits.bEndPointUp == pdFALSE_UNSIGNED )
            {
                xResult = pdFALSE;
                break;
            }
        }

        pxEndPoint = pxEndPoint->pxNext;
    }

    return xResult;
}
/*-----------------------------------------------------------*/

#if ( ipconfigCHECK_IP_QUEUE_SPACE != 0 )

/**
 * @brief 获取 IP 任务队列中的最小空间。
 *
 * @return IP 任务队列中可能的最小空间。
 */
    UBaseType_t uxGetMinimumIPQueueSpace( void )
    {
        return uxQueueMinimumSpace;
    }
#endif
/*-----------------------------------------------------------*/

/**
 * @brief 通过检查网络缓冲区的类型获取 IP 头的大小。
 * @param[in] pxNetworkBuffer 网络缓冲区。
 * @return 相应 IP 头的大小。
 */
size_t uxIPHeaderSizePacket( const NetworkBufferDescriptor_t * pxNetworkBuffer )
{
    size_t uxResult;
    /* 将缓冲区映射到以太网头结构体，以便轻松访问字段。 */
    /* MISRA 参考 11.3.1 [未对齐访问] */
    /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
    /* coverity[misra_c_2012_rule_11_3_violation] */
    const EthernetHeader_t * pxHeader = ( ( const EthernetHeader_t * ) pxNetworkBuffer->pucEthernetBuffer );

    if( pxHeader->usFrameType == ( uint16_t ) ipIPv6_FRAME_TYPE )
    {
        uxResult = ipSIZE_OF_IPv6_HEADER;
    }
    else
    {
        uxResult = ipSIZE_OF_IPv4_HEADER;
    }

    return uxResult;
}
/*-----------------------------------------------------------*/

/**
 * @brief 通过检查套接字 bIsIPv6 是否设置来获取 IP 头的大小。
 * @param[in] pxSocket 套接字。
 * @return 相应 IP 头的大小。
 */
size_t uxIPHeaderSizeSocket( const FreeRTOS_Socket_t * pxSocket )
{
    size_t uxResult;

    if( ( pxSocket != NULL ) && ( pxSocket->bits.bIsIPv6 != pdFALSE_UNSIGNED ) )
    {
        uxResult = ipSIZE_OF_IPv6_HEADER;
    }
    else
    {
        uxResult = ipSIZE_OF_IPv4_HEADER;
    }

    return uxResult;
}
/*-----------------------------------------------------------*/

/* 提供对私有成员的访问以进行验证。 */
#ifdef FREERTOS_TCP_ENABLE_VERIFICATION
    #include "aws_freertos_ip_verification_access_ip_define.h"
#endif

