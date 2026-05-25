/*
 * FreeRTOS 内核 V11.1.0
 * 版权所有 (C) 2021 Amazon.com, Inc. 或其附属公司。保留所有权利。
 *
 * SPDX-License-Identifier: MIT
 *
 * 特此免费授予获得本软件及相关文档文件（“软件”）副本的任何人不受限制地处理本软件的权利，
 * 包括但不限于使用、复制、修改、合并、发布、分发、再授权和/或销售本软件副本的权利，
 * 以及允许向其提供本软件的人员在遵守以下条件的前提下行使上述权利：
 *
 * 上述版权声明和本许可声明应包含在本软件的所有副本或主要部分中。
 *
 * 本软件按“原样”提供，不作任何明示或暗示的保证，包括但不仅限于对适销性和特定用途适用性的
 * 暗示保证。在任何情况下，作者或版权持有人均不对因本软件或本软件的使用或其他交易引起的任何索赔、
 * 损害或其他责任承担责任，无论是在合同诉讼、侵权行为还是其他方面，即使已被告知可能发生此类损害。
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/* 标准库头文件包含。 */
#include <stdlib.h>

/* 定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 可以防止 task.h 重新定义所有 API 函数
 * 以使用 MPU 包装器。那只应在 task.h 被应用程序文件包含时进行。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* FreeRTOS 内核头文件包含。 */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "event_groups.h"

/* MPU 移植层要求上面的头文件定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE，
 * 但在本文件中不需要，以便生成正确的特权与非特权链接和放置。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* 如果应用程序未配置包含事件组功能，则此整个源文件将被跳过。
 * 此 #if 在本文件的最底部关闭。如果要包含事件组，请确保在 FreeRTOSConfig.h
 * 中将 configUSE_EVENT_GROUPS 设置为 1。 */
#if ( configUSE_EVENT_GROUPS == 1 )

    typedef struct EventGroupDef_t
    {
        EventBits_t uxEventBits;
        List_t xTasksWaitingForBits; /**< 等待位被设置的任务列表。 */

        #if ( configUSE_TRACE_FACILITY == 1 )
            UBaseType_t uxEventGroupNumber;
        #endif

        #if ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
            uint8_t ucStaticallyAllocated; /**< 如果事件组是静态分配的，则设置为 pdTRUE，以确保不会尝试释放内存。 */
        #endif
    } EventGroup_t;

/*-----------------------------------------------------------*/

