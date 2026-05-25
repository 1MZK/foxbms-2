/**
 *
 * @copyright &copy; 2010 - 2026, Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 * 版权所有。
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * 在满足以下条件的前提下，允许以源代码和二进制形式进行重新分发和使用，无论是否经过修改：
 *
 * 1. 源代码的重新分发必须保留上述版权声明、此条件列表以及下述免责声明。
 *
 * 2. 二进制形式的重新分发必须在随分发提供的文档和/或其他材料中复制上述版权声明、
 *    此条件列表以及下述免责声明。
 *
 * 3. 未经特定事先书面许可，版权持有者的名称及其贡献者的名称不得用于支持或推广
 *    由本软件派生的产品。
 *
 * 本软件由版权持有者和贡献者“按原样”提供，不提供任何明示或暗示的保证，包括但不仅限于
 * 对适销性和特定用途适用性的暗示保证。在任何情况下，版权持有者或贡献者对任何直接、间接、
 * 偶然、特殊、惩罚性或后果性损害（包括但不仅限于替代商品或服务的采购、使用、数据或
 * 利润的损失或业务中断）不承担责任，无论其基于何种责任理论，无论是合同责任、严格责任
 * 或侵权（包括疏忽或其他），即使已被告知可能发生此类损害，也是如此。
 *
 * 我们恳请您在您的硬件、软件、文档或广告材料中使用以下一个或多个短语来指代 foxBMS：
 *
 * - "本产品使用了 foxBMS&reg; 的部分内容"
 * - "本产品包含了 foxBMS&reg; 的部分内容"
 * - "本产品派生自 foxBMS&reg;"
 *
 */

/**
 * @file    ftask_cfg.h
 * @author  foxBMS 团队
 * @date    2019-08-26 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup TASK_CONFIGURATION
 * @prefix  FTSK
 *
 * @brief   任务配置头文件
 * @details TODO
 */

#ifndef FOXBMS__FTASK_CFG_H_
#define FOXBMS__FTASK_CFG_H_

/*========== 包含文件 =======================================================*/
#include "foxbms_config.h"

#include "os.h"

#include <stdint.h>

/*========== 宏和定义 =========================================================*/
/** @brief 引擎任务的栈大小 */
#define FTSK_TASK_ENGINE_STACK_SIZE_IN_BYTES (1024u)

/** @brief 引擎任务的优先级 */
#define FTSK_TASK_ENGINE_PRIORITY (OS_PRIORITY_REAL_TIME)

/** @brief 引擎任务的相位 */
#define FTSK_TASK_ENGINE_PHASE (0u)

/** @brief 引擎任务的周期时间 */
#define FTSK_TASK_ENGINE_CYCLE_TIME (1u)

/** @brief 引擎任务允许的最大抖动 */
#define FTSK_TASK_ENGINE_MAXIMUM_JITTER (1u)

/** @brief 引擎任务的 pvParameters 参数 */
#define FTSK_TASK_ENGINE_PV_PARAMETERS (NULL_PTR)

/** @brief 周期 1ms 任务的栈大小 */
#define FTSK_TASK_CYCLIC_1MS_STACK_SIZE_IN_BYTES (1024u)

/** @brief 周期 1ms 任务的优先级 */
#define FTSK_TASK_CYCLIC_1MS_PRIORITY (OS_PRIORITY_VERY_HIGH)

/** @brief 周期 1ms 任务的相位 */
#define FTSK_TASK_CYCLIC_1MS_PHASE (0u)

/** @brief 1ms 任务的周期时间 */
#define FTSK_TASK_CYCLIC_1MS_CYCLE_TIME (1u)

/** @brief 1ms 任务允许的最大抖动 */
#define FTSK_TASK_CYCLIC_1MS_MAXIMUM_JITTER (1u)

/** @brief 1ms 任务的 pvParameters 参数 */
#define FTSK_TASK_CYCLIC_1MS_PV_PARAMETERS (NULL_PTR)

/** @brief 周期 10ms 任务的栈大小 */
#define FTSK_TASK_CYCLIC_10MS_STACK_SIZE_IN_BYTES (5120u)

