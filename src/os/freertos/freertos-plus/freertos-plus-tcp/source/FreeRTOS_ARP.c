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
 * @file FreeRTOS_ARP.c
 * @brief 实现 FreeRTOS+TCP 网络协议栈的地址解析协议 (ARP)。
 */

/* 标准库头文件包含。 */
#include <stdint.h>
#include <stdio.h>

/* FreeRTOS 内核头文件包含。 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP 协议栈头文件包含。 */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_IP_Timers.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_ARP.h"
#include "FreeRTOS_UDP_IP.h"
#include "FreeRTOS_DHCP.h"
#if ( ipconfigUSE_LLMNR == 1 )
    #include "FreeRTOS_DNS.h"
#endif /* ipconfigUSE_LLMNR */
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"
#include "FreeRTOS_Routing.h"

#if ( ipconfigUSE_IPv4 != 0 )

/** @brief 当 ARP 表中条目的寿命达到此值时（它递减到零，所以这是一个老条目），
 *         将发送一个 ARP 请求，以查看该条目是否仍然有效，从而可以刷新。 */
    #define arpMAX_ARP_AGE_BEFORE_NEW_ARP_REQUEST    ( 3U )

/** @brief 免费ARP之间的时间间隔。 */
    #ifndef arpGRATUITOUS_ARP_PERIOD
        #define arpGRATUITOUS_ARP_PERIOD    ( pdMS_TO_TICKS( 20000U ) )
    #endif

/** @brief 当有另一个设备与本设备具有相同的 IP 地址时，应发送防御性 ARP 请求。
 *         但是，根据 RFC 5227 第 1.1 节，连续的防御性 ARP 数据包之间必须有至少 10 秒的间隔。 */
    #ifndef arpIP_CLASH_RESET_TIMEOUT_MS
        #define arpIP_CLASH_RESET_TIMEOUT_MS    10000U
    #endif

/** @brief 在每个 arpIP_CLASH_RESET_TIMEOUT_MS 周期内，为 ARP 冲突发送的防御性 ARP 的最大数量。
 *         根据 RFC 5227 第 2.4 部分 b 的规定，重试次数限制为 1 次。*/
    #ifndef arpIP_CLASH_MAX_RETRIES
        #define arpIP_CLASH_MAX_RETRIES    1U
    #endif


    static void vARPProcessPacketRequest( ARPPacket_t * pxARPFrame,
                                          NetworkEndPoint_t * pxTargetEndPoint,
                                          uint32_t ulSenderProtocolAddress );

    static void vARPProcessPacketReply( const ARPPacket_t * pxARPFrame,
                                        NetworkEndPoint_t * pxTargetEndPoint,
                                        uint32_t ulSenderProtocolAddress );

/*
 * 根据 IP 地址在 ARP 缓存中查找 MAC 地址。
 */
    static eResolutionLookupResult_t prvCacheLookup( uint32_t ulAddressToLookup,
                                                     MACAddress_t * const pxMACAddress,
                                                     NetworkEndPoint_t ** ppxEndPoint );

    static eResolutionLookupResult_t eARPGetCacheEntryGateWay( uint32_t * pulIPAddress,
                                                               MACAddress_t * const pxMACAddress,
                                                               struct xNetworkEndPoint ** ppxEndPoint );

    static BaseType_t prvFindCacheEntry( const MACAddress_t * pxMACAddress,
                                         const uint32_t ulIPAddress,
                                         struct xNetworkEndPoint * pxEndPoint,
                                         CacheLocation_t * pxLocation );

/*-----------------------------------------------------------*/

/** @brief ARP 缓存表。 */
    _static ARPCacheRow_t xARPCache[ ipconfigARP_CACHE_ENTRIES ];


/*
 * IP 冲突检测目前仅在内部使用。当 DHCP 不响应时，驱动程序可以尝试随机的链路层 IP 地址 (169.254.x.x)。
 * 它将发送一个免费 ARP 消息，并在一段时间后检查下面的变量：
 */
    #if ( ipconfigARP_USE_CLASH_DETECTION != 0 )
        /* 如果另一个设备响应了免费 ARP 消息，则此变量变为非零。 */
        BaseType_t xARPHadIPClash;
        /* 具有相同 IP 地址的另一个设备的 MAC 地址。 */
        MACAddress_t xARPClashMacAddress;
    #endif /* ipconfigARP_USE_CLASH_DETECTION */

/*-----------------------------------------------------------*/

/** @brief  上次发送免费 ARP 的时间。免费 ARP 用于确保 ARP 表是最新的并检测 IP 地址冲突。 */
    static TickType_t xLastGratuitousARPTime = 0U;

