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
 * @file    bms.h
 * @author  foxBMS 团队
 * @date    2020-02-24 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup ENGINE
 * @prefix  BMS
 *
 * @brief   BMS 驱动头文件
 * @details TODO
 */

#ifndef FOXBMS__BMS_H_
#define FOXBMS__BMS_H_

/*========== 包含文件 =======================================================*/
#include "battery_system_cfg.h"
#include "bms_cfg.h"

#include "contactor.h"
#include "fstd_types.h"

#include <stdbool.h>
#include <stdint.h>

/*========== 宏和定义 =========================================================*/

/** 电池系统状态的符号名称 */
typedef enum {
    BMS_CHARGING,    /*!< 电池正在充电 */
    BMS_DISCHARGING, /*!< 电池正在放电 */
    BMS_RELAXATION,  /*!< 电池正在弛豫（电流停止后的电压恢复过程） */
    BMS_AT_REST,     /*!< 电池处于静置状态 */
} BMS_CURRENT_FLOW_STATE_e;

/** BMS 控制忙碌状态的符号名称 */
typedef enum {
    BMS_CHECK_OK,     /*!< BMS 控制正常 */
    BMS_CHECK_BUSY,   /*!< BMS 控制忙碌 */
    BMS_CHECK_NOT_OK, /*!< BMS 控制异常 */
} BMS_CHECK_e;

/** 是否考虑预充电的符号名称 */
typedef enum {
    BMS_DO_NOT_TAKE_PRECHARGE_INTO_ACCOUNT, /*!< 不考虑预充电 */
    BMS_TAKE_PRECHARGE_INTO_ACCOUNT,        /*!< 考虑预充电 */
} BMS_CONSIDER_PRECHARGE_e;

/** 预充电过程结果 */
typedef enum {
    BMS_PRECHARGING_ONGOING,          /*!< 预充电正在进行 */
    BMS_PRECHARGING_FAILED,           /*!< 预充电失败 */
    BMS_PRECHARGING_FINISHED,         /*!< 预充电完成 */
    BMS_PRECHARING_HAS_NOT_STARTED,   /*!< 预充电尚未开始 */
} BMS_RESULT_PRECHARGE_PROCESS_e;

/** BMS 状态机的状态 */
typedef enum {
    /* 初始化序列 */
    BMS_FSM_STATE_UNINITIALIZED,  /*!< 未初始化 */
    BMS_FSM_STATE_INITIALIZATION, /*!< 初始化中 */
    BMS_FSM_STATE_INITIALIZED,    /*!< 已初始化 */
    BMS_FSM_STATE_IDLE,           /*!< 空闲 */
    BMS_FSM_STATE_OPEN_CONTACTORS,/*!< 断开接触器 */
    BMS_FSM_STATE_STANDBY,        /*!< 待机 */
    BMS_FSM_STATE_PRECHARGE,      /*!< 预充电 */
    BMS_FSM_STATE_NORMAL,         /*!< 正常工作 */
    BMS_FSM_STATE_DISCHARGE,      /*!< 放电 */
    BMS_FSM_STATE_CHARGE,         /*!< 充电 */
    BMS_FSM_STATE_ERROR,          /*!< 错误 */
    BMS_FSM_STATE_UNDEFINED,      /*!< 未定义 */
    BMS_FSM_STATE_RESERVED1,      /*!< 保留 */
} BMS_FSM_STATES_e;