/** @brief 周期 10ms 任务的优先级 */
#define FTSK_TASK_CYCLIC_10MS_PRIORITY (OS_PRIORITY_HIGH)

/** @brief 周期 10ms 任务的相位 */
#define FTSK_TASK_CYCLIC_10MS_PHASE (2u)

/** @brief 10ms 任务的周期时间 */
#define FTSK_TASK_CYCLIC_10MS_CYCLE_TIME (10u)

/** @brief 10ms 任务允许的最大抖动 */
#define FTSK_TASK_CYCLIC_10MS_MAXIMUM_JITTER (2u)

/** @brief 10ms 任务的 pvParameters 参数 */
#define FTSK_TASK_CYCLIC_10MS_PV_PARAMETERS (NULL_PTR)

/** @brief 周期 100ms 任务的栈大小 */
#define FTSK_TASK_CYCLIC_100MS_STACK_SIZE_IN_BYTES (1024u)

/** @brief 周期 100ms 任务的优先级 */
#define FTSK_TASK_CYCLIC_100MS_PRIORITY (OS_PRIORITY_ABOVE_NORMAL)

/** @brief 周期 100ms 任务的相位 */
#define FTSK_TASK_CYCLIC_100MS_PHASE (56u)

/** @brief 100ms 任务的周期时间 */
#define FTSK_TASK_CYCLIC_100MS_CYCLE_TIME (100u)

/** @brief 100ms 任务允许的最大抖动 */
#define FTSK_TASK_CYCLIC_100MS_MAXIMUM_JITTER (5u)

/** @brief 100ms 任务的 pvParameters 参数 */
#define FTSK_TASK_CYCLIC_100MS_PV_PARAMETERS (NULL_PTR)

/** @brief 用于算法的周期 100ms 任务的栈大小 */
#define FTSK_TASK_CYCLIC_ALGORITHM_100MS_STACK_SIZE_IN_BYTES (1024u)

/** @brief 用于算法的周期 100ms 任务的优先级 */
#define FTSK_TASK_CYCLIC_ALGORITHM_100MS_PRIORITY (OS_PRIORITY_NORMAL)

/** @brief 用于算法的周期 100ms 任务的相位 */
#define FTSK_TASK_CYCLIC_ALGORITHM_100MS_PHASE (64u)

/** @brief 用于算法的 100ms 任务的周期时间 */
#define FTSK_TASK_CYCLIC_ALGORITHM_100MS_CYCLE_TIME (100u)

/** @brief 用于算法的 100ms 任务允许的最大抖动 */
#define FTSK_TASK_CYCLIC_ALGORITHM_100MS_MAXIMUM_JITTER (5u)

/** @brief 用于算法的 100ms 任务的 pvParameters 参数 */
#define FTSK_TASK_CYCLIC_ALGORITHM_100MS_PV_PARAMETERS (NULL_PTR)

/** @brief 连续运行的 I2C 任务的栈大小 */
#define FTSK_TASK_I2C_STACK_SIZE_IN_BYTES (2048u)

/** @brief 连续运行的 I2C 任务的优先级 */
#define FTSK_TASK_I2C_PRIORITY (FTSK_TASK_CYCLIC_10MS_PRIORITY)

/** @brief 连续运行的 I2C 任务的相位 */
#define FTSK_TASK_I2C_PHASE (0u)

/** @brief 连续运行的 I2C 任务的周期时间 */
#define FTSK_TASK_I2C_CYCLE_TIME (0u)

/** @brief 连续运行的 I2C 任务的 pvParameters 参数 */
#define FTSK_TASK_I2C_PV_PARAMETERS (NULL_PTR)

#if (FOXBMS_AFE_DRIVER_TYPE_NO_FSM == 1)
/** @brief 连续运行的 AFE 任务的栈大小 */
#define FTSK_TASK_AFE_STACK_SIZE_IN_BYTES (4096u)

/** @brief 连续运行的 AFE 任务的优先级 */
#define FTSK_TASK_AFE_PRIORITY (OS_PRIORITY_ABOVE_HIGH)

/** @brief 连续运行的 AFE 任务的相位 */
#define FTSK_TASK_AFE_PHASE (0u)