/**
 * @brief 处理 ARP 数据包。
 *
 * @param[in] pxNetworkBuffer 包含待处理数据包的网络缓冲区。
 *
 * @return 一个枚举值，指示是返回该帧还是释放它。
 */
    eFrameProcessingResult_t eARPProcessPacket( const NetworkBufferDescriptor_t * pxNetworkBuffer )
    {
        /* MISRA 参考 11.3.1 [未对齐访问] */
        /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
        /* coverity[misra_c_2012_rule_11_3_violation] */
        ARPPacket_t * pxARPFrame = ( ( ARPPacket_t * ) pxNetworkBuffer->pucEthernetBuffer );
        eFrameProcessingResult_t eReturn = eReleaseBuffer;
        const ARPHeader_t * pxARPHeader;
        uint32_t ulTargetProtocolAddress, ulSenderProtocolAddress;

        /* 用于符合 MISRA 规则 21.15 的 memcpy() 辅助变量 */
        const void * pvCopySource;
        void * pvCopyDest;
        NetworkEndPoint_t * pxTargetEndPoint = pxNetworkBuffer->pxEndPoint;

        /* 下一个防御性请求在 arpIP_CLASH_RESET_TIMEOUT_MS 期间内不得发送。 */
        static TickType_t uxARPClashTimeoutPeriod = pdMS_TO_TICKS( arpIP_CLASH_RESET_TIMEOUT_MS );

        /* 此局部变量用于跟踪发送的 ARP 请求数量，并将每个 arpIP_CLASH_RESET_TIMEOUT_MS 
         * 周期的请求限制为 arpIP_CLASH_MAX_RETRIES 次。 */
        static UBaseType_t uxARPClashCounter = 0U;
        /* 上次发送 ARP 冲突请求的时间。 */
        static TimeOut_t xARPClashTimeOut;

        pxARPHeader = &( pxARPFrame->xARPHeader );

        /* 仅支持以太网硬件类型。
         * ARP 包中只能存在 IPv4 地址。
         * 硬件长度（MAC 地址）必须为 6 字节。并且
         * 协议地址长度必须为 4 字节，因为它是 IPv4。 */
        if( ( pxARPHeader->usHardwareType == ipARP_HARDWARE_TYPE_ETHERNET ) &&
            ( pxARPHeader->usProtocolType == ipARP_PROTOCOL_TYPE ) &&
            ( pxARPHeader->ucHardwareAddressLength == ipMAC_ADDRESS_LENGTH_BYTES ) &&
            ( pxARPHeader->ucProtocolAddressLength == ipIP_ADDRESS_LENGTH_BYTES ) )
        {
            /* ucSenderProtocolAddress 字段对齐不良，需逐字节复制。 */

            /*
             * 使用辅助变量进行 memcpy() 以符合 MISRA 规则 21.15。
             * 这些应该会被优化掉。
             */
            pvCopySource = pxARPHeader->ucSenderProtocolAddress;
            pvCopyDest = &ulSenderProtocolAddress;
            ( void ) memcpy( pvCopyDest, pvCopySource, sizeof( ulSenderProtocolAddress ) );
            /* ulTargetProtocolAddress 字段对齐良好，可直接进行 32 位复制。 */
            ulTargetProtocolAddress = pxARPHeader->ulTargetProtocolAddress;

            if( uxARPClashCounter != 0U )
            {
                /* 是否已达到超时时间？ */
                if( xTaskCheckForTimeOut( &xARPClashTimeOut, &uxARPClashTimeoutPeriod ) == pdTRUE )
                {
                    /* 已等待足够长的时间，重置计数器。 */
                    uxARPClashCounter = 0;
                }
            }

            /* 检查最高字节的最低位是否为 1，以判断是否为多播地址或广播地址 (FF:FF:FF:FF:FF:FF)。 */
            if( ( pxARPHeader->xSenderHardwareAddress.ucBytes[ 0 ] & 0x01U ) == 0x01U )
            {
                /* 发送方地址是多播或广播地址，这对于 ARP 包是不允许的。丢弃该包。参见 RFC 1812 第 3.3.2 节。 */
                iptraceDROPPED_INVALID_ARP_PACKET( pxARPHeader );
            }
            else if( ( ipFIRST_LOOPBACK_IPv4 <= ( FreeRTOS_ntohl( ulSenderProtocolAddress ) ) ) &&
                     ( ( FreeRTOS_ntohl( ulSenderProtocolAddress ) ) < ipLAST_LOOPBACK_IPv4 ) )
            {
                /* 本地环回地址绝不能出现在主机外部。参见 RFC 1122 第 3.2.1.3 节。 */
                iptraceDROPPED_INVALID_ARP_PACKET( pxARPHeader );
            }
            /* 检查此 IP 地址是否与另一台设备存在冲突。 */
            else if( ( pxTargetEndPoint != NULL ) && ( ulSenderProtocolAddress == pxTargetEndPoint->ipv4_settings.ulIPAddress ) )
            {
                if( uxARPClashCounter < arpIP_CLASH_MAX_RETRIES )
                {
                    /* 增加计数器。 */
                    uxARPClashCounter++;

                    /* 发送防御性 ARP 请求。 */
                    FreeRTOS_OutputARPRequest_Multi( pxTargetEndPoint, pxTargetEndPoint->ipv4_settings.ulIPAddress );

                    /* 由于刚刚发送了针对此 IP 的 ARP 请求，因此在 arpGRATUITOUS_ARP_PERIOD 期间不要发送免费 ARP。 */
                    xLastGratuitousARPTime = xTaskGetTickCount();

                    /* 记录发送此请求的时间。 */
                    vTaskSetTimeOutState( &xARPClashTimeOut );

                    /* 将超时期限重置为给定值。 */
                    uxARPClashTimeoutPeriod = pdMS_TO_TICKS( arpIP_CLASH_RESET_TIMEOUT_MS );
                }

                /* 处理接收到的 ARP 帧以查看是否存在冲突。 */
                #if ( ipconfigARP_USE_CLASH_DETECTION != 0 )
                {
                    NetworkEndPoint_t * pxSourceEndPoint = FreeRTOS_FindEndPointOnIP_IPv4( ulSenderProtocolAddress );

                    if( ( pxSourceEndPoint != NULL ) && ( pxSourceEndPoint->ipv4_settings.ulIPAddress == ulSenderProtocolAddress ) )
                    {
                        xARPHadIPClash = pdTRUE;
                        /* 记住具有相同 IP 地址的另一台设备的 MAC 地址。 */
                        ( void ) memcpy( xARPClashMacAddress.ucBytes, pxARPHeader->xSenderHardwareAddress.ucBytes, sizeof( xARPClashMacAddress.ucBytes ) );
                    }
                }
                #endif /* ipconfigARP_USE_CLASH_DETECTION */
            }
            else
            {
                iptraceARP_PACKET_RECEIVED();

                /* 仍在测试时的一些额外日志记录。 */
                #if ( ipconfigHAS_DEBUG_PRINTF != 0 )
                    if( pxARPHeader->usOperation == ( uint16_t ) ipARP_REPLY )
                    {
                        FreeRTOS_debug_printf( ( "ipARP_REPLY from %xip to %xip end-point %xip\n",
                                                 ( unsigned ) FreeRTOS_ntohl( ulSenderProtocolAddress ),
                                                 ( unsigned ) FreeRTOS_ntohl( ulTargetProtocolAddress ),
                                                 ( unsigned ) FreeRTOS_ntohl( ( pxTargetEndPoint != NULL ) ? pxTargetEndPoint->ipv4_settings.ulIPAddress : 0U ) ) );
                    }
                #endif /* ( ipconfigHAS_DEBUG_PRINTF != 0 ) */

                #if ( ipconfigHAS_DEBUG_PRINTF != 0 )
                    if( ( pxARPHeader->usOperation == ( uint16_t ) ipARP_REQUEST ) &&
                        ( ulSenderProtocolAddress != ulTargetProtocolAddress ) &&
                        ( pxTargetEndPoint != NULL ) )
                    {
                        FreeRTOS_debug_printf( ( "ipARP_REQUEST from %xip to %xip end-point %xip\n",
                                                 ( unsigned ) FreeRTOS_ntohl( ulSenderProtocolAddress ),
                                                 ( unsigned ) FreeRTOS_ntohl( ulTargetProtocolAddress ),
                                                 ( unsigned ) ( FreeRTOS_ntohl( ( pxTargetEndPoint != NULL ) ? pxTargetEndPoint->ipv4_settings.ulIPAddress : 0U ) ) ) );
                    }
                #endif /* ( ipconfigHAS_DEBUG_PRINTF != 0 ) */

                /* 除非启用日志记录，否则不会使用 ulTargetProtocolAddress。 */
                ( void ) ulTargetProtocolAddress;

                /* 如果本地 IP 地址为零，则不执行任何操作，因为这意味着 DHCP 请求尚未完成。 */
                if( ( pxTargetEndPoint != NULL ) && ( pxTargetEndPoint->bits.bEndPointUp != pdFALSE_UNSIGNED ) )
                {
                    switch( pxARPHeader->usOperation )
                    {
                        case ipARP_REQUEST:

                            if( ulTargetProtocolAddress == pxTargetEndPoint->ipv4_settings.ulIPAddress )
                            {
                                if( memcmp( pxTargetEndPoint->xMACAddress.ucBytes,
                                            pxARPHeader->xSenderHardwareAddress.ucBytes,
                                            ipMAC_ADDRESS_LENGTH_BYTES ) != 0 )
                                {
                                    vARPProcessPacketRequest( pxARPFrame, pxTargetEndPoint, ulSenderProtocolAddress );
                                    eReturn = eReturnEthernetFrame;
                                }
                            }
                            /* 检查它是否是免费 ARP 请求，并验证它是否属于同一子网掩码。 */
                            else if( ( ulSenderProtocolAddress == ulTargetProtocolAddress ) &&
                                     ( ( ulSenderProtocolAddress & pxTargetEndPoint->ipv4_settings.ulNetMask ) == ( pxTargetEndPoint->ipv4_settings.ulNetMask & pxTargetEndPoint->ipv4_settings.ulIPAddress ) ) )
                            {
                                const MACAddress_t xGARPTargetAddress = { { 0, 0, 0, 0, 0, 0 } };

                                /* 确保目标 MAC 地址是 ff:ff:ff:ff:ff:ff 或 00:00:00:00:00:00，
                                 * 并且发送方 MAC 地址与端点 MAC 地址不匹配。 */
                                if( ( ( memcmp( pxARPHeader->xTargetHardwareAddress.ucBytes, xBroadcastMACAddress.ucBytes, ipMAC_ADDRESS_LENGTH_BYTES ) == 0 ) ||
                                      ( ( memcmp( pxARPHeader->xTargetHardwareAddress.ucBytes, xGARPTargetAddress.ucBytes, ipMAC_ADDRESS_LENGTH_BYTES ) == 0 ) ) ) &&
                                    ( memcmp( pxTargetEndPoint->xMACAddress.ucBytes, pxARPHeader->xSenderHardwareAddress.ucBytes, ipMAC_ADDRESS_LENGTH_BYTES ) != 0 ) )
                                {
                                    MACAddress_t xHardwareAddress;
                                    NetworkEndPoint_t * pxCachedEndPoint;

                                    pxCachedEndPoint = NULL;

                                    /* 该请求是一个免费 ARP 消息。
                                     * 如果条目已存在，则刷新该条目。 */
                                    /* 确定请求的 IP 地址的 ARP 缓存状态。 */
                                    if( eARPGetCacheEntry( &( ulSenderProtocolAddress ), &( xHardwareAddress ), &( pxCachedEndPoint ) ) == eResolutionCacheHit )
                                    {
                                        /* 检查端点是否与 ARP 缓存中存在的端点匹配 */
                                        if( pxCachedEndPoint == pxTargetEndPoint )
                                        {
                                            vARPRefreshCacheEntry( &( pxARPHeader->xSenderHardwareAddress ), ulSenderProtocolAddress, pxTargetEndPoint );
                                        }
                                    }
                                }
                            }
                            else
                            {
                                /* 什么都不做，让 coverity 满意 */
                            }

                            break;

                        case ipARP_REPLY:
                            vARPProcessPacketReply( pxARPFrame, pxTargetEndPoint, ulSenderProtocolAddress );
                            break;

                        default:
                            /* 无效操作。 */
                            break;
                    }
                }
            }
        }
        else
        {
            iptraceDROPPED_INVALID_ARP_PACKET( pxARPHeader );
        }

        return eReturn;
    }