/** BMS 状态机的子状态 */
typedef enum {
    BMS_FSM_SUBSTATE_ENTRY,                       /*!< 子状态入口状态 */
    BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_INTERLOCK, /*!< 子状态：联锁闭合后检查测量值 */
    BMS_FSM_SUBSTATE_INTERLOCK_CHECKED,           /*!< 子状态：已检查联锁 */
    BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS,        /*!< 子状态：检查是否有状态请求 */
    BMS_FSM_SUBSTATE_CHECK_BALANCING_REQUESTS,    /*!< 子状态：检查是否有均衡请求 */
    BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS,           /*!< 子状态：检查是否设置了任何错误标志 */
    BMS_FSM_SUBSTATE_CHECK_CONTACTOR_NORMAL_STATE, /*!< 子状态：预充中，检查接触器是否达到正常状态 */
    BMS_FSM_SUBSTATE_CHECK_CONTACTOR_CHARGE_STATE, /*!< 子状态：预充中，检查接触器是否达到充电正常状态 */
    BMS_FSM_SUBSTATE_PRECHARGE_CLOSE_MINUS,       /*!< 子状态：预充闭合负极接触器 */
    BMS_FSM_SUBSTATE_PRECHARGE_CLOSE_PRECHARGE,   /*!< 子状态：预充闭合预充接触器 */
    BMS_FSM_SUBSTATE_PRECHARGE_CHECK_PRECHARGE_PROCESS, /*!< 子状态：预充检查预充过程 */
    BMS_FSM_SUBSTATE_PRECHARGE_OPEN_PRECHARGE,    /*!< 子状态：预充断开预充接触器 */
    BMS_FSM_SUBSTATE_PRECHARGE_CHECK_OPEN_PRECHARGE, /*!< 子状态：预充检查预充接触器是否断开 */
    BMS_FSM_SUBSTATE_OPEN_FIRST_CONTACTOR,        /*!< 子状态：断开第一个接触器 */
    BMS_FSM_SUBSTATE_OPEN_SECOND_CONTACTOR_MINUS, /*!< 子状态：断开第二个接触器（负极） */
    BMS_FSM_SUBSTATE_OPEN_SECOND_CONTACTOR_PLUS,  /*!< 子状态：断开第二个接触器（正极） */
    BMS_FSM_SUBSTATE_CHECK_CLOSE_SECOND_STRING_CONTACTOR_PRECHARGE_STATE, /*!< 子状态：预充状态下检查闭合第二个串接触器 */
    BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE, /*!< 子状态：预充检查错误标志 */
    BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_FIRST_STRING, /*!< 子状态：预充检查第一串的错误标志 */
    BMS_FSM_SUBSTATE_PRECHARGE_CLOSE_NEXT_STRING, /*!< 子状态：预充闭合下一个串 */
    BMS_FSM_SUBSTATE_CLOSE_SECOND_CONTACTOR_PLUS, /*!< 子状态：闭合第二个接触器（正极） */
    BMS_FSM_SUBSTATE_CHECK_STRING_CLOSED,         /*!< 子状态：检查串是否闭合 */
    BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_CLOSING_STRINGS, /*!< 子状态：预充闭合串时检查错误标志 */
    BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_CLOSING_PRECHARGE, /*!< 子状态：闭合预充时检查错误标志 */
    BMS_FSM_SUBSTATE_NORMAL_CLOSE_NEXT_STRING,    /*!< 子状态：正常模式下闭合下一个串 */
    BMS_FSM_SUBSTATE_NORMAL_CLOSE_SECOND_STRING_CONTACTOR, /*!< 子状态：正常模式下闭合第二个串接触器 */
    BMS_FSM_SUBSTATE_OPEN_ALL_PRECHARGE_CONTACTORS, /*!< 子状态：断开所有预充接触器 */
    BMS_FSM_SUBSTATE_CHECK_ALL_PRECHARGE_CONTACTORS_OPEN, /*!< 子状态：检查所有预充接触器是否断开 */
    BMS_FSM_SUBSTATE_OPEN_STRINGS_ENTRY,          /*!< 子状态：断开串入口 */
    BMS_FSM_SUBSTATE_OPEN_FIRST_STRING_CONTACTOR, /*!< 子状态：断开第一个串接触器 */
    BMS_FSM_SUBSTATE_OPEN_SECOND_STRING_CONTACTOR,/*!< 子状态：断开第二个串接触器 */
    BMS_FSM_SUBSTATE_CHECK_SECOND_STRING_CONTACTOR, /*!< 子状态：检查第二个串接触器 */
    BMS_FSM_SUBSTATE_HANDLE_SUPPLY_VOLTAGE_30C_LOSS, /*!< 子状态：处理 30C 电源电压丢失 */
    BMS_FSM_SUBSTATE_OPEN_STRINGS_EXIT,           /*!< 子状态：断开串退出 */
} BMS_FSM_SUB_e;