/** @brief 连续运行的 AFE 任务的周期时间 */
#define FTSK_TASK_AFE_CYCLE_TIME (0u)

/** @brief 连续运行的 AFE 任务的 pvParameters 参数 */
#define FTSK_TASK_AFE_PV_PARAMETERS (NULL_PTR)
#endif

#if defined(FOXBMS_UART_SUPPORT) && FOXBMS_UART_SUPPORT == 1
/** @brief 连续运行的 UART 任务的栈大小 */
#define FTSK_TASK_UART_STACK_SIZE_IN_BYTES (1024u)

/** @brief 连续运行的 UART 任务的优先级 */
#define FTSK_TASK_UART_PRIORITY (OS_PRIORITY_NORMAL)

/** @brief 连续运行的 UART 任务的相位 */
#define FTSK_TASK_UART_PHASE (0u)

/** @brief 连续运行的 UART 任务的周期时间 */
#define FTSK_TASK_UART_CYCLE_TIME (0u)

/** @brief 连续运行的 UART 任务的 pvParameters 参数 */
#define FTSK_TASK_UART_PV_PARAMETERS (NULL_PTR)
#endif

#if (defined(FOXBMS_TCP_SUPPORT) && (FOXBMS_TCP_SUPPORT == 1))
/** @brief EMAC 任务的栈大小 */
#define FTSK_TASK_EMAC_STACK_SIZE_IN_BYTES (2048u)

/** @brief EMAC 任务的优先级 */
#define FTSK_TASK_EMAC_PRIORITY (OS_PRIORITY_ABOVE_NORMAL)

/** @brief EMAC 任务的相位 */
#define FTSK_TASK_EMAC_PHASE (10u)

/** @brief EMAC 任务的周期时间 */
#define FTSK_TASK_EMAC_CYCLE_TIME (0u)

/** @brief EMAC 任务的 pvParameters 参数 */
#define FTSK_TASK_EMAC_PV_PARAMETERS (NULL_PTR)
#endif

/*========== 外部常量和变量声明 ======================*/
/**
 * @brief   引擎任务的任务配置
 * @details 用于数据库和系统监控的任务
 */
extern OS_TASK_DEFINITION_s ftsk_taskDefinitionEngine;

/**
 * @brief   周期 1ms 任务的任务配置
 * @details 周期 1ms 任务
 */
extern OS_TASK_DEFINITION_s ftsk_taskDefinitionCyclic1ms;

/**
 * @brief   周期 10ms 任务的任务配置
 * @details 周期 10ms 任务
 */
extern OS_TASK_DEFINITION_s ftsk_taskDefinitionCyclic10ms;

/**
 * @brief   周期 100ms 任务的任务配置
 * @details 周期 100ms 任务
 */
extern OS_TASK_DEFINITION_s ftsk_taskDefinitionCyclic100ms;

/**
 * @brief   用于算法的周期 100ms 任务的任务配置
 * @details 用于算法的周期 100ms 任务
 */
extern OS_TASK_DEFINITION_s ftsk_taskDefinitionCyclicAlgorithm100ms;

/**
 * @brief   用于 MCU I2C 通信的连续运行任务的任务配置
 * @details 用于 MCU I2C 通信的连续运行任务
 */
extern OS_TASK_DEFINITION_s ftsk_taskDefinitionI2c;

#if (FOXBMS_AFE_DRIVER_TYPE_NO_FSM == 1)
/**
 * @brief   用于 AFE 的连续运行任务的任务配置
 * @details 用于 AFE 的连续运行任务
 */
extern OS_TASK_DEFINITION_s ftsk_taskDefinitionAfe;
#endif

#if defined(FOXBMS_UART_SUPPORT) && FOXBMS_UART_SUPPORT == 1
/**
 * @brief   用于 UART 流控制的任务的任务配置
 * @details 用于 UART 流控制处理的任务
 */
extern OS_TASK_DEFINITION_s ftsk_taskDefinitionUart;
#endif

#if (defined(FOXBMS_TCP_SUPPORT) && (FOXBMS_TCP_SUPPORT == 1))
/**
 * @brief   用于 MCU EMAC 通信的任务的任务配置
 * @details 用于 MCU EMAC 通信的任务
 */