/*-----------------------------------------------------------*/

/**
 * @brief 处理 ARP 请求数据包。
 *
 * @param[in] pxARPFrame 完整的 ARP 帧。
 * @param[in] pxTargetEndPoint 处理对等地址的端点。
 * @param[in] ulSenderProtocolAddress 发送方的 IP 地址。
 *
 */
    static void vARPProcessPacketRequest( ARPPacket_t * pxARPFrame,
                                          NetworkEndPoint_t * pxTargetEndPoint,
                                          uint32_t ulSenderProtocolAddress )
    {
        ARPHeader_t * pxARPHeader = &( pxARPFrame->xARPHeader );
/* 用于符合 MISRA 规则 21.15 的 memcpy() 辅助变量 */
        const void * pvCopySource;
        void * pvCopyDest;


        /* 该数据包包含一个 ARP 请求。它是针对某个端点的 IP 地址的吗？ */
        /* 已确认 pxTargetEndPoint 不为 NULL。 */
        iptraceSENDING_ARP_REPLY( ulSenderProtocolAddress );

        /* 该请求是针对本节点地址的。将条目添加到 ARP 缓存中，
         * 或者如果条目已存在则刷新该条目。 */
        vARPRefreshCacheEntry( &( pxARPHeader->xSenderHardwareAddress ), ulSenderProtocolAddress, pxTargetEndPoint );

        /* 在同一缓冲区中生成回复负载。 */
        pxARPHeader->usOperation = ( uint16_t ) ipARP_REPLY;

        /* 这里无法检测到双重 IP 地址，这在处理 ARP 包路径中处理 */

        /*
         * 使用辅助变量进行 memcpy() 以符合 MISRA 规则 21.15。
         * 这些应该会被优化掉。
         */
        pvCopySource = pxARPHeader->xSenderHardwareAddress.ucBytes;
        pvCopyDest = pxARPHeader->xTargetHardwareAddress.ucBytes;
        ( void ) memcpy( pvCopyDest, pvCopySource, sizeof( MACAddress_t ) );
        pxARPHeader->ulTargetProtocolAddress = ulSenderProtocolAddress;

        /*
         * 使用辅助变量进行 memcpy() 以符合 MISRA 规则 21.15。
         * 这些应该会被优化掉。
         */
        pvCopySource = pxTargetEndPoint->xMACAddress.ucBytes;
        pvCopyDest = pxARPHeader->xSenderHardwareAddress.ucBytes;
        ( void ) memcpy( pvCopyDest, pvCopySource, sizeof( MACAddress_t ) );
        pvCopySource = &( pxTargetEndPoint->ipv4_settings.ulIPAddress );
        pvCopyDest = pxARPHeader->ucSenderProtocolAddress;
        ( void ) memcpy( pvCopyDest, pvCopySource, sizeof( pxARPHeader->ucSenderProtocolAddress ) );
    }
/*-----------------------------------------------------------*/

/**
 * @brief 某设备发送了 ARP 回复，处理它。
 * @param[in] pxARPFrame 接收到的 ARP 包。
 * @param[in] pxTargetEndPoint 接收该包的端点。
 * @param[in] ulSenderProtocolAddress 涉及的 IPv4 地址。
 */
    static void vARPProcessPacketReply( const ARPPacket_t * pxARPFrame,
                                        NetworkEndPoint_t * pxTargetEndPoint,
                                        uint32_t ulSenderProtocolAddress )
    {
        const ARPHeader_t * pxARPHeader = &( pxARPFrame->xARPHeader );
        uint32_t ulTargetProtocolAddress = pxARPHeader->ulTargetProtocolAddress;

        /* 如果数据包是发往本设备的，或者条目已经存在。 */
        if( ( ulTargetProtocolAddress == pxTargetEndPoint->ipv4_settings.ulIPAddress ) ||
            ( xIsIPInARPCache( ulSenderProtocolAddress ) == pdTRUE ) )
        {
            iptracePROCESSING_RECEIVED_ARP_REPLY( ulTargetProtocolAddress );
            vARPRefreshCacheEntry( &( pxARPHeader->xSenderHardwareAddress ), ulSenderProtocolAddress, pxTargetEndPoint );
        }

        if( ( pxARPWaitingNetworkBuffer != NULL ) &&
            ( uxIPHeaderSizePacket( pxARPWaitingNetworkBuffer ) == ipSIZE_OF_IPv4_HEADER ) )
        {
            /* MISRA 参考 11.3.1 [未对齐访问] */
/* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
            /* coverity[misra_c_2012_rule_11_3_violation] */
            const IPPacket_t * pxARPWaitingIPPacket = ( ( IPPacket_t * ) pxARPWaitingNetworkBuffer->pucEthernetBuffer );
            const IPHeader_t * pxARPWaitingIPHeader = &( pxARPWaitingIPPacket->xIPHeader );

            if( ulSenderProtocolAddress == pxARPWaitingIPHeader->ulSourceIPAddress )
            {
                IPStackEvent_t xEventMessage;
                const TickType_t xDontBlock = ( TickType_t ) 0;

                xEventMessage.eEventType = eNetworkRxEvent;
                xEventMessage.pvData = ( void * ) pxARPWaitingNetworkBuffer;

                if( xSendEventStructToIPTask( &xEventMessage, xDontBlock ) != pdPASS )
                {
                    /* 发送消息失败，因此释放网络缓冲区。 */
                    vReleaseNetworkBufferAndDescriptor( pxARPWaitingNetworkBuffer );
                }

                /* 清除缓冲区指针。 */
                pxARPWaitingNetworkBuffer = NULL;

                /* 找到了 ARP 解析，禁用 ARP 解析定时器。 */
                vIPSetARPResolutionTimerEnableState( pdFALSE );

                iptrace_DELAYED_ARP_REQUEST_REPLIED();
            }
        }
    }
/*-----------------------------------------------------------*/

/**
 * @brief 检查 IP 地址是否在 ARP 缓存中。
 *
 * @param[in] ulAddressToLookup 要检查的 IP 地址的 32 位表示形式。
 *
 * @return 找到 IP 地址时返回 pdTRUE，否则返回 pdFALSE。
 */
    BaseType_t xIsIPInARPCache( uint32_t ulAddressToLookup )
    {
        BaseType_t x, xReturn = pdFALSE;

        /* 遍历 ARP 缓存中的每个条目。 */
        for( x = 0; x < ipconfigARP_CACHE_ENTRIES; x++ )
        {
            /* ARP 缓存表中的这一行是否包含正在查询的 IP 地址的条目？ */
            if( xARPCache[ x ].ulIPAddress == ulAddressToLookup )
            {
                xReturn = pdTRUE;

                /* 找到了匹配的有效条目。 */
                if( xARPCache[ x ].ucValid == ( uint8_t ) pdFALSE )
                {
                    /* 此条目正在等待 ARP 回复，因此无效。 */
                    xReturn = pdFALSE;
                }

                break;
            }
        }

        return xReturn;
    }