/** BMS 状态机的 CAN 状态 */
typedef enum {
    BMS_CAN_STATE_UNINITIALIZED,  /*!< CAN 状态：未初始化 */
    BMS_CAN_STATE_INITIALIZATION, /*!< CAN 状态：初始化中 */
    BMS_CAN_STATE_INITIALIZED,    /*!< CAN 状态：已初始化 */
    BMS_CAN_STATE_IDLE,           /*!< CAN 状态：空闲 */
    BMS_CAN_STATE_OPEN_CONTACTORS,/*!< CAN 状态：断开接触器 */
    BMS_CAN_STATE_STANDBY,        /*!< CAN 状态：待机 */
    BMS_CAN_STATE_PRECHARGE,      /*!< CAN 状态：预充电 */
    BMS_CAN_STATE_NORMAL,         /*!< CAN 状态：正常工作 */
    BMS_CAN_STATE_CHARGE,         /*!< CAN 状态：充电 */
    BMS_CAN_STATE_ERROR,          /*!< CAN 状态：错误 */
} BMS_CAN_STATE_e;

/** BMS 状态机的状态请求 */
typedef enum {
    BMS_STATE_INITIALIZATION_REQUEST, /*!< 请求初始化 */
    BMS_STATE_ERROR_REQUEST,          /*!< 请求进入错误状态 */
    BMS_STATE_NO_REQUEST,             /*!< 无请求的虚拟请求 */
} BMS_STATE_REQUEST_e;

/** 向 BMS 状态机发出状态请求时可能的返回值 */
typedef enum {
    BMS_OK,                  /*!< 请求成功 */
    BMS_REQUEST_PENDING,     /*!< 错误：当前正在处理另一个请求 */
    BMS_ILLEGAL_REQUEST,     /*!< 错误：请求无法执行 */
    BMS_ALREADY_INITIALIZED, /*!< 错误：BMS 状态机已初始化 */
} BMS_RETURN_TYPE_e;

/** 功率路径类型（放电或充电） */
typedef enum {
    BMS_POWER_PATH_OPEN, /* 接触器断开 */
    BMS_POWER_PATH_0,    /* 功率路径 0 */
    BMS_POWER_PATH_1,    /* 功率路径 1 */
} BMS_POWER_PATH_TYPE_e;

/**
 * 此结构体包含与 CONT 状态机相关的所有变量。
 * 用户可以通过此变量获取 CONT 状态机的当前状态
 */