extern OS_TASK_DEFINITION_s ftsk_taskDefinitionEmac;
#endif

/**
 * @brief 任务句柄定义
 */
extern OS_TASK_HANDLE ftsk_taskHandleI2c;

#if (FOXBMS_AFE_DRIVER_TYPE_NO_FSM == 1)
/**
 * @brief 任务句柄定义
 */
extern OS_TASK_HANDLE ftsk_taskHandleAfe;
#endif

#if defined(FOXBMS_UART_SUPPORT) && FOXBMS_UART_SUPPORT == 1
/**
 * @brief 任务句柄定义
 */
extern OS_TASK_HANDLE ftsk_taskHandleUart;
#endif

#if (defined(FOXBMS_TCP_SUPPORT) && (FOXBMS_TCP_SUPPORT == 1))
/**
 * @brief 任务句柄定义
 */

extern OS_TASK_HANDLE ftsk_taskHandleEmac;
#endif

/*========== 外部函数原型 =====================================*/
/**
 * @brief   初始化数据库
 * @details 调度器启动后的启动操作
 * @warning 不要更改此函数的内容。这极有可能破坏系统。此函数保留在配置文件中
 *          是为了拥有统一的任务配置。
 */
extern void FTSK_InitializeUserCodeEngine(void);

/**
 * @brief   用于数据库和系统监控模块的引擎任务
 * @details 调度器启动后的启动操作。第一个运行的任务，所有其他任务仅在此任务
 *          启动后才会启动
 * @warning 不要更改此函数的内容。这极有可能破坏系统。此函数保留在配置文件中
 *          是为了拥有统一的任务配置。
 */
extern void FTSK_RunUserCodeEngine(void);

/**
 * @brief   所有任务启动前的初始化函数
 * @details 此函数在调度器启动后但在任何周期任务运行前调用。在此处初始化在
 *          启动过程中未使用的模块。
 */
extern void FTSK_InitializeUserCodePreCyclicTasks(void);

/**
 * @brief   周期 1ms 任务
 * @details TODO
 */
extern void FTSK_RunUserCodeCyclic1ms(void);

/**
 * @brief   周期 10ms 任务
 * @details TODO
 */
extern void FTSK_RunUserCodeCyclic10ms(void);

/**
 * @brief   周期 100ms 任务
 * @details TODO
 */
extern void FTSK_RunUserCodeCyclic100ms(void);

/**
 * @brief   用于算法的周期 100ms 任务
 * @details TODO
 */
extern void FTSK_RunUserCodeCyclicAlgorithm100ms(void);

/**
 * @brief   连续运行的 I2C 任务
 * @details 实现 MCU 通过 I2C 的通信
 */
extern void FTSK_RunUserCodeI2c(void);

#if (FOXBMS_AFE_DRIVER_TYPE_NO_FSM == 1)
/**
 * @brief   用于 AFE 的连续运行任务
 * @details 实现与无状态机的 AFE 的通信。
 */
extern void FTSK_RunUserCodeAfe(void);
#endif

#if defined(FOXBMS_UART_SUPPORT) && FOXBMS_UART_SUPPORT == 1
/**
 * @brief   用于 UART 的连续运行任务
 * @details 实现 UART 的软件流控制
 */
extern void FTSK_RunUserCodeUart(void);
#endif

#if (defined(FOXBMS_TCP_SUPPORT) && (FOXBMS_TCP_SUPPORT == 1))
/**
 * @brief   用于 EMAC 的连续运行任务
 * @details 实现 MCU 通过 EMAC 的 TCP 通信
 */
extern void FTSK_RunUserCodeEmac(void);
#endif

/**
 * @brief   空闲任务
 * @details 如果 FreeRTOSConfig.h 中的 configUSE_IDLE_HOOK 被启用，则由
 *          #vApplicationIdleHook() 调用。如果不需要此钩子，可以在 FreeRTOS
 *          配置中禁用它。
 */
extern void FTSK_RunUserCodeIdle(void);

/*========== 外部化的静态函数原型 (单元测试) ===========*/
#ifdef UNITY_UNIT_TEST
#endif

#endif /* FOXBMS__FTASK_CFG_H_ */