/**
 * @brief 检查位于本地子网上的数据包是否需要 ARP 解析。如果需要，则发送 ARP 请求。
 *
 * @param[in] pxNetworkBuffer 包含待检查数据包的网络缓冲区。
 *
 * @return 如果数据包需要 ARP 解析则返回 pdTRUE，否则返回 pdFALSE。
 */
    BaseType_t xCheckRequiresARPResolution( const NetworkBufferDescriptor_t * pxNetworkBuffer )
    {
        BaseType_t xNeedsARPResolution = pdFALSE;

        /* MISRA 参考 11.3.1 [未对齐访问] */
        /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
        /* coverity[misra_c_2012_rule_11_3_violation] */
        const IPPacket_t * pxIPPacket = ( ( const IPPacket_t * ) pxNetworkBuffer->pucEthernetBuffer );
        const IPHeader_t * pxIPHeader = &( pxIPPacket->xIPHeader );
        const IPV4Parameters_t * pxIPv4Settings = &( pxNetworkBuffer->pxEndPoint->ipv4_settings );

        configASSERT( ( pxIPPacket->xEthernetHeader.usFrameType == ipIPv4_FRAME_TYPE ) || ( pxIPPacket->xEthernetHeader.usFrameType == ipARP_FRAME_TYPE ) );

        if( ( pxIPHeader->ulSourceIPAddress & pxIPv4Settings->ulNetMask ) == ( pxIPv4Settings->ulIPAddress & pxIPv4Settings->ulNetMask ) )
        {
            /* 如果 IP 在同一子网上，并且我们还没有 ARP 条目，
             * 那么我们应该发送 ARP 以查找 MAC 地址。 */
            if( xIsIPInARPCache( pxIPHeader->ulSourceIPAddress ) == pdFALSE )
            {
                FreeRTOS_OutputARPRequest_Multi( pxNetworkBuffer->pxEndPoint, pxIPHeader->ulSourceIPAddress );

                /* 此数据包需要解析，因为它位于同一子网但不在 ARP 缓存中。 */
                xNeedsARPResolution = pdTRUE;
            }
        }

        return xNeedsARPResolution;
    }

    #if ( ipconfigUSE_ARP_REMOVE_ENTRY != 0 )

/**
 * @brief 移除与 .pxMACAddress 匹配的 ARP 缓存条目。
 *
 * @param[in] pxMACAddress 指向要移除其条目的 MAC 地址的指针。
 * @return 找到并移除条目时返回 IP 地址，否则返回零。
 */
        uint32_t ulARPRemoveCacheEntryByMac( const MACAddress_t * pxMACAddress )
        {
            BaseType_t x;
            uint32_t lResult = 0;

            configASSERT( pxMACAddress != NULL );

            /* 遍历 ARP 缓存表中的每个条目。 */
            for( x = 0; x < ipconfigARP_CACHE_ENTRIES; x++ )
            {
                if( ( memcmp( xARPCache[ x ].xMACAddress.ucBytes, pxMACAddress->ucBytes, sizeof( pxMACAddress->ucBytes ) ) == 0 ) )
                {
                    lResult = xARPCache[ x ].ulIPAddress;
                    ( void ) memset( &xARPCache[ x ], 0, sizeof( xARPCache[ x ] ) );
                    break;
                }
            }

            return lResult;
        }

    #endif /* ipconfigUSE_ARP_REMOVE_ENTRY != 0 */
/*-----------------------------------------------------------*/

/**
 * @brief 在 ARP 缓存中查找 IP-MAC 对并重置 'age' 字段。如果未找到匹配项，则不采取任何操作。
 *
 * @param[in] pxMACAddress 指向需要更新的条目的 MAC 地址的指针。
 * @param[in] ulIPAddress 对应条目需要更新的 IP 地址。
 */
    void vARPRefreshCacheEntryAge( const MACAddress_t * pxMACAddress,
                                   const uint32_t ulIPAddress )
    {
        BaseType_t x;

        if( pxMACAddress != NULL )
        {
            /* 遍历 ARP 缓存中的每个条目。 */
            for( x = 0; x < ipconfigARP_CACHE_ENTRIES; x++ )
            {
                /* 缓存表中的这一行是否包含正在查询的 IP 地址的条目？ */
                if( xARPCache[ x ].ulIPAddress == ulIPAddress )
                {
                    /* 此缓存条目是否具有相同的 MAC 地址？ */
                    if( memcmp( xARPCache[ x ].xMACAddress.ucBytes, pxMACAddress->ucBytes, sizeof( pxMACAddress->ucBytes ) ) == 0 )
                    {
                        /* IP 地址和 MAC 匹配，更新此条目的寿命。 */
                        xARPCache[ x ].ucAge = ( uint8_t ) ipconfigMAX_ARP_AGE;
                        break;
                    }
                }
            }
        }
    }
/*-----------------------------------------------------------*/

/**
 * @brief 添加/更新 ARP 缓存条目中 MAC 地址到 IP 地址的映射。
 *
 * @param[in] pxMACAddress 指向正在更新映射的 MAC 地址的指针。
 * @param[in] ulIPAddress 正在更新映射的 IP 地址的 32 位表示形式。
 * @param[in] pxEndPoint 存储在表中的端点。
 */
    void vARPRefreshCacheEntry( const MACAddress_t * pxMACAddress,
                                const uint32_t ulIPAddress,
                                struct xNetworkEndPoint * pxEndPoint )
    {
        #if ( ipconfigARP_STORES_REMOTE_ADDRESSES == 0 )
            /* 仅当 IP 地址在本地网络上时才处理。 */
            BaseType_t xAddressIsLocal = ( FreeRTOS_FindEndPointOnNetMask( ulIPAddress ) != NULL ) ? 1 : 0; /* ARP 远程地址。 */

            /* 仅当 IP 地址与某个端点匹配时才处理。 */
            if( xAddressIsLocal != 0 )
        #else

            /* 如果 ipconfigARP_STORES_REMOTE_ADDRESSES 不为零，具有不同子网掩码的 IP 地址也将被存储。
             * 当回复来自不同子网掩码的 UDP 消息时，可以查找 IP 地址并发送回复。
             * 此选项对于具有多个网关的系统很有用，回复肯定会到达。
             * 如果 ipconfigARP_STORES_REMOTE_ADDRESSES 为零，则网关地址是唯一的选择。 */

            if( pdTRUE )
        #endif
        {
            CacheLocation_t xLocation;
            BaseType_t xReady;

            xReady = prvFindCacheEntry( pxMACAddress, ulIPAddress, pxEndPoint, &( xLocation ) );

            if( xReady == pdFALSE )
            {
                if( xLocation.xMacEntry >= 0 )
                {
                    xLocation.xUseEntry = xLocation.xMacEntry;

                    if( xLocation.xIpEntry >= 0 )
                    {
                        /* 在不同位置找到了 MAC 地址和 IP 地址：清除与 IP 地址匹配的条目 */
                        ( void ) memset( &( xARPCache[ xLocation.xIpEntry ] ), 0, sizeof( ARPCacheRow_t ) );
                    }
                }
                else if( xLocation.xIpEntry >= 0 )
                {
                    /* 找到了包含 IP 地址的条目，但其 MAC 地址不同 */
                    xLocation.xUseEntry = xLocation.xIpEntry;
                }
                else
                {
                    /* 未找到匹配条目。 */
                }

                /* 如果未找到该条目，我们使用最老的条目并设置 IP 地址 */
                xARPCache[ xLocation.xUseEntry ].ulIPAddress = ulIPAddress;

                if( pxMACAddress != NULL )
                {
                    ( void ) memcpy( xARPCache[ xLocation.xUseEntry ].xMACAddress.ucBytes, pxMACAddress->ucBytes, sizeof( pxMACAddress->ucBytes ) );

                    iptraceARP_TABLE_ENTRY_CREATED( ulIPAddress, ( *pxMACAddress ) );
                    /* 并且此条目不需要立即关注 */
                    xARPCache[ xLocation.xUseEntry ].ucAge = ( uint8_t ) ipconfigMAX_ARP_AGE;
                    xARPCache[ xLocation.xUseEntry ].ucValid = ( uint8_t ) pdTRUE;
                    xARPCache[ xLocation.xUseEntry ].pxEndPoint = pxEndPoint;
                }
                else if( xLocation.xIpEntry < 0 )
                {
                    xARPCache[ xLocation.xUseEntry ].ucAge = ( uint8_t ) ipconfigMAX_ARP_RETRANSMISSIONS;
                    xARPCache[ xLocation.xUseEntry ].ucValid = ( uint8_t ) pdFALSE;
                }
                else
                {
                    /* 不会存储任何内容。 */
                }
            }
        }
    }
/*-----------------------------------------------------------*/