typedef struct {
    uint32_t currentSystick; /*!< 当前系统时间戳。每次调用 #BMS_Trigger 时更新 */
    uint16_t timer;          /*!< 状态机处理下一个状态前的时间（毫秒），例如以 1ms 为计数单位 */
    BMS_STATE_REQUEST_e stateRequest;           /*!< 向状态机发出的当前状态请求 */
    BMS_FSM_STATES_e state;                     /*!< 状态机的当前状态 */
    BMS_FSM_SUB_e substate;                     /*!< 状态机的当前子状态 */
    BMS_FSM_STATES_e lastState;                 /*!< 状态机的前一个状态 */
    BMS_FSM_SUB_e lastSubstate;                 /*!< 状态机的前一个子状态 */
    uint32_t ErrRequestCounter;                 /*!< 对 AFE 状态机的非法请求计数 */
    STD_RETURN_TYPE_e initFinished;             /*!< 如果初始化通过则为 #STD_OK，否则为 #STD_NOT_OK */
    uint8_t triggerentry;                       /*!< 重入保护计数器（函数运行标志） */
    uint8_t counter;                            /*!< 通用计数器 */
    BMS_CURRENT_FLOW_STATE_e currentFlowState;  /*!< 电池系统状态 */
    uint32_t restTimer_10ms;                    /*!< 电池系统静置前的计时器 */
    uint16_t OscillationTimeout;                /*!< 防止接触器振荡的超时时间 */
    uint8_t prechargeTryCounter;                /*!< 防止接触器振荡的超时时间（预充尝试计数） */
    BMS_POWER_PATH_TYPE_e powerPath;            /*!< 功率路径类型（放电或充电） */
    uint8_t numberOfClosedStrings;              /*!< 已闭合的串数量 */
    uint16_t stringOpenTimeout;                 /*!< 串断开时间过长时的中止超时 */
    uint32_t nextStringClosedTimer;             /*!< 下一个串闭合后的等待计时器 */
    uint16_t stringCloseTimeout;                /*!< 串闭合时间过长时的中止超时 */
    BMS_FSM_STATES_e nextState;                 /*!< 状态机的下一个状态 */
    uint8_t firstClosedString;                  /*!< 最先闭合的串（具有最高或最低电压） */
    uint16_t prechargeOpenTimeout;              /*!< 串断开时间过长时的中止超时（预充） */
    uint16_t prechargeCloseTimeout;             /*!< 串闭合时间过长时的中止超时（预充） */
    uint32_t remainingDelay_ms;                 /*!< 状态机切换到错误状态前的剩余时间 */
    uint32_t minimumActiveDelay_ms;             /*!< 所有活动致命错误的最小延迟时间 */
    uint32_t timeAboveContactorBreakCurrent_ms; /*!< 电流超过接触器最大分断电流的持续时间 */
    uint8_t stringToBeOpened;                   /*!< 当前正在断开的串 */
    CONT_TYPE_e contactorToBeOpened;            /*!< 当前正在断开的接触器 */
    uint32_t startOfPrecharging;                /*!< 预充电启动时的系统滴答时间 */
    bool transitionToErrorState;                /*!< 是否检测到致命错误且延迟处于活动状态的标志 */
    bool closedPrechargeContactors[BS_NR_OF_STRINGS]; /*!< 预充接触器已闭合的串 */
    bool closedStrings[BS_NR_OF_STRINGS];             /*!< 接触器已闭合的串 */
    bool deactivatedStrings[BS_NR_OF_STRINGS]; /*!< 检测到错误后停用的串，无法闭合 */
} BMS_STATE_s;

/*========== 外部常量和变量声明 ======================*/

/*========== 外部函数原型 =====================================*/
/**
 * @brief   设置状态变量 bms_state 的当前状态请求。
 * @details 此函数用于向状态机发出状态请求，例如，启动电压测量、读取电压测量结果、重新初始化。
 *          它调用 #BMS_CheckStateRequest() 来检查请求是否有效。
 *          如果请求无效，则会被拒绝。检查结果会立即返回，以便请求方在发出无效状态请求时能够采取措施。
 * @param   statereq    要设置的状态请求
 * @return  当前状态请求
 */
extern BMS_RETURN_TYPE_e BMS_SetStateRequest(BMS_STATE_REQUEST_e statereq);

/**
 * @brief   返回当前状态。
 * @details 此函数用于 SYS 状态机的运行。
 * @return  当前状态，取自 BMS_FSM_STATES_e
 */
extern BMS_FSM_STATES_e BMS_GetState(void);

/**
 * @brief   返回当前子状态。
 * @details 此函数用于 SYS 状态机的运行。
 * @return  当前子状态，取自 BMS_FSM_SUB_e
 */
extern BMS_FSM_SUB_e BMS_GetSubstate(void);

/**
 * @brief   获取初始化状态。
 * @details 此函数用于获取 BMS 的初始化状态。
 * @return  如果已初始化则返回 #STD_OK，否则返回 #STD_NOT_OK
 */
extern STD_RETURN_TYPE_e BMS_GetInitializationState(void);

/**
 * @brief   BMS 驱动状态机的触发函数。
 * @details 此函数包含 BMS 状态机中的事件序列。
 *          它必须以时间触发方式调用，每 10 毫秒调用一次。
 *          需要对此函数进行适配，以适应电池系统为目标应用提供的行为。
 */
