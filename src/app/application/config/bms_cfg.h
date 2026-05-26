/**
 *
 * @copyright &copy; 2010 - 2026, Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 * (此处省略了冗长的BSD开源协议文本，该代码遵循BSD-3-Clause开源协议)
 */

/**
 * @file    bms_cfg.h
 * @author  foxBMS Team
 * @date    2020-02-24 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup ENGINE_CONFIGURATION
 * @prefix  BMS
 *
 * @brief   BMS 驱动配置头文件
 * @details TODO (原文档待补充详细描述)
 */

#ifndef FOXBMS__BMS_CFG_H_
#define FOXBMS__BMS_CFG_H_

/*========== 包含文件 =======================================================*/

#include "ftask_cfg.h" /* 包含任务调度配置，用于获取任务周期时间 */
#include <stdint.h>

/*========== 宏定义与类型声明 ===============================================*/

/** 预充过程的监控选项枚举 */
typedef enum {
    BS_PRECHARGE_MONITOR_CURRENT,             /*!< 仅监控电池电流 */
    BS_PRECHARGE_MONITOR_VOLTAGE,             /*!< 仅监控直流母线电压与电池电压的差值 */
    BS_PRECHARGE_MONITOR_CURRENT_AND_VOLTAGE, /*!< 同时监控电池电流和电压差值 */
} BS_PRECHARGE_MONITORING_e;

/** 无请求时发送消息的 ID */
#define BMS_REQ_ID_NOREQ (0u)

/** 通过 CAN 请求进入 STANDBY（待机）状态的 ID */
#define BMS_REQ_ID_STANDBY (3u)

/** 通过 CAN 请求进入 NORMAL（正常工作）状态的 ID */
#define BMS_REQ_ID_NORMAL (1u)

/** 通过 CAN 请求进入 CHARGE（充电）状态的 ID */
#define BMS_REQ_ID_CHARGE (2u)

/**
 * @brief   当寻找下一个可用电池串时，如果没有更多的串可用，
 *          函数将返回此值 */
#define BMS_NO_STRING_AVAILABLE (255u)

/**
 * @brief   BMS状态机任务运行周期 (TODO: 原文待补充)
 * @details 此宏必须表示运行 BMS 状态机上下文的任务周期时间。
 *          例如，如果 #BMS_Trigger() 运行在 10ms 任务中，则此宏必须设为 10。
 *          这设置了两次连续执行状态/子状态之间的最小时间间隔。
 *          此宏仅用于编译时断言检查，对实际代码没有程序运行时的动态影响。
 */
#define BMS_STATEMACHINE_TASK_CYCLE_CONTEXT_MS (10u)

/* 编译时断言：确保 BMS 状态机的周期时间与操作系统的 10ms 任务周期一致 */
#if BMS_STATEMACHINE_TASK_CYCLE_CONTEXT_MS != FTSK_TASK_CYCLIC_10MS_CYCLE_TIME
#error "Invalid BMS configuration. Make sure that BMS timing is configured correctly!"
#endif

/**
 * @brief   BMS 状态机短时间定义，表示需等待多少次 #BMS_Trigger() 调用
 *          后才处理下一个状态/子状态
 */
#define BMS_FSM_SHORTTIME (1u) /* 1 * 10ms = 10ms */

/**
 * @brief   BMS 状态机中等时间定义，表示需等待多少次 #BMS_Trigger() 调用
 *          后才处理下一个状态/子状态
 */
#define BMS_FSM_MEDIUMTIME (5u) /* 5 * 10ms = 50ms */

/**
 * @brief   BMS 状态机长时间定义，表示需等待多少次 #BMS_Trigger() 调用
 *          后才处理下一个状态/子状态
 */
#define BMS_FSM_LONGTIME (10u) /* 10 * 10ms = 100ms */

/** 闭合任何电池串负极或正极接触器后，需等待的时间（以 #BMS_Trigger() 调用次数计） */
#define BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR (20u) /* 20 * 10ms = 200ms */

/** 断开任何电池串负极或正极接触器后，需等待的时间（以 #BMS_Trigger() 调用次数计） */
#define BMS_WAIT_TIME_AFTER_OPENING_STRING_CONTACTOR (10u) /* 10 * 10ms = 100ms */

/** 闭合完整的电池串之间需等待的时间（以 #BMS_Trigger() 调用次数计） */
#define BMS_WAIT_TIME_BETWEEN_CLOSING_STRINGS (10u) /* 10 * 10ms = 100ms */

/**
 * @brief   闭合电池串时的超时时间（以 #BMS_Trigger() 调用次数计）。
 *          如果超时后该串仍未闭合，状态机将进入错误状态。
 */