/**
 * @brief ARP 查找的结果应存储在 ARP 缓存中。此辅助函数查找位置。
 * @param[in] pxMACAddress 属于该 IP 地址的 MAC 地址。
 * @param[in] ulIPAddress 条目的 IP 地址。
 * @param[in] pxEndPoint 将存储在表中的端点。
 * @param[out] pxLocation 此搜索的结果将写入此结构体。
 */
    static BaseType_t prvFindCacheEntry( const MACAddress_t * pxMACAddress,
                                         const uint32_t ulIPAddress,
                                         struct xNetworkEndPoint * pxEndPoint,
                                         CacheLocation_t * pxLocation )
    {
        BaseType_t x = 0;
        uint8_t ucMinAgeFound = 0U;
        BaseType_t xReturn = pdFALSE;

        #if ( ipconfigARP_STORES_REMOTE_ADDRESSES != 0 )
            BaseType_t xAddressIsLocal = ( FreeRTOS_FindEndPointOnNetMask( ulIPAddress ) != NULL ) ? 1 : 0; /* ARP 远程地址。 */
        #endif

        /* 从最大可能数开始。 */
        ucMinAgeFound--;

        pxLocation->xIpEntry = -1;
        pxLocation->xMacEntry = -1;
        pxLocation->xUseEntry = 0;

        /* 遍历 ARP 缓存表中的每个条目。 */
        for( x = 0; x < ipconfigARP_CACHE_ENTRIES; x++ )
        {
            BaseType_t xMatchingMAC = pdFALSE;

            if( pxMACAddress != NULL )
            {
                if( memcmp( xARPCache[ x ].xMACAddress.ucBytes, pxMACAddress->ucBytes, sizeof( pxMACAddress->ucBytes ) ) == 0 )
                {
                    xMatchingMAC = pdTRUE;
                }
            }

            /* 缓存表中的这一行是否包含正在查询的 IP 地址的条目？ */
            if( xARPCache[ x ].ulIPAddress == ulIPAddress )
            {
                if( pxMACAddress == NULL )
                {
                    /* 如果参数 pxMACAddress 为 NULL，将保留一个条目以指示存在未完成的 ARP 请求，
                     * 此条目将具有 "ucValid == pdFALSE"。 */
                    pxLocation->xIpEntry = x;
                    break;
                }

                /* 查看 MAC 地址是否也匹配。 */
                if( xMatchingMAC != pdFALSE )
                {
                    /* 对于每个接收到的数据包都会调用此函数，这是目前最常见的路径。 */
                    xARPCache[ x ].ucAge = ( uint8_t ) ipconfigMAX_ARP_AGE;
                    xARPCache[ x ].ucValid = ( uint8_t ) pdTRUE;
                    xARPCache[ x ].pxEndPoint = pxEndPoint;
                    /* 向调用者指示条目已更新。 */
                    xReturn = pdTRUE;
                    break;
                }

                /* 找到了包含 ulIPAddress 的条目，但 MAC 地址不匹配。
                 * 可能是一个 ucValid=pdFALSE 的条目，正在等待 ARP 回复。
                 * 仍然想看看是否与给定的 MAC 地址匹配。如果找到，
                 * 必须清除这两个条目中的一个。 */
                pxLocation->xIpEntry = x;
            }
            else if( xMatchingMAC != pdFALSE )
            {
                /* 找到了具有给定 MAC 地址的条目，但 IP 地址不同。
                 * 继续循环以查找可能与 ulIPAddress 的匹配项。 */
                #if ( ipconfigARP_STORES_REMOTE_ADDRESSES != 0 )
                {
                    /* 如果 ARP 存储网络外 IP 地址的 MAC 地址，则不应覆盖网关的 MAC 地址。 */
                    BaseType_t xOtherIsLocal = ( FreeRTOS_FindEndPointOnNetMask( xARPCache[ x ].ulIPAddress ) != NULL ) ? 1 : 0; /* ARP 远程地址。 */

                    if( xAddressIsLocal == xOtherIsLocal )
                    {
                        pxLocation->xMacEntry = x;
                    }
                }
                #else /* if ( ipconfigARP_STORES_REMOTE_ADDRESSES != 0 ) */
                {
                    pxLocation->xMacEntry = x;
                }
                #endif /* if ( ipconfigARP_STORES_REMOTE_ADDRESSES != 0 ) */
            }

            /* _HT_
             * 我们难道不应该在这里测试 xARPCache[ x ].ucValid == pdFALSE 吗？ */
            else if( xARPCache[ x ].ucAge < ucMinAgeFound )
            {
                /* 在遍历表时，记住包含最老条目（年龄计数最低，因为年龄递减至零）的表行，
                 * 以便如果此函数需要添加一个不存在的条目时，可以重用该行。 */
                ucMinAgeFound = xARPCache[ x ].ucAge;
                pxLocation->xUseEntry = x;
            }
            else
            {
                /* 此缓存条目暂时没有任何操作。 */
            }
        } /* for( x = 0; x < ipconfigARP_CACHE_ENTRIES; x++ ) */

        return xReturn;
    }
/*-----------------------------------------------------------*/

    #if ( ipconfigUSE_ARP_REVERSED_LOOKUP == 1 )

/**
 * @brief 从缓存表中检索条目
 *
 * @param[in] pxMACAddress 目标条目的 MAC 地址。
 * @param[out] pulIPAddress 设置为找到的 IP 地址，如果未找到则保持不变。
 *
 * @return eResolutionCacheMiss 或 eResolutionCacheHit。
 */
        eResolutionLookupResult_t eARPGetCacheEntryByMac( const MACAddress_t * const pxMACAddress,
                                                          uint32_t * pulIPAddress,
                                                          struct xNetworkInterface ** ppxInterface )
        {
            BaseType_t x;
            eResolutionLookupResult_t eReturn = eResolutionCacheMiss;

            configASSERT( pxMACAddress != NULL );
            configASSERT( pulIPAddress != NULL );

            if( ppxInterface != NULL )
            {
                *( ppxInterface ) = NULL;
            }

            /* 遍历 ARP 缓存中的每个条目。 */
            for( x = 0; x < ipconfigARP_CACHE_ENTRIES; x++ )
            {
                /* ARP 缓存表中的这一行是否包含正在搜索的 MAC 地址的条目？ */
                if( memcmp( pxMACAddress->ucBytes, xARPCache[ x ].xMACAddress.ucBytes, sizeof( MACAddress_t ) ) == 0 )
                {
                    *pulIPAddress = xARPCache[ x ].ulIPAddress;

                    if( ( ppxInterface != NULL ) &&
                        ( xARPCache[ x ].pxEndPoint != NULL ) )
                    {
                        *( ppxInterface ) = xARPCache[ x ].pxEndPoint->pxNetworkInterface;
                    }

                    eReturn = eResolutionCacheHit;
                    break;
                }
            }

            return eReturn;
        }
    #endif /* ipconfigUSE_ARP_REVERSED_LOOKUP */

/*-----------------------------------------------------------*/

/**
 * @brief 在 ARP 缓存中查找 ulIPAddress。
 *
 * @param[in,out] pulIPAddress 指向要查询 ARP 缓存的 IP 地址的指针。
 * @param[in,out] pxMACAddress 指向 MACAddress_t 变量的指针，如果找到，MAC 地址将存储在此处。
 * @param[out] ppxEndPoint 将存储网关端点的指针。
 *
 * @return 如果 IP 地址存在，则将关联的 MAC 地址复制到 pxMACAddress 中，刷新 ARP 缓存条目的寿命，
 *         并返回 eResolutionCacheHit。如果 ARP 缓存中不存在该 IP 地址，则返回 eResolutionCacheMiss。
 *         如果由于任何原因无法发送数据包（可能是 DHCP 仍在处理中，或者寻址需要网关但没有定义网关），
 *         则返回 eResolutionFailed。
 */
    eResolutionLookupResult_t eARPGetCacheEntry( uint32_t * pulIPAddress,
                                                 MACAddress_t * const pxMACAddress,
                                                 struct xNetworkEndPoint ** ppxEndPoint )
    {
        eResolutionLookupResult_t eReturn = eResolutionFailed;
        uint32_t ulAddressToLookup;
        NetworkEndPoint_t * pxEndPoint = NULL;

        configASSERT( pxMACAddress != NULL );
        configASSERT( pulIPAddress != NULL );
        configASSERT( ppxEndPoint != NULL );

        *( ppxEndPoint ) = NULL;
        ulAddressToLookup = *pulIPAddress;
        pxEndPoint = FreeRTOS_FindEndPointOnIP_IPv4( ulAddressToLookup );

        if( xIsIPv4Loopback( ulAddressToLookup ) != 0 )
        {
            if( pxEndPoint != NULL )
            {
                /* 对于多播，使用第一个 IPv4 端点。 */
                ( void ) memcpy( pxMACAddress->ucBytes, pxEndPoint->xMACAddress.ucBytes, sizeof( pxMACAddress->ucBytes ) );
                *( ppxEndPoint ) = pxEndPoint;
                eReturn = eResolutionCacheHit;
            }
        }
        else if( xIsIPv4Multicast( ulAddressToLookup ) != 0 )
        {
            /* 获取 IP 地址的最低 23 位。 */
            vSetMultiCastIPv4MacAddress( ulAddressToLookup, pxMACAddress );

            pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

            for( ;
                 pxEndPoint != NULL;
                 pxEndPoint = FreeRTOS_NextEndPoint( NULL, pxEndPoint ) )
            {
                if( pxEndPoint->bits.bIPv6 == 0U ) /* 在 for 循环中已检查 NULL 端点，无需额外检查 */
                {
                    /* 对于多播，使用第一个 IPv4 端点。 */
                    *( ppxEndPoint ) = pxEndPoint;
                    eReturn = eResolutionCacheHit;
                    break;
                }
            }
        }
        else if( ( FreeRTOS_htonl( ulAddressToLookup ) & 0xffU ) == 0xffU ) /* 这是像 x.x.x.255 这样的广播地址吗？ */
        {
            /* 这是广播，因此使用广播 MAC 地址。 */
            ( void ) memcpy( pxMACAddress->ucBytes, xBroadcastMACAddress.ucBytes, sizeof( MACAddress_t ) );
            pxEndPoint = FreeRTOS_FindEndPointOnNetMask( ulAddressToLookup );

            if( pxEndPoint != NULL )
            {
                *( ppxEndPoint ) = pxEndPoint;
            }

            eReturn = eResolutionCacheHit;
        }
        else
        {
            eReturn = eARPGetCacheEntryGateWay( pulIPAddress, pxMACAddress, ppxEndPoint );
        }

        return eReturn;
    }