extern void BMS_Trigger(void);

/**
 * @brief   返回当前电池系统状态（充电/放电、静置或弛豫阶段）
 *
 * @return  #BMS_CURRENT_FLOW_STATE_e
 */
extern BMS_CURRENT_FLOW_STATE_e BMS_GetBatterySystemState(void);

/**
 * @brief   获取电流流向，电流值作为函数参数
 * @param[in]   current_mA 流过的电流
 * @return  根据电流方向返回 #BMS_DISCHARGING 或 #BMS_CHARGING。
 *          如果无电流则返回 #BMS_AT_REST。(类型: #BMS_CURRENT_FLOW_STATE_e)
 */
extern BMS_CURRENT_FLOW_STATE_e BMS_GetCurrentFlowDirection(int32_t current_mA);

/**
 * @brief   返回串状态（闭合或断开）
 * @param[in]   stringNumber   寻址的串
 * @return  如果串断开则返回 false，如果串闭合则返回 true
 */
extern bool BMS_IsStringClosed(uint8_t stringNumber);

/**
 * @brief   返回串当前是否正在预充电
 * @param[in]   stringNumber   寻址的串
 * @return  如果预充接触器断开则返回 false，如果闭合且串正在预充电则返回 true
 */
extern bool BMS_IsStringPrecharging(uint8_t stringNumber);

/**
 * @brief   返回已连接的串数量
 * @return  返回已连接的串数量
 */
extern uint8_t BMS_GetNumberOfConnectedStrings(void);

/**
 * @brief   检查向错误状态的转换是否处于活动状态
 * @return  如果正在向错误状态转换则返回 True，否则返回 false
 */
extern bool BMS_IsTransitionToErrorStateActive(void);

/*========== 外部化的静态函数原型 (单元测试) ===========*/
#ifdef UNITY_UNIT_TEST
/* database.h 仅在 bms.c 中包含，并作为静态函数的参数使用。
 * 因此，我们需要在此处添加所需的包含。 */
#include "database.h"

extern BMS_RETURN_TYPE_e TEST_BMS_CheckStateRequest(BMS_STATE_REQUEST_e statereq);
extern BMS_STATE_REQUEST_e TEST_BMS_TransferStateRequest(void);
extern uint8_t TEST_BMS_CheckReEntrance(void);
extern uint8_t TEST_BMS_CheckCanRequests(void);
extern STD_RETURN_TYPE_e TEST_BMS_IsBatterySystemStateOkay(void);
extern bool TEST_BMS_IsContactorFeedbackValid(uint8_t stringNumber, CONT_TYPE_e contactorType);
extern bool TEST_BMS_IsAnyFatalErrorFlagSet(void);
extern void TEST_BMS_GetMeasurementValues(void);
extern void TEST_BMS_CheckOpenSenseWire(void);
extern STD_RETURN_TYPE_e TEST_BMS_MonitorPrechargeProcess(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues,
    BS_PRECHARGE_MONITORING_e monitoringParameters,
    uint32_t timeout_ms);
extern STD_RETURN_TYPE_e TEST_BMS_CheckPrecharge(uint8_t stringNumber, DATA_BLOCK_PACK_VALUES_s *pPackValues);
extern uint8_t TEST_BMS_GetHighestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues);
extern uint8_t TEST_BMS_GetClosestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues);
extern uint8_t TEST_BMS_GetLowestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues);
extern int32_t TEST_BMS_GetStringVoltageDifference(uint8_t string, DATA_BLOCK_PACK_VALUES_s *pPackValues);
extern int32_t TEST_BMS_GetAverageStringCurrent(DATA_BLOCK_PACK_VALUES_s *pPackValues);
extern void TEST_BMS_UpdateBatterySystemState(DATA_BLOCK_PACK_VALUES_s *pPackValues);
#endif

#endif /* FOXBMS__BMS_H_ */