#define BMS_STRING_CLOSE_TIMEOUT (500u) /* 500 * 10ms = 5000ms = 5s */

/**
 * @brief   断开电池串时的超时时间（以 #BMS_Trigger() 调用次数计）。
 *          如果超时后该串仍未断开，状态机将进入错误状态。
 */
#define BMS_STRING_OPEN_TIMEOUT (1000u) /* 1000 * 10ms = 10000ms = 10s */

/**
 * @brief   允许闭合下一个电池串的最大电压差（单位：mV）。
 *          只有当两串之间的电压差低于此值时，才允许闭合，防止产生巨大环流。
 */
#define BMS_NEXT_STRING_VOLTAGE_LIMIT_MV (3000) /* 3000mV = 3V */

/** 允许闭合下一个电池串的最大平均串电流（单位：mA） */
#define BMS_AVERAGE_STRING_CURRENT_LIMIT_MA (20000) /* 20000mA = 20A */

/** 断开预充接触器后的延迟时间（以 #BMS_Trigger() 调用次数计） */
#define BMS_TIME_WAIT_AFTER_OPENING_PRECHARGE (50u) /* 50 * 10ms = 500ms */

/**
 * @brief   预充失败导致预充断开后，需等待的时间（以 #BMS_Trigger() 调用次数计）
 */
#define BMS_TIME_WAIT_AFTER_PRECHARGE_FAIL (300u) /* 300 * 10ms = 3000ms = 3s */

/**
 * @brief   重新进入预充状态前需等待的超时时间（单位：1*10ms）
 * @details 防止在控制单元发送错误状态请求时，接触器发生机械上的快速闭合/断开循环（防抖/防振荡）。
 */
#define BMS_OSCILLATION_TIMEOUT (1000u) /* 1000 * 10ms = 10s */

/** 
 * 选择用于监控预充过程的参数。
 *  可选选项为：
 *  - 监控电池电流（电流低于 #BMS_PRECHARGE_CURRENT_THRESHOLD_mA）
 *  - 监控直流母线电压（电池电压与直流母线电压差低于 #BMS_PRECHARGE_VOLTAGE_THRESHOLD_mV）
 *  - 同时监控电池电流和直流母线电压
 *  当前配置：同时监控电流和电压
 */
#define BMS_PRECHARGE_MONITORING_PARAMETERS (BS_PRECHARGE_MONITOR_CURRENT_AND_VOLTAGE)

/** 允许尝试闭合接触器（预充）的最大次数 */
#define BMS_PRECHARGE_TRIES (3u)

/** 预充电压阈值限制（单位：mV），压差小于此值认为预充完成 */
#define BMS_PRECHARGE_VOLTAGE_THRESHOLD_mV (1000LL) /* 1000mV = 1V, LL后缀表示long long类型防止溢出 */

/** 预充电流阈值限制（单位：mA），电流小于此值认为预充完成 */
#define BMS_PRECHARGE_CURRENT_THRESHOLD_mA (50) /* 50mA */

/** 
 * 预充超时：从发出闭合预充接触器请求开始，预充过程允许的最大持续时间（单位：ms）。
 * 超过此时间未完成预充则判定为预充超时失败。
 */
#define BMS_MAXIMUM_PRECHARGE_DURATION_ms (2000u) /* 2000ms = 2s */

/**
 * @details 预充失败导致接触器断开后，需等待的时间
 *          （以 #BMS_Trigger() 调用次数计）
 */
#define BMS_FSM_TIME_UNTIL_PRECHARGE_FAIL (100u) /* 100 * 10ms = 1000ms = 1s */

/**
 * @details 闭合预充接触器时的超时时间（以 #BMS_Trigger() 调用次数计）。
 *          如果超时后预充接触器仍未闭合，状态机将进入错误状态。
 */
#define BMS_PRECHARGE_CLOSE_TIMEOUT (500u) /* 500 * 10ms = 5000ms = 5s */

/**
 * @details 断开预充接触器时的超时时间（以 #BMS_Trigger() 调用次数计）。
 *          如果超时后预充接触器仍未断开，状态机将进入错误状态。
 */
#define BMS_PRECHARGE_OPEN_TIMEOUT (500u) /* 500 * 10ms = 5000ms = 5s */

/*========== 外部常量和变量声明 =============================================*/

/*========== 外部函数原型 ===================================================*/

/*========== 外部化的静态函数原型 (用于单元测试) =============================*/
#ifdef UNITY_UNIT_TEST
#endif

#endif /* FOXBMS__BMS_CFG_H_ */