/*-----------------------------------------------------------*/

/**
 * @brief 该 IPv4 地址显然是一个外网地址。查找网关..
 * @param[in] pulIPAddress 目标 IP 地址。它可能会被网关的 IP 地址替换。
 * @param[in] pxMACAddress 如果在缓存中找到 MAC 地址，它将被存储到提供的缓冲区中。
 * @param[out] ppxEndPoint 网关的端点将被复制到指针所指位置。
 */
    static eResolutionLookupResult_t eARPGetCacheEntryGateWay( uint32_t * pulIPAddress,
                                                               MACAddress_t * const pxMACAddress,
                                                               struct xNetworkEndPoint ** ppxEndPoint )
    {
        eResolutionLookupResult_t eReturn = eResolutionCacheMiss;
        uint32_t ulAddressToLookup = *( pulIPAddress );
        NetworkEndPoint_t * pxEndPoint;
        uint32_t ulOriginal = *pulIPAddress;

        /* 假定具有相同子网掩码的设备位于同一 LAN 上，不需要网关。 */
        pxEndPoint = FreeRTOS_FindEndPointOnNetMask( ulAddressToLookup );

        if( pxEndPoint == NULL )
        {
            /* 未找到匹配的端点，寻找网关。 */
            #if ( ipconfigARP_STORES_REMOTE_ADDRESSES == 1 )
                eReturn = prvCacheLookup( ulAddressToLookup, pxMACAddress, ppxEndPoint );

                if( eReturn == eResolutionCacheHit )
                {
                    /* 协议栈配置为存储“远程 IP 地址”，即属于不同子网掩码的地址。
                     * prvCacheLookup() 返回命中，因此 MAC 地址已知。 */
                }
                else
            #endif
            {
                /* IP 地址不在本地网络上，因此查找路由器的硬件地址（如果有）。 */
                *( ppxEndPoint ) = FreeRTOS_FindGateWay( ( BaseType_t ) ipTYPE_IPv4 );

                if( *( ppxEndPoint ) != NULL )
                {
                    /* 可以安全访问 'ipv4_settings'，因为提供了 'ipTYPE_IPv4'。 */
                    ulAddressToLookup = ( *ppxEndPoint )->ipv4_settings.ulGatewayAddress;
                }
                else
                {
                    ulAddressToLookup = 0U;
                }
            }
        }
        else
        {
            /* IP 地址在本地网络上，因此直接查找请求的 IP 地址。 */
            ulAddressToLookup = *pulIPAddress;
            *ppxEndPoint = pxEndPoint;
        }

        #if ( ipconfigARP_STORES_REMOTE_ADDRESSES == 1 )
            if( eReturn == eResolutionCacheMiss )
        #endif
        {
            if( ulAddressToLookup == 0U )
            {
                /* 地址不在本地网络上，并且没有路由器。 */
                eReturn = eResolutionFailed;
            }
            else
            {
                eReturn = prvCacheLookup( ulAddressToLookup, pxMACAddress, ppxEndPoint );

                if( ( eReturn != eResolutionCacheHit ) || ( ulOriginal != ulAddressToLookup ) )
                {
                    FreeRTOS_debug_printf( ( "ARP %xip %s using %xip\n",
                                             ( unsigned ) FreeRTOS_ntohl( ulOriginal ),
                                             ( eReturn == eResolutionCacheHit ) ? "hit" : "miss",
                                             ( unsigned ) FreeRTOS_ntohl( ulAddressToLookup ) ) );
                }

                /* 可能 ARP 必须发送到网关。 */
                *pulIPAddress = ulAddressToLookup;
            }
        }

        return eReturn;
    }
/*-----------------------------------------------------------*/

/**
 * @brief 在 ARP 缓存中查找 IP 地址。
 *
 * @param[in] ulAddressToLookup 要查找的 IP 地址的 32 位表示形式。
 * @param[out] pxMACAddress 指向 MACAddress_t 变量的指针，如果 ARP 缓存命中，
 *                          与该 IP 地址对应的 MAC 地址将存储在此处。
 * @param[in,out] ppxEndPoint 将存储指向端点的指针。
 *
 * @return 找到 IP 地址时：eResolutionCacheHit，未找到时：eResolutionCacheMiss，
 *         等待 ARP 回复时：eResolutionFailed。
 */
    static eResolutionLookupResult_t prvCacheLookup( uint32_t ulAddressToLookup,
                                                     MACAddress_t * const pxMACAddress,
                                                     NetworkEndPoint_t ** ppxEndPoint )
    {
        BaseType_t x;
        eResolutionLookupResult_t eReturn = eResolutionCacheMiss;

        /* 遍历 ARP 缓存中的每个条目。 */
        for( x = 0; x < ipconfigARP_CACHE_ENTRIES; x++ )
        {
            /* ARP 缓存表中的这一行是否包含正在查询的 IP 地址的条目？ */
            if( xARPCache[ x ].ulIPAddress == ulAddressToLookup )
            {
                /* 找到了匹配的有效条目。 */
                if( xARPCache[ x ].ucValid == ( uint8_t ) pdFALSE )
                {
                    /* 此条目正在等待 ARP 回复，因此无效。 */
                    eReturn = eResolutionFailed;
                }
                else
                {
                    /* 找到了有效条目。 */
                    ( void ) memcpy( pxMACAddress->ucBytes, xARPCache[ x ].xMACAddress.ucBytes, sizeof( MACAddress_t ) );
                    /* 在唯一的调用者 eARPGetCacheEntry() 中已测试了 ppxEndPoint != NULL。 */
                    *( ppxEndPoint ) = xARPCache[ x ].pxEndPoint;
                    eReturn = eResolutionCacheHit;
                }

                break;
            }
        }

        return eReturn;
    }
/*-----------------------------------------------------------*/