/*
 * 测试 uxCurrentEventBits 中设置的位，查看是否满足等待条件。
 * 等待条件由 xWaitForAllBits 定义。如果 xWaitForAllBits 为 pdTRUE，
 * 则当 uxBitsToWaitFor 中设置的所有位也在 uxCurrentEventBits 中设置时，满足等待条件。
 * 如果 xWaitForAllBits 为 pdFALSE，则当 uxBitsToWaitFor 中设置的任何位也在
 * uxCurrentEventBits 中设置时，满足等待条件。
 */
    static BaseType_t prvTestWaitCondition( const EventBits_t uxCurrentEventBits,
                                            const EventBits_t uxBitsToWaitFor,
                                            const BaseType_t xWaitForAllBits ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

    #if ( configSUPPORT_STATIC_ALLOCATION == 1 )

        EventGroupHandle_t xEventGroupCreateStatic( StaticEventGroup_t * pxEventGroupBuffer )
        {
            EventGroup_t * pxEventBits;

            traceENTER_xEventGroupCreateStatic( pxEventGroupBuffer );

            /* 必须提供一个 StaticEventGroup_t 对象。 */
            configASSERT( pxEventGroupBuffer );

            #if ( configASSERT_DEFINED == 1 )
            {
                /* 完整性检查，用于声明 StaticEventGroup_t 类型变量的结构大小
                 * 必须等于真实事件组结构的大小。 */
                volatile size_t xSize = sizeof( StaticEventGroup_t );
                configASSERT( xSize == sizeof( EventGroup_t ) );
            }
            #endif /* configASSERT_DEFINED */

            /* 用户提供了一个静态分配的事件组 - 使用它。 */
            /* MISRA 参考 11.3.1 [未对齐访问] */
            /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-113 */
            /* coverity[misra_c_2012_rule_11_3_violation] */
            pxEventBits = ( EventGroup_t * ) pxEventGroupBuffer;

            if( pxEventBits != NULL )
            {
                pxEventBits->uxEventBits = 0;
                vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );

                #if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
                {
                    /* 静态和动态分配都可以使用，因此记录此事件组是静态创建的，
                     * 以防事件组稍后被删除。 */
                    pxEventBits->ucStaticallyAllocated = pdTRUE;
                }
                #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

                traceEVENT_GROUP_CREATE( pxEventBits );
            }
            else
            {
                /* xEventGroupCreateStatic 应该只在 pxEventGroupBuffer 指向
                 * 预分配（编译时分配）的 StaticEventGroup_t 变量时被调用。 */
                traceEVENT_GROUP_CREATE_FAILED();
            }

            traceRETURN_xEventGroupCreateStatic( pxEventBits );

            return pxEventBits;
        }

    #endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

    #if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

        EventGroupHandle_t xEventGroupCreate( void )
        {
            EventGroup_t * pxEventBits;

            traceENTER_xEventGroupCreate();

            /* MISRA 参考 11.5.1 [Malloc 内存分配] */
            /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-115 */
            /* coverity[misra_c_2012_rule_11_5_violation] */
            pxEventBits = ( EventGroup_t * ) pvPortMalloc( sizeof( EventGroup_t ) );

            if( pxEventBits != NULL )
            {
                pxEventBits->uxEventBits = 0;
                vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );

                #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
                {
                    /* 静态和动态分配都可以使用，因此记录此事件组是动态分配的，
                     * 以防事件组稍后被删除。 */
                    pxEventBits->ucStaticallyAllocated = pdFALSE;
                }
                #endif /* configSUPPORT_STATIC_ALLOCATION */

                traceEVENT_GROUP_CREATE( pxEventBits );
            }
            else
            {
                traceEVENT_GROUP_CREATE_FAILED();
            }

            traceRETURN_xEventGroupCreate( pxEventBits );

            return pxEventBits;
        }

    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

    EventBits_t xEventGroupSync( EventGroupHandle_t xEventGroup,
                                 const EventBits_t uxBitsToSet,
                                 const EventBits_t uxBitsToWaitFor,
                                 TickType_t xTicksToWait )
    {
        EventBits_t uxOriginalBitValue, uxReturn;
        EventGroup_t * pxEventBits = xEventGroup;
        BaseType_t xAlreadyYielded;
        BaseType_t xTimeoutOccurred = pdFALSE;

        traceENTER_xEventGroupSync( xEventGroup, uxBitsToSet, uxBitsToWaitFor, xTicksToWait );

        configASSERT( ( uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES ) == 0 );
        configASSERT( uxBitsToWaitFor != 0 );
        #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
        {
            configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
        }
        #endif

        vTaskSuspendAll();
        {
            uxOriginalBitValue = pxEventBits->uxEventBits;

            ( void ) xEventGroupSetBits( xEventGroup, uxBitsToSet );

            if( ( ( uxOriginalBitValue | uxBitsToSet ) & uxBitsToWaitFor ) == uxBitsToWaitFor )
            {
                /* 所有汇合位现在都已设置 - 无需阻塞。 */
                uxReturn = ( uxOriginalBitValue | uxBitsToSet );

                /* 汇合操作总是清除位。除非这是汇合中唯一的任务，
                 * 否则它们应该已经被清除了。 */
                pxEventBits->uxEventBits &= ~uxBitsToWaitFor;

                xTicksToWait = 0;
            }
            else
            {
                if( xTicksToWait != ( TickType_t ) 0 )
                {
                    traceEVENT_GROUP_SYNC_BLOCK( xEventGroup, uxBitsToSet, uxBitsToWaitFor );

                    /* 将调用任务正在等待的位存储在任务的事件列表项中，
                     * 以便内核在找到匹配时知道。然后进入阻塞状态。 */
                    vTaskPlaceOnUnorderedEventList( &( pxEventBits->xTasksWaitingForBits ), ( uxBitsToWaitFor | eventCLEAR_EVENTS_ON_EXIT_BIT | eventWAIT_FOR_ALL_BITS ), xTicksToWait );

                    /* 这个赋值是多余的，因为 uxReturn 会在任务解除阻塞后被设置，
                     * 但如果省略此赋值，某些编译器会错误地生成关于 uxReturn 未设置即返回的警告。 */
                    uxReturn = 0;
                }
                else
                {
                    /* 汇合位未设置，但未指定阻塞时间 - 仅返回当前事件位值。 */
                    uxReturn = pxEventBits->uxEventBits;
                    xTimeoutOccurred = pdTRUE;
                }
            }
        }
        xAlreadyYielded = xTaskResumeAll();

        if( xTicksToWait != ( TickType_t ) 0 )
        {
            if( xAlreadyYielded == pdFALSE )
            {
                taskYIELD_WITHIN_API();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            /* 任务阻塞等待其所需的位被设置 - 此时要么所需的位已被设置，
             * 要么阻塞时间已过期。如果所需的位被设置，它们将被存储在任务
             * 的事件列表项中，现在应该检索然后清除。 */
            uxReturn = uxTaskResetEventItemValue();

            if( ( uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET ) == ( EventBits_t ) 0 )
            {
                /* 任务超时，仅返回当前事件位值。 */
                taskENTER_CRITICAL();
                {
                    uxReturn = pxEventBits->uxEventBits;

                    /* 虽然任务到达这里是因为在它等待的位被设置之前超时了，
                     * 但自它解除阻塞以来，可能另一个任务已经设置了这些位。
                     * 如果是这种情况，则需要在退出前清除这些位。 */
                    if( ( uxReturn & uxBitsToWaitFor ) == uxBitsToWaitFor )
                    {
                        pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                taskEXIT_CRITICAL();

                xTimeoutOccurred = pdTRUE;
            }
            else
            {
                /* 任务因位被设置而解除阻塞。 */
            }

            /* 控制位可能被设置为任务曾阻塞的标志，不应返回。 */
            uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
        }

        traceEVENT_GROUP_SYNC_END( xEventGroup, uxBitsToSet, uxBitsToWaitFor, xTimeoutOccurred );

        /* 当不使用跟踪宏时，防止编译器警告。 */
        ( void ) xTimeoutOccurred;

        traceRETURN_xEventGroupSync( uxReturn );

        return uxReturn;
    }
/*-----------------------------------------------------------*/

    EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup,
                                     const EventBits_t uxBitsToWaitFor,
                                     const BaseType_t xClearOnExit,
                                     const BaseType_t xWaitForAllBits,
                                     TickType_t xTicksToWait )
    {
        EventGroup_t * pxEventBits = xEventGroup;
        EventBits_t uxReturn, uxControlBits = 0;
        BaseType_t xWaitConditionMet, xAlreadyYielded;
        BaseType_t xTimeoutOccurred = pdFALSE;

        traceENTER_xEventGroupWaitBits( xEventGroup, uxBitsToWaitFor, xClearOnExit, xWaitForAllBits, xTicksToWait );

        /* 检查用户是否未尝试等待内核本身使用的位，并且至少请求了一个位。 */
        configASSERT( xEventGroup );
        configASSERT( ( uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES ) == 0 );
        configASSERT( uxBitsToWaitFor != 0 );
        #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
        {
            configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
        }
        #endif

        vTaskSuspendAll();
        {
            const EventBits_t uxCurrentEventBits = pxEventBits->uxEventBits;

            /* 检查等待条件是否已经满足。 */
            xWaitConditionMet = prvTestWaitCondition( uxCurrentEventBits, uxBitsToWaitFor, xWaitForAllBits );

            if( xWaitConditionMet != pdFALSE )
            {
                /* 等待条件已满足，因此无需阻塞。 */
                uxReturn = uxCurrentEventBits;
                xTicksToWait = ( TickType_t ) 0;

                /* 如果要求，则清除等待位。 */
                if( xClearOnExit != pdFALSE )
                {
                    pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else if( xTicksToWait == ( TickType_t ) 0 )
            {
                /* 等待条件未满足，但未指定阻塞时间，因此仅返回当前值。 */
                uxReturn = uxCurrentEventBits;
                xTimeoutOccurred = pdTRUE;
            }
            else
            {
                /* 任务将阻塞等待其所需的位被设置。uxControlBits 用于
                 * 记住此次 xEventGroupWaitBits() 调用的指定行为 -
                 * 以便在事件位解除任务阻塞时使用。 */
                if( xClearOnExit != pdFALSE )
                {
                    uxControlBits |= eventCLEAR_EVENTS_ON_EXIT_BIT;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                if( xWaitForAllBits != pdFALSE )
                {
                    uxControlBits |= eventWAIT_FOR_ALL_BITS;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                /* 将调用任务正在等待的位存储在任务的事件列表项中，
                 * 以便内核在找到匹配时知道。然后进入阻塞状态。 */
                vTaskPlaceOnUnorderedEventList( &( pxEventBits->xTasksWaitingForBits ), ( uxBitsToWaitFor | uxControlBits ), xTicksToWait );

                /* 这是多余的，因为它会在任务解除阻塞后被设置，
                 * 但如果不这样做，某些编译器会错误地生成关于变量未设置即返回的警告。 */
                uxReturn = 0;

                traceEVENT_GROUP_WAIT_BITS_BLOCK( xEventGroup, uxBitsToWaitFor );
            }
        }
        xAlreadyYielded = xTaskResumeAll();

        if( xTicksToWait != ( TickType_t ) 0 )
        {
            if( xAlreadyYielded == pdFALSE )
            {
                taskYIELD_WITHIN_API();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            /* 任务阻塞等待其所需的位被设置 - 此时要么所需的位被设置，
             * 要么阻塞时间过期。如果所需的位被设置，它们将被存储在任务
             * 的事件列表项中，现在应该检索然后清除。 */
            uxReturn = uxTaskResetEventItemValue();

            if( ( uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET ) == ( EventBits_t ) 0 )
            {
                taskENTER_CRITICAL();
                {
                    /* 任务超时，仅返回当前事件位值。 */
                    uxReturn = pxEventBits->uxEventBits;

                    /* 在任务离开阻塞状态到再次运行之间，事件位可能已被更新。 */
                    if( prvTestWaitCondition( uxReturn, uxBitsToWaitFor, xWaitForAllBits ) != pdFALSE )
                    {
                        if( xClearOnExit != pdFALSE )
                        {
                            pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    xTimeoutOccurred = pdTRUE;
                }
                taskEXIT_CRITICAL();
            }
            else
            {
                /* 任务因位被设置而解除阻塞。 */
            }

            /* 任务已阻塞，因此可能设置了控制位。 */
            uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
        }

        traceEVENT_GROUP_WAIT_BITS_END( xEventGroup, uxBitsToWaitFor, xTimeoutOccurred );

        /* 当不使用跟踪宏时，防止编译器警告。 */
        ( void ) xTimeoutOccurred;

        traceRETURN_xEventGroupWaitBits( uxReturn );

        return uxReturn;
    }
/*-----------------------------------------------------------*/

    EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup,
                                      const EventBits_t uxBitsToClear )
    {
        EventGroup_t * pxEventBits = xEventGroup;
        EventBits_t uxReturn;

        traceENTER_xEventGroupClearBits( xEventGroup, uxBitsToClear );

        /* 检查用户是否未尝试清除内核本身使用的位。 */
        configASSERT( xEventGroup );
        configASSERT( ( uxBitsToClear & eventEVENT_BITS_CONTROL_BYTES ) == 0 );

        taskENTER_CRITICAL();
        {
            traceEVENT_GROUP_CLEAR_BITS( xEventGroup, uxBitsToClear );

            /* 返回的值是位被清除之前的事件组值。 */
            uxReturn = pxEventBits->uxEventBits;

            /* 清除位。 */
            pxEventBits->uxEventBits &= ~uxBitsToClear;
        }
        taskEXIT_CRITICAL();

        traceRETURN_xEventGroupClearBits( uxReturn );

        return uxReturn;
    }
/*-----------------------------------------------------------*/

    #if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

        BaseType_t xEventGroupClearBitsFromISR( EventGroupHandle_t xEventGroup,
                                                const EventBits_t uxBitsToClear )
        {
            BaseType_t xReturn;

            traceENTER_xEventGroupClearBitsFromISR( xEventGroup, uxBitsToClear );

            traceEVENT_GROUP_CLEAR_BITS_FROM_ISR( xEventGroup, uxBitsToClear );
            xReturn = xTimerPendFunctionCallFromISR( vEventGroupClearBitsCallback, ( void * ) xEventGroup, ( uint32_t ) uxBitsToClear, NULL );

            traceRETURN_xEventGroupClearBitsFromISR( xReturn );

            return xReturn;
        }

    #endif /* if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) ) */
/*-----------------------------------------------------------*/

    EventBits_t xEventGroupGetBitsFromISR( EventGroupHandle_t xEventGroup )
    {
        UBaseType_t uxSavedInterruptStatus;
        EventGroup_t const * const pxEventBits = xEventGroup;
        EventBits_t uxReturn;

        traceENTER_xEventGroupGetBitsFromISR( xEventGroup );

        /* MISRA 参考 4.7.1 [应检查返回值] */
        /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#dir-47 */
        /* coverity[misra_c_2012_directive_4_7_violation] */
        uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
        {
            uxReturn = pxEventBits->uxEventBits;
        }
        taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );

        traceRETURN_xEventGroupGetBitsFromISR( uxReturn );

        return uxReturn;
    }
/*-----------------------------------------------------------*/

    EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup,
                                    const EventBits_t uxBitsToSet )
    {
        ListItem_t * pxListItem;
        ListItem_t * pxNext;
        ListItem_t const * pxListEnd;
        List_t const * pxList;
        EventBits_t uxBitsToClear = 0, uxBitsWaitedFor, uxControlBits;
        EventGroup_t * pxEventBits = xEventGroup;
        BaseType_t xMatchFound = pdFALSE;

        traceENTER_xEventGroupSetBits( xEventGroup, uxBitsToSet );

        /* 检查用户是否未尝试设置内核本身使用的位。 */
        configASSERT( xEventGroup );
        configASSERT( ( uxBitsToSet & eventEVENT_BITS_CONTROL_BYTES ) == 0 );

        pxList = &( pxEventBits->xTasksWaitingForBits );
        pxListEnd = listGET_END_MARKER( pxList );
        vTaskSuspendAll();
        {
            traceEVENT_GROUP_SET_BITS( xEventGroup, uxBitsToSet );

            pxListItem = listGET_HEAD_ENTRY( pxList );

            /* 设置位。 */
            pxEventBits->uxEventBits |= uxBitsToSet;

            /* 查看新的位值是否应解除任何任务的阻塞。 */
            while( pxListItem != pxListEnd )
            {
                pxNext = listGET_NEXT( pxListItem );
                uxBitsWaitedFor = listGET_LIST_ITEM_VALUE( pxListItem );
                xMatchFound = pdFALSE;

                /* 从控制位中分离出等待的位。 */
                uxControlBits = uxBitsWaitedFor & eventEVENT_BITS_CONTROL_BYTES;
                uxBitsWaitedFor &= ~eventEVENT_BITS_CONTROL_BYTES;

                if( ( uxControlBits & eventWAIT_FOR_ALL_BITS ) == ( EventBits_t ) 0 )
                {
                    /* 仅查找单个位被设置。 */
                    if( ( uxBitsWaitedFor & pxEventBits->uxEventBits ) != ( EventBits_t ) 0 )
                    {
                        xMatchFound = pdTRUE;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else if( ( uxBitsWaitedFor & pxEventBits->uxEventBits ) == uxBitsWaitedFor )
                {
                    /* 所有位都已设置。 */
                    xMatchFound = pdTRUE;
                }
                else
                {
                    /* 需要所有位被设置，但并非所有位都被设置了。 */
                }

                if( xMatchFound != pdFALSE )
                {
                    /* 位匹配。退出时应该清除位吗？ */
                    if( ( uxControlBits & eventCLEAR_EVENTS_ON_EXIT_BIT ) != ( EventBits_t ) 0 )
                    {
                        uxBitsToClear |= uxBitsWaitedFor;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* 在将任务从事件列表中移除之前，将实际的事件标志值存储在任务的事件列表项中。
                     * eventUNBLOCKED_DUE_TO_BIT_SET 位被设置，以便任务知道它是由于其所需位
                     * 匹配而解除阻塞，而不是因为超时。 */
                    vTaskRemoveFromUnorderedEventList( pxListItem, pxEventBits->uxEventBits | eventUNBLOCKED_DUE_TO_BIT_SET );
                }

                /* 移动到下一个列表项。注意此处未使用 pxListItem->pxNext，
                 * 因为列表项可能已从事件列表中移除并插入到了就绪/挂起读取列表中。 */
                pxListItem = pxNext;
            }

            /* 清除在控制字中设置 eventCLEAR_EVENTS_ON_EXIT_BIT 时匹配的任何位。 */
            pxEventBits->uxEventBits &= ~uxBitsToClear;
        }
        ( void ) xTaskResumeAll();

        traceRETURN_xEventGroupSetBits( pxEventBits->uxEventBits );

        return pxEventBits->uxEventBits;
    }
/*-----------------------------------------------------------*/

    void vEventGroupDelete( EventGroupHandle_t xEventGroup )
    {
        EventGroup_t * pxEventBits = xEventGroup;
        const List_t * pxTasksWaitingForBits;

        traceENTER_vEventGroupDelete( xEventGroup );

        configASSERT( pxEventBits );

        pxTasksWaitingForBits = &( pxEventBits->xTasksWaitingForBits );

        vTaskSuspendAll();
        {
            traceEVENT_GROUP_DELETE( xEventGroup );

            while( listCURRENT_LIST_LENGTH( pxTasksWaitingForBits ) > ( UBaseType_t ) 0 )
            {
                /* 解除任务的阻塞，返回 0，因为事件列表正在被删除，
                 * 因此不可能有任何位被设置。 */
                configASSERT( pxTasksWaitingForBits->xListEnd.pxNext != ( const ListItem_t * ) &( pxTasksWaitingForBits->xListEnd ) );
                vTaskRemoveFromUnorderedEventList( pxTasksWaitingForBits->xListEnd.pxNext, eventUNBLOCKED_DUE_TO_BIT_SET );
            }
        }
        ( void ) xTaskResumeAll();

        #if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
        {
            /* 事件组只能是动态分配的 - 再次释放它。 */
            vPortFree( pxEventBits );
        }
        #elif ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
        {
            /* 事件组可能是静态分配的，也可能是动态分配的，
             * 因此在尝试释放内存之前进行检查。 */
            if( pxEventBits->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
            {
                vPortFree( pxEventBits );
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        traceRETURN_vEventGroupDelete();
    }
/*-----------------------------------------------------------*/

    #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
        BaseType_t xEventGroupGetStaticBuffer( EventGroupHandle_t xEventGroup,
                                               StaticEventGroup_t ** ppxEventGroupBuffer )
        {
            BaseType_t xReturn;
            EventGroup_t * pxEventBits = xEventGroup;

            traceENTER_xEventGroupGetStaticBuffer( xEventGroup, ppxEventGroupBuffer );

            configASSERT( pxEventBits );
            configASSERT( ppxEventGroupBuffer );

            #if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
            {
                /* 检查事件组是否是静态分配的。 */
                if( pxEventBits->ucStaticallyAllocated == ( uint8_t ) pdTRUE )
                {
                    /* MISRA 参考 11.3.1 [未对齐访问] */
                    /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-113 */
                    /* coverity[misra_c_2012_rule_11_3_violation] */
                    *ppxEventGroupBuffer = ( StaticEventGroup_t * ) pxEventBits;
                    xReturn = pdTRUE;
                }
                else
                {
                    xReturn = pdFALSE;
                }
            }
            #else /* configSUPPORT_DYNAMIC_ALLOCATION */
            {
                /* 事件组必须是静态分配的。 */
                /* MISRA 参考 11.3.1 [未对齐访问] */
                /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-113 */
                /* coverity[misra_c_2012_rule_11_3_violation] */
                *ppxEventGroupBuffer = ( StaticEventGroup_t * ) pxEventBits;
                xReturn = pdTRUE;
            }
            #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

            traceRETURN_xEventGroupGetStaticBuffer( xReturn );

            return xReturn;
        }
    #endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/* 仅供内部使用 - 执行从中断中挂起的“设置位”命令。 */
    void vEventGroupSetBitsCallback( void * pvEventGroup,
                                     uint32_t ulBitsToSet )
    {
        traceENTER_vEventGroupSetBitsCallback( pvEventGroup, ulBitsToSet );

        /* MISRA 参考 11.5.4 [回调函数参数] */
        /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-115 */
        /* coverity[misra_c_2012_rule_11_5_violation] */
        ( void ) xEventGroupSetBits( pvEventGroup, ( EventBits_t ) ulBitsToSet );

        traceRETURN_vEventGroupSetBitsCallback();
    }
/*-----------------------------------------------------------*/

/* 仅供内部使用 - 执行从中断中挂起的“清除位”命令。 */
    void vEventGroupClearBitsCallback( void * pvEventGroup,
                                       uint32_t ulBitsToClear )
    {
        traceENTER_vEventGroupClearBitsCallback( pvEventGroup, ulBitsToClear );

        /* MISRA 参考 11.5.4 [回调函数参数] */
        /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-115 */
        /* coverity[misra_c_2012_rule_11_5_violation] */
        ( void ) xEventGroupClearBits( pvEventGroup, ( EventBits_t ) ulBitsToClear );

        traceRETURN_vEventGroupClearBitsCallback();
    }
/*-----------------------------------------------------------*/

    static BaseType_t prvTestWaitCondition( const EventBits_t uxCurrentEventBits,
                                            const EventBits_t uxBitsToWaitFor,
                                            const BaseType_t xWaitForAllBits )
    {
        BaseType_t xWaitConditionMet = pdFALSE;

        if( xWaitForAllBits == pdFALSE )
        {
            /* 任务只需等待 uxBitsToWaitFor 中的一个位被设置。是否已经设置了一个？ */
            if( ( uxCurrentEventBits & uxBitsToWaitFor ) != ( EventBits_t ) 0 )
            {
                xWaitConditionMet = pdTRUE;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* 任务必须等待 uxBitsToWaitFor 中的所有位被设置。它们是否都已设置？ */
            if( ( uxCurrentEventBits & uxBitsToWaitFor ) == uxBitsToWaitFor )
            {
                xWaitConditionMet = pdTRUE;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }

        return xWaitConditionMet;
    }
/*-----------------------------------------------------------*/

    #if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

        BaseType_t xEventGroupSetBitsFromISR( EventGroupHandle_t xEventGroup,
                                              const EventBits_t uxBitsToSet,
                                              BaseType_t * pxHigherPriorityTaskWoken )
        {
            BaseType_t xReturn;

            traceENTER_xEventGroupSetBitsFromISR( xEventGroup, uxBitsToSet, pxHigherPriorityTaskWoken );

            traceEVENT_GROUP_SET_BITS_FROM_ISR( xEventGroup, uxBitsToSet );
            xReturn = xTimerPendFunctionCallFromISR( vEventGroupSetBitsCallback, ( void * ) xEventGroup, ( uint32_t ) uxBitsToSet, pxHigherPriorityTaskWoken );

            traceRETURN_xEventGroupSetBitsFromISR( xReturn );

            return xReturn;
        }

    #endif /* if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) ) */
/*-----------------------------------------------------------*/

    #if ( configUSE_TRACE_FACILITY == 1 )

        UBaseType_t uxEventGroupGetNumber( void * xEventGroup )
        {
            UBaseType_t xReturn;

            /* MISRA 参考 11.5.2 [不透明指针] */
            /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-115 */
            /* coverity[misra_c_2012_rule_11_5_violation] */
            EventGroup_t const * pxEventBits = ( EventGroup_t * ) xEventGroup;

            traceENTER_uxEventGroupGetNumber( xEventGroup );

            if( xEventGroup == NULL )
            {
                xReturn = 0;
            }
            else
            {
                xReturn = pxEventBits->uxEventGroupNumber;
            }

            traceRETURN_uxEventGroupGetNumber( xReturn );

            return xReturn;
        }

    #endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

    #if ( configUSE_TRACE_FACILITY == 1 )

        void vEventGroupSetNumber( void * xEventGroup,
                                   UBaseType_t uxEventGroupNumber )
        {
            traceENTER_vEventGroupSetNumber( xEventGroup, uxEventGroupNumber );

            /* MISRA 参考 11.5.2 [不透明指针] */
            /* 更多细节请见: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-115 */
            /* coverity[misra_c_2012_rule_11_5_violation] */
            ( ( EventGroup_t * ) xEventGroup )->uxEventGroupNumber = uxEventGroupNumber;

            traceRETURN_vEventGroupSetNumber();
        }

    #endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

/* 如果应用程序未配置包含事件组功能，则此整个源文件将被跳过。
 * 如果要包含事件组，请确保在 FreeRTOSConfig.h 中将 configUSE_EVENT_GROUPS 设置为 1。 */
#endif /* configUSE_EVENT_GROUPS == 1 */