/**
 * @brief 调用此函数将更新（或“老化”）ARP 缓存条目。
 *        该函数还将尝试通过发送 ARP 查询来防止条目被移除。
 *        它还会检查我们是否正在等待 ARP 回复 - 如果是，则将重新发送 ARP 请求。
 *        如果 ARP 条目已“老化”为 0，它将从 ARP 缓存中移除。
 */
    void vARPAgeCache( void )
    {
        BaseType_t x;
        TickType_t xTimeNow;

        /* 遍历 ARP 缓存中的每个条目。 */
        for( x = 0; x < ipconfigARP_CACHE_ENTRIES; x++ )
        {
            /* 如果条目有效（其寿命大于零）。 */
            if( xARPCache[ x ].ucAge > 0U )
            {
                /* 递减此 ARP 缓存表行中条目的寿命值。当寿命达到零时，它不再被视为有效。 */
                ( xARPCache[ x ].ucAge )--;

                /* 如果条目尚未有效，则它正在等待 ARP 回复，应重新传输 ARP 请求。 */
                if( xARPCache[ x ].ucValid == ( uint8_t ) pdFALSE )
                {
                    FreeRTOS_OutputARPRequest( xARPCache[ x ].ulIPAddress );
                }
                else if( xARPCache[ x ].ucAge <= ( uint8_t ) arpMAX_ARP_AGE_BEFORE_NEW_ARP_REQUEST )
                {
                    /* 此条目很快将被移除。查看 MAC 地址是否仍然有效以防止这种情况发生。 */
                    iptraceARP_TABLE_ENTRY_WILL_EXPIRE( xARPCache[ x ].ulIPAddress );
                    FreeRTOS_OutputARPRequest( xARPCache[ x ].ulIPAddress );
                }
                else
                {
                    /* 寿命刚刚递减，无需执行操作。 */
                }

                if( xARPCache[ x ].ucAge == 0U )
                {
                    /* 该条目不再有效。将其清除。 */
                    iptraceARP_TABLE_ENTRY_EXPIRED( xARPCache[ x ].ulIPAddress );
                    xARPCache[ x ].ulIPAddress = 0U;
                }
            }
        }

        xTimeNow = xTaskGetTickCount();

        if( ( xLastGratuitousARPTime == ( TickType_t ) 0 ) || ( ( xTimeNow - xLastGratuitousARPTime ) > ( TickType_t ) arpGRATUITOUS_ARP_PERIOD ) )
        {
            NetworkEndPoint_t * pxEndPoint = pxNetworkEndPoints;

            while( pxEndPoint != NULL )
            {
                if( ( pxEndPoint->bits.bEndPointUp != pdFALSE_UNSIGNED ) && ( pxEndPoint->ipv4_settings.ulIPAddress != 0U ) )
                {
                    if( pxEndPoint->bits.bIPv6 == pdFALSE_UNSIGNED ) /* LCOV_EXCL_BR_LINE */
                    {
                        FreeRTOS_OutputARPRequest_Multi( pxEndPoint, pxEndPoint->ipv4_settings.ulIPAddress );
                    }
                }

                pxEndPoint = pxEndPoint->pxNext;
            }

            xLastGratuitousARPTime = xTimeNow;
        }
    }
/*-----------------------------------------------------------*/

/**
 * @brief 发送免费 ARP 包，允许本节点向整个网络宣告 IP-MAC 映射。
 */
    void vARPSendGratuitous( void )
    {
        /* 将 xLastGratuitousARPTime 设置为 0 将在下次调用 vARPAgeCache() 时强制发送免费 ARP。 */
        xLastGratuitousARPTime = ( TickType_t ) 0;

        /* 让 IP 任务调用 vARPAgeCache()。 */
        ( void ) xSendEventToIPTask( eARPTimerEvent );
    }

/*-----------------------------------------------------------*/

/**
 * @brief 创建并向给定的 IPv4 端点发送 ARP 请求数据包。
 *
 * @param[in] pxEndPoint 发送请求应通过的端点。
 * @param[in] ulIPAddress 需要其物理 (MAC) 地址的 IP 地址的 32 位表示形式。
 */
    void FreeRTOS_OutputARPRequest_Multi( NetworkEndPoint_t * pxEndPoint,
                                          uint32_t ulIPAddress )
    {
        NetworkBufferDescriptor_t * pxNetworkBuffer;

        if( ( pxEndPoint->bits.bIPv6 == pdFALSE_UNSIGNED ) &&
            ( pxEndPoint->ipv4_settings.ulIPAddress != 0U ) )
        {
            /* 这是从 IP 事件任务的上下文中调用的，因此不能使用阻塞时间。 */
            pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( sizeof( ARPPacket_t ), ( TickType_t ) 0U );

            if( pxNetworkBuffer != NULL )
            {
                pxNetworkBuffer->xIPAddress.ulIP_IPv4 = ulIPAddress;
                pxNetworkBuffer->pxEndPoint = pxEndPoint;
                pxNetworkBuffer->pxInterface = pxEndPoint->pxNetworkInterface;
                vARPGenerateRequestPacket( pxNetworkBuffer );

                #if ( ipconfigETHERNET_MINIMUM_PACKET_BYTES > 0 )
                {
                    if( pxNetworkBuffer->xDataLength < ( size_t ) ipconfigETHERNET_MINIMUM_PACKET_BYTES )
                    {
                        BaseType_t xIndex;

                        for( xIndex = ( BaseType_t ) pxNetworkBuffer->xDataLength; xIndex < ( BaseType_t ) ipconfigETHERNET_MINIMUM_PACKET_BYTES; xIndex++ )
                        {
                            pxNetworkBuffer->pucEthernetBuffer[ xIndex ] = 0U;
                        }

                        pxNetworkBuffer->xDataLength = ( size_t ) ipconfigETHERNET_MINIMUM_PACKET_BYTES;
                    }
                }
                #endif /* if( ipconfigETHERNET_MINIMUM_PACKET_BYTES > 0 ) */

                if( xIsCallingFromIPTask() != pdFALSE )
                {
                    iptraceNETWORK_INTERFACE_OUTPUT( pxNetworkBuffer->xDataLength, pxNetworkBuffer->pucEthernetBuffer );

                    /* 只有 IP 任务才允许直接调用此函数。 */
                    if( pxEndPoint->pxNetworkInterface != NULL )
                    {
                        ( void ) pxEndPoint->pxNetworkInterface->pfOutput( pxEndPoint->pxNetworkInterface, pxNetworkBuffer, pdTRUE );
                    }
                }
                else
                {
                    IPStackEvent_t xSendEvent;

                    /* 向 IP 任务发送消息以发送此 ARP 包。 */
                    xSendEvent.eEventType = eNetworkTxEvent;
                    xSendEvent.pvData = pxNetworkBuffer;

                    if( xSendEventStructToIPTask( &xSendEvent, ( TickType_t ) portMAX_DELAY ) == pdFAIL )
                    {
                        /* 发送消息失败，因此释放网络缓冲区。 */
                        vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );
                    }
                }
            }
        }
    }

/*-----------------------------------------------------------*/

/**
 * @brief 创建并发送 ARP 请求数据包。
 *
 * @param[in] ulIPAddress 需要其物理 (MAC) 地址的 IP 地址的 32 位表示形式。
 */
    void FreeRTOS_OutputARPRequest( uint32_t ulIPAddress )
    {
        /* 假设系统中属于不同物理接口的 IPv4 端点将具有不同的子网，
         * 但同一接口上的端点可能具有相同的子网。 */
        NetworkEndPoint_t * pxEndPoint = FreeRTOS_FindEndPointOnNetMask( ulIPAddress );

        if( pxEndPoint != NULL )
        {
            FreeRTOS_OutputARPRequest_Multi( pxEndPoint, ulIPAddress );
        }
    }
/*-----------------------------------------------------------*/

/**
 * @brief  等待地址解析：在 ARP 缓存中查找 IP 地址，如果需要则发送 ARP 请求，并等待回复。
 *         在调用 FreeRTOS_sendto() 之前调用此函数非常有用。
 *
 * @param[in] ulIPAddress 要查找的 IP 地址。
 * @param[in] uxTicksToWait 等待回复的最大时钟滴答数。
 *
 * @return 成功时返回零。
 */
    BaseType_t xARPWaitResolution( uint32_t ulIPAddress,
                                   TickType_t uxTicksToWait )
    {
        BaseType_t xResult = -pdFREERTOS_ERRNO_EADDRNOTAVAIL;
        TimeOut_t xTimeOut;
        MACAddress_t xMACAddress;
        eResolutionLookupResult_t xLookupResult;
        NetworkEndPoint_t * pxEndPoint;
        size_t uxSendCount = ipconfigMAX_ARP_RETRANSMISSIONS;
        uint32_t ulIPAddressCopy = ulIPAddress;

        /* IP 任务不应调用此函数。 */
        configASSERT( xIsCallingFromIPTask() == pdFALSE );

        xLookupResult = eARPGetCacheEntry( &( ulIPAddressCopy ), &( xMACAddress ), &( pxEndPoint ) );

        if( xLookupResult == eResolutionCacheMiss )
        {
            const TickType_t uxSleepTime = pdMS_TO_TICKS( 250U );

            /* 我们可以在这里使用 ipconfigMAX_ARP_RETRANSMISSIONS。 */
            vTaskSetTimeOutState( &xTimeOut );

            while( uxSendCount > 0U )
            {
                FreeRTOS_OutputARPRequest( ulIPAddressCopy );

                vTaskDelay( uxSleepTime );

                xLookupResult = eARPGetCacheEntry( &( ulIPAddressCopy ), &( xMACAddress ), &( pxEndPoint ) );

                if( ( xTaskCheckForTimeOut( &( xTimeOut ), &( uxTicksToWait ) ) == pdTRUE ) ||
                    ( xLookupResult != eResolutionCacheMiss ) )
                {
                    break;
                }

                /* 递减计数。 */
                uxSendCount--;
            }
        }

        if( xLookupResult == eResolutionCacheHit )
        {
            xResult = 0;
        }

        return xResult;
    }
/*-----------------------------------------------------------*/

/**
 * @brief 通过将各种常量细节复制到缓冲区来生成 ARP 请求数据包。
 *
 * @param[in,out] pxNetworkBuffer 指向必须填充 ARP 请求数据包详细信息的缓冲区的指针。
 */
    void vARPGenerateRequestPacket( NetworkBufferDescriptor_t * const pxNetworkBuffer )
    {
/* 发送 IPv4 ARP 包时，以太网和 ARP 头的一部分始终是常量。
 * 此数组定义了常量部分，允许使用简单的 memcpy() 填充数据包的这部分，而不是单独写入。 */
        static const uint8_t xDefaultPartARPPacketHeader[] =
        {
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 以太网目标地址。 */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 以太网源地址。 */
            0x08, 0x06,                         /* 以太网帧类型 (ipARP_FRAME_TYPE)。 */
            0x00, 0x01,                         /* usHardwareType (ipARP_HARDWARE_TYPE_ETHERNET)。 */
            0x08, 0x00,                         /* usProtocolType。 */
            ipMAC_ADDRESS_LENGTH_BYTES,         /* ucHardwareAddressLength。 */
            ipIP_ADDRESS_LENGTH_BYTES,          /* ucProtocolAddressLength。 */
            0x00, 0x01,                         /* usOperation (ipARP_REQUEST)。 */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* xSenderHardwareAddress。 */
            0x00, 0x00, 0x00, 0x00,             /* ulSenderProtocolAddress。 */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* xTargetHardwareAddress。 */
        };

        ARPPacket_t * pxARPPacket;

/* 用于符合 MISRA 规则 21.15 的 memcpy() 辅助变量 */
        const void * pvCopySource;
        void * pvCopyDest;

        /* 缓冲区分配确保缓冲区始终有空间容纳 ARP 包。
         * 参见 portable/BufferManagement 下的缓冲区分配实现 1 和 2。 */
        configASSERT( pxNetworkBuffer != NULL );
        configASSERT( pxNetworkBuffer->xDataLength >= sizeof( ARPPacket_t ) );
        configASSERT( pxNetworkBuffer->pxEndPoint != NULL );

        /* MISRA 参考 11.3.1 [未对齐访问] */
/* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/blob/main/MISRA.md#rule-113 */
        /* coverity[misra_c_2012_rule_11_3_violation] */
        pxARPPacket = ( ( ARPPacket_t * ) pxNetworkBuffer->pucEthernetBuffer );

        /* 将头部信息的常量部分 memcpy 到数据包中的正确位置。这将复制：
         *  xEthernetHeader.ulDestinationAddress
         *  xEthernetHeader.usFrameType;
         *  xARPHeader.usHardwareType;
         *  xARPHeader.usProtocolType;
         *  xARPHeader.ucHardwareAddressLength;
         *  xARPHeader.ucProtocolAddressLength;
         *  xARPHeader.usOperation;
         *  xARPHeader.xTargetHardwareAddress;
         */

        /*
         * 使用辅助变量进行 memcpy() 以符合 MISRA 规则 21.15。
         * 这些应该会被优化掉。
         */
        pvCopySource = xDefaultPartARPPacketHeader;
        pvCopyDest = pxARPPacket;
        ( void ) memcpy( pvCopyDest, pvCopySource, sizeof( xDefaultPartARPPacketHeader ) );

        pvCopySource = pxNetworkBuffer->pxEndPoint->xMACAddress.ucBytes;
        pvCopyDest = pxARPPacket->xEthernetHeader.xSourceAddress.ucBytes;
        ( void ) memcpy( pvCopyDest, pvCopySource, ipMAC_ADDRESS_LENGTH_BYTES );

        pvCopySource = pxNetworkBuffer->pxEndPoint->xMACAddress.ucBytes;
        pvCopyDest = pxARPPacket->xARPHeader.xSenderHardwareAddress.ucBytes;
        ( void ) memcpy( pvCopyDest, pvCopySource, ipMAC_ADDRESS_LENGTH_BYTES );

        pvCopySource = &( pxNetworkBuffer->pxEndPoint->ipv4_settings.ulIPAddress );
        pvCopyDest = pxARPPacket->xARPHeader.ucSenderProtocolAddress;
        ( void ) memcpy( pvCopyDest, pvCopySource, sizeof( pxARPPacket->xARPHeader.ucSenderProtocolAddress ) );
        pxARPPacket->xARPHeader.ulTargetProtocolAddress = pxNetworkBuffer->xIPAddress.ulIP_IPv4;

        pxNetworkBuffer->xDataLength = sizeof( ARPPacket_t );

        iptraceCREATING_ARP_REQUEST( pxNetworkBuffer->xIPAddress.ulIP_IPv4 );
    }
/*-----------------------------------------------------------*/

/**
 * @brief 调用此函数将清除 ARP 缓存。
 * @param[in] pxEndPoint 仅清除此端点的条目，当为 NULL 时，清除整个 ARP 缓存。
 */
    void FreeRTOS_ClearARP( const struct xNetworkEndPoint * pxEndPoint )
    {
        if( pxEndPoint != NULL )
        {
            BaseType_t x;

            for( x = 0; x < ipconfigARP_CACHE_ENTRIES; x++ )
            {
                if( xARPCache[ x ].pxEndPoint == pxEndPoint )
                {
                    ( void ) memset( &( xARPCache[ x ] ), 0, sizeof( ARPCacheRow_t ) );
                }
            }
        }
        else
        {
            ( void ) memset( xARPCache, 0, sizeof( xARPCache ) );
        }
    }
/*-----------------------------------------------------------*/

    #if ( ipconfigHAS_PRINTF != 0 ) || ( ipconfigHAS_DEBUG_PRINTF != 0 )

        void FreeRTOS_PrintARPCache( void )
        {
            BaseType_t x, xCount = 0;

            /* 遍历 ARP 缓存中的每个条目。 */
            for( x = 0; x < ipconfigARP_CACHE_ENTRIES; x++ )
            {
                if( ( xARPCache[ x ].ulIPAddress != 0U ) && ( xARPCache[ x ].ucAge > ( uint8_t ) 0U ) )
                {
                    /* 查看 MAC 地址是否也匹配，我们就都开心了 */
                    FreeRTOS_printf( ( "ARP %2d: %3u - %16xip : %02x:%02x:%02x : %02x:%02x:%02x\n",
                                       ( int ) x,
                                       xARPCache[ x ].ucAge,
                                       ( unsigned ) FreeRTOS_ntohl( xARPCache[ x ].ulIPAddress ),
                                       xARPCache[ x ].xMACAddress.ucBytes[ 0 ],
                                       xARPCache[ x ].xMACAddress.ucBytes[ 1 ],
                                       xARPCache[ x ].xMACAddress.ucBytes[ 2 ],
                                       xARPCache[ x ].xMACAddress.ucBytes[ 3 ],
                                       xARPCache[ x ].xMACAddress.ucBytes[ 4 ],
                                       xARPCache[ x ].xMACAddress.ucBytes[ 5 ] ) );
                    xCount++;
                }
            }

            FreeRTOS_printf( ( "Arp has %ld entries\n", xCount ) );
        }
    #endif /* ( ipconfigHAS_PRINTF != 0 ) || ( ipconfigHAS_DEBUG_PRINTF != 0 ) */

#endif /* ( ipconfigUSE_IPv4 != 0 ) */

