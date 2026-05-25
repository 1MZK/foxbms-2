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
 * @file    bms.c
 * @author  foxBMS 团队
 * @date    2020-02-24 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup ENGINE
 * @prefix  BMS
 *
 * @brief   BMS 驱动实现
 * @details 实现控制 BMS 的状态机
 *
 */

/*========== 包含文件 =======================================================*/
#include "bms.h"

#include "battery_cell_cfg.h"

#include "afe.h"
#include "bal.h"
#include "can_cbs_tx_cyclic.h"
#include "database.h"
#include "diag.h"
#include "foxmath.h"
#include "imd.h"
#include "led.h"
#include "meas.h"
#include "os.h"
#include "soa.h"
#include "sps.h"

#include <stdbool.h>
#include <stdint.h>

/*========== 宏和定义 =========================================================*/
/** 未设置“活动延迟时间”的默认值 */
#define BMS_NO_ACTIVE_DELAY_TIME_ms (UINT32_MAX)

/**
 * 保存上次状态和上次子状态
 */
#define BMS_SAVE_LAST_STATES()                \
    bms_state.lastState    = bms_state.state; \
    bms_state.lastSubstate = bms_state.substate

/*========== 静态常量和变量定义 =======================*/

/**
 * 包含 BMS 状态机的状态
 */
static BMS_STATE_s bms_state = {
    .currentSystick                    = 0u,
    .timer                             = 0u,
    .stateRequest                      = BMS_STATE_NO_REQUEST,
    .state                             = BMS_FSM_STATE_UNINITIALIZED,
    .substate                          = BMS_FSM_SUBSTATE_ENTRY,
    .lastState                         = BMS_FSM_STATE_UNINITIALIZED,
    .lastSubstate                      = BMS_FSM_SUBSTATE_ENTRY,
    .triggerentry                      = 0u,
    .ErrRequestCounter                 = 0u,
    .initFinished                      = STD_NOT_OK,
    .counter                           = 0u,
    .OscillationTimeout                = 0u,
    .prechargeTryCounter               = 0u,
    .powerPath                         = BMS_POWER_PATH_OPEN,
    .closedStrings                     = {GEN_REPEAT_U(false, GEN_STRIP(BS_NR_OF_STRINGS))},
    .closedPrechargeContactors         = {GEN_REPEAT_U(false, GEN_STRIP(BS_NR_OF_STRINGS))},
    .numberOfClosedStrings             = 0u,
    .deactivatedStrings                = {GEN_REPEAT_U(false, GEN_STRIP(BS_NR_OF_STRINGS))},
    .firstClosedString                 = 0u,
    .stringOpenTimeout                 = 0u,
    .nextStringClosedTimer             = 0u,
    .stringCloseTimeout                = 0u,
    .nextState                         = BMS_FSM_STATE_STANDBY,
    .restTimer_10ms                    = BS_RELAXATION_PERIOD_10ms,
    .currentFlowState                  = BMS_RELAXATION,
    .remainingDelay_ms                 = BMS_NO_ACTIVE_DELAY_TIME_ms,
    .minimumActiveDelay_ms             = BMS_NO_ACTIVE_DELAY_TIME_ms,
    .startOfPrecharging                = 0u,
    .transitionToErrorState            = false,
    .timeAboveContactorBreakCurrent_ms = 0u,
    .stringToBeOpened                  = 0u,
    .contactorToBeOpened               = CONT_UNDEFINED,
};

/** 数据库表的本地副本 */
/**@{*/
static DATA_BLOCK_MIN_MAX_s bms_tableMinMax         = {.header.uniqueId = DATA_BLOCK_ID_MIN_MAX};
static DATA_BLOCK_OPEN_WIRE_s bms_tableOpenWire     = {.header.uniqueId = DATA_BLOCK_ID_OPEN_WIRE_BASE};
static DATA_BLOCK_PACK_VALUES_s bms_tablePackValues = {.header.uniqueId = DATA_BLOCK_ID_PACK_VALUES};
/**@}*/

/*========== 外部常量和变量定义 =======================*/

/*========== 静态函数原型 =====================================*/

/**
 * @brief       检查发出的状态请求。
 * @details     此函数检查状态请求的有效性。检查结果立即返回。
 * @param[in]   statereq    要检查的状态请求
 * @return      发出的状态请求的结果
 */
static BMS_RETURN_TYPE_e BMS_CheckStateRequest(BMS_STATE_REQUEST_e statereq);

/**
 * @brief   将当前状态请求传递给状态机。
 * @details 此函数从 #bms_state 获取当前状态请求并将其传递给状态机。
 *          它将 #bms_state 的值重置为 #BMS_STATE_NO_REQUEST
 * @return  当前状态请求
 */
static BMS_STATE_REQUEST_e BMS_TransferStateRequest(void);

/**
 * @brief   SYS 状态机触发函数的重入检查
 * @details 此函数不可重入，应仅由时间或事件触发。它递增状态变量 bms_state
 *          的 triggerentry 计数器。它绝不应被两个不同的进程调用，因此如果是
 *          这种情况，调用此函数时 triggerentry 永远不应大于 0。
 * @return  retval  如果没有该函数的其他实例处于活动状态则返回 0，否则返回 0xff
 */
static uint8_t BMS_CheckReEntrance(void);

/**
 * @brief   检查向 BMS 状态机发出的状态请求。
 * @details 检查数据库中的状态请求并将此值设置为返回值。
 * @return  请求的状态
 */
static uint8_t BMS_CheckCanRequests(void);

/**
 * @brief   检查诊断模块中严重级别为 #DIAG_FATAL_ERROR 的所有错误标志
 * @details 检查诊断模块中严重级别为 #DIAG_FATAL_ERROR 的所有错误标志。
 *          此外，设置 bms_state 变量的 minimumActiveDelay_ms 参数。
 * @return  如果错误标志被设置则返回 true，否则返回 false
 */
static bool BMS_IsAnyFatalErrorFlagSet(void);

/**
 * @brief   检查是否设置了任何错误标志，并处理直到接触器需要断开的延迟。
 * @details 检查所有严重级别为 #DIAG_FATAL_ERROR 的诊断条目，并处理配置的
 *          延迟直到接触器需要断开。如果多个错误同时激活，则使用最短的延迟。
 * @return  如果检测到错误且延迟时间已过则返回 #STD_NOT_OK，否则返回 #STD_OK
 */
static STD_RETURN_TYPE_e BMS_IsBatterySystemStateOkay(void);

/**
 * @brief   检查特定接触器的接触器反馈是否有效
 * @details 读取错误标志数据库条目并检查此特定接触器的反馈是否有效。
 * @return  如果未检测到错误且反馈有效则返回 true，否则返回 false
 */
static bool BMS_IsContactorFeedbackValid(uint8_t stringNumber, CONT_TYPE_e contactorType);

/** 获取静态模块变量的最新数据库条目 */
static void BMS_GetMeasurementValues(void);

/**
 * @brief   检查是否有开路感测线
 */
static void BMS_CheckOpenSenseWire(void);

/**
 * @brief       检查是否违反了电流限制
 * @param[in]   stringNumber          寻址的串
 * @param[in]   pPackValues           指向包值数据库条目的指针
 * @param[in]   monitoringParameters  监控参数
 * @param[in]   timeout_ms            超时时间（毫秒）
 * @return      如果预充成功则返回 BMS_PRECHARGING_SUCCESSFUL
 *              如果预充正在进行则返回 BMS_PRECHARGING_ONGOING
 *              如果达到超时且预充过程未成功则返回 BMS_PRECHARGING_FAILED (类型: #BMS_RESULT_PRECHARGE_PROCESS_e)
 */
static BMS_RESULT_PRECHARGE_PROCESS_e BMS_MonitorPrechargeProcess(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues,
    BS_PRECHARGE_MONITORING_e monitoringParameters,
    uint32_t timeout_ms);

/**
 * @brief       检查通过的电池电流是否低于限值
 * @param[in]   stringNumber 寻址的串
 * @param[in]   pPackValues  指向包值数据库条目的指针
 * @return      如果电池电流低于限值则返回 #STD_OK，否则返回 #STD_NOT_OK
 */
static STD_RETURN_TYPE_e BMS_IsPrechargeCurrentBelowLimit(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues);

/**
 * @brief       检查是否违反了电流限制
 * @param[in]   stringNumber 寻址的串
 * @param[in]   pPackValues  指向包值数据库条目的指针
 * @return      如果电池与直流母线电压之间的电压差低于限值则返回 true，否则返回 false
 */
static STD_RETURN_TYPE_e BMS_IsPrechargeVoltageBelowLimit(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues);

/**
 * @brief   返回总电压最高的串的 ID
 * @details 这用于在请求行驶时闭合第一串。
 * @param[in]   precharge   如果为 #BMS_DO_NOT_TAKE_PRECHARGE_INTO_ACCOUNT，
 *                          则忽略串的预充可用性。
 *                          如果为 #BMS_TAKE_PRECHARGE_INTO_ACCOUNT，则仅选择
 *                          有预充可用的串。
 * @param[in]   pPackValues 指向包值数据库条目的指针
 * @return  电压最高的串的索引。如果没有可用的串，则返回 #BMS_NO_STRING_AVAILABLE。
 */
static uint8_t BMS_GetHighestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues);

/**
 * @brief   返回电压最接近首个闭合串电压的串的 ID
 * @details 这用于在行驶模式下闭合更多串。
 * @param[in]   precharge   如果为 #BMS_DO_NOT_TAKE_PRECHARGE_INTO_ACCOUNT，
 *                          则忽略串的预充可用性。
 *                          如果为 #BMS_TAKE_PRECHARGE_INTO_ACCOUNT，则仅选择
 *                          有预充可用的串。
 * @param[in]   pPackValues 指向包值数据库条目的指针
 * @return  电压最接近首个闭合串电压的串的索引。
 *          如果没有可用的串，则返回 #BMS_NO_STRING_AVAILABLE。
 */
static uint8_t BMS_GetClosestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues);

/**
 * @brief   返回总电压最低的串的 ID
 * @details 这用于在请求充电时闭合第一串。
 *
 * @param[in]   precharge   如果为 0，则忽略串的预充可用性。
 *                          如果为 1，则仅选择有预充可用的串。
 * @param[in]   pPackValues 指向包值数据库条目的指针
 * @return  电压最低的串的索引。如果没有可用的串，则返回 #BMS_NO_STRING_AVAILABLE。
 */
static uint8_t BMS_GetLowestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues);

/**
 * @brief   返回首个闭合串与指定串 ID 之间的电压差
 * @details 此函数用于在尝试闭合更多串时检查电压。
 * @param[in]   string  必须与首个闭合串进行比较的串 ID
 * @param[in]   pPackValues 指向包值数据库条目的指针
 * @return  电压差（单位 mV），如果电压无效且无法计算差值则返回 INT32_MAX
 */
static int32_t BMS_GetStringVoltageDifference(uint8_t string, const DATA_BLOCK_PACK_VALUES_s *pPackValues);

/**
 * @brief   返回流经所有串的平均电流。
 * @details 此函数在闭合串时使用。
 * @param[in]   pPackValues 指向包值数据库条目的指针
 * @return  考虑所有串的平均电流（单位 mA）。如果没有有效的电流测量则返回 INT32_MAX
 */
static int32_t BMS_GetAverageStringCurrent(DATA_BLOCK_PACK_VALUES_s *pPackValues);

/**
 * @brief   根据测量/最近的电流值更新电池系统状态变量
 * @param[in]   pPackValues  来自电流传感器的最近测量值
 */
static void BMS_UpdateBatterySystemState(DATA_BLOCK_PACK_VALUES_s *pPackValues);

/**
 * @brief   根据实际电流流向获取第一个应该断开的串接触器
 * @details 检查接触器的安装方向，并断开沿首选电流方向安装的接触器。
 *          如果没有沿首选电流方向断开可用的接触器，则先断开正极接触器。
 *          这可能是因为两个接触器安装在同一方向，或者接触器是双向的。
 * @param stringNumber         将要断开的串
 * @param flowDirection        电流流向（充电或放电）
 * @return #CONT_TYPE_e 应该断开的接触器
 */
static CONT_TYPE_e BMS_GetFirstContactorToBeOpened(uint8_t stringNumber, BMS_CURRENT_FLOW_STATE_e flowDirection);

/**
 * @brief   获取第二个应该断开的串接触器
 * @details 对于第二个接触器，不需要检查接触器的安装方向，因为断开第一个
 *          接触器时电流已经被切断。
 * @param stringNumber             将要断开的串
 * @param firstOpenedContactorType 已断开的第一个接触器的类型
 * @return #CONT_TYPE_e 应该断开的接触器
 */
static CONT_TYPE_e BMS_GetSecondContactorToBeOpened(uint8_t stringNumber, CONT_TYPE_e firstOpenedContactorType);

/*========== 静态函数实现 ================================*/

static BMS_RETURN_TYPE_e BMS_CheckStateRequest(BMS_STATE_REQUEST_e statereq) {
    if (statereq == BMS_STATE_ERROR_REQUEST) {
        return BMS_OK;
    }

    if (bms_state.stateRequest == BMS_STATE_NO_REQUEST) {
        /* 仅允许从未初始化状态进行初始化 */
        if (statereq == BMS_STATE_INITIALIZATION_REQUEST) {
            if (bms_state.state == BMS_FSM_STATE_UNINITIALIZED) {
                return BMS_OK;
            } else {
                return BMS_ALREADY_INITIALIZED;
            }
        } else {
            return BMS_ILLEGAL_REQUEST;
        }
    } else {
        return BMS_REQUEST_PENDING;
    }
}

static uint8_t BMS_CheckReEntrance(void) {
    uint8_t retval = 0;
    OS_EnterTaskCritical();
    if (!bms_state.triggerentry) {
        bms_state.triggerentry++;
    } else {
        retval = 0xFF; /* 函数被多次调用 */
    }
    OS_ExitTaskCritical();
    return retval;
}

static BMS_STATE_REQUEST_e BMS_TransferStateRequest(void) {
    BMS_STATE_REQUEST_e retval = BMS_STATE_NO_REQUEST;

    OS_EnterTaskCritical();
    retval                 = bms_state.stateRequest;
    bms_state.stateRequest = BMS_STATE_NO_REQUEST;
    OS_ExitTaskCritical();
    return retval;
}

static void BMS_GetMeasurementValues(void) {
    DATA_READ_DATA(&bms_tablePackValues, &bms_tableOpenWire, &bms_tableMinMax);
}

static uint8_t BMS_CheckCanRequests(void) {
    uint8_t retVal                     = BMS_REQ_ID_NOREQ;
    DATA_BLOCK_STATE_REQUEST_s request = {.header.uniqueId = DATA_BLOCK_ID_STATE_REQUEST};

    DATA_READ_DATA(&request);

    if (request.stateRequestViaCan == BMS_REQ_ID_STANDBY) {
        retVal = BMS_REQ_ID_STANDBY;
    } else if (request.stateRequestViaCan == BMS_REQ_ID_NORMAL) {
        retVal = BMS_REQ_ID_NORMAL;
    } else if (request.stateRequestViaCan == BMS_REQ_ID_CHARGE) {
        retVal = BMS_REQ_ID_CHARGE;
    } else if (request.stateRequestViaCan == BMS_REQ_ID_NOREQ) {
        retVal = BMS_REQ_ID_NOREQ;
    } else {
        /* 无效或无请求，默认为 BMS_REQ_ID_NOREQ (已设置) */
    }

    return retVal;
}

static void BMS_CheckOpenSenseWire(void) {
    uint8_t openWireDetected = 0;

    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
        /* 遍历所有模块 */
        for (uint8_t m = 0u; m < BS_NR_OF_MODULES_PER_STRING; m++) {
            /* 遍历所有电压感测线：每模组电芯数 + 1 */
            for (uint8_t wire = 0u; wire < (BS_NR_OF_CELL_BLOCKS_PER_MODULE + 1); wire++) {
                /* 检测到开路 */
                if (bms_tableOpenWire.openWire[s][(wire + (m * (BS_NR_OF_CELL_BLOCKS_PER_MODULE + 1))) == 1] > 0u) {
                    openWireDetected++;

                    /* 在此处添加额外的错误处理 */
                }
            }
        }
        /* 如果检测到开路则设置错误 */
        if (openWireDetected == 0u) {
            DIAG_Handler(DIAG_ID_AFE_OPEN_WIRE, DIAG_EVENT_OK, DIAG_STRING, s);
        } else {
            DIAG_Handler(DIAG_ID_AFE_OPEN_WIRE, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
        }
    }
}

static BMS_RESULT_PRECHARGE_PROCESS_e BMS_MonitorPrechargeProcess(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues,
    BS_PRECHARGE_MONITORING_e monitoringParameters,
    uint32_t timeout_ms) {
    /* 确保我们不会越界访问数据库表中的数组 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    FAS_ASSERT(pPackValues != NULL_PTR);
    FAS_ASSERT(
        (monitoringParameters == BS_PRECHARGE_MONITOR_CURRENT) ||
        (monitoringParameters == BS_PRECHARGE_MONITOR_VOLTAGE) ||
        (monitoringParameters == BS_PRECHARGE_MONITOR_CURRENT_AND_VOLTAGE));
    /* AXIVION 常规 Generic-MissingParameterAssert: timeout_ms: 参数接受整个范围 */
    /* 指示预充正在进行，直到完成或预充失败 */
    BMS_RESULT_PRECHARGE_PROCESS_e prechargingState = BMS_PRECHARGING_ONGOING;
    STD_RETURN_TYPE_e currentPrecharged             = STD_NOT_OK;
    STD_RETURN_TYPE_e voltagePrecharged             = STD_NOT_OK;
    if (monitoringParameters == BS_PRECHARGE_MONITOR_CURRENT) {
        currentPrecharged = BMS_IsPrechargeCurrentBelowLimit(stringNumber, pPackValues);
        voltagePrecharged = STD_OK;
    } else if (monitoringParameters == BS_PRECHARGE_MONITOR_VOLTAGE) {
        voltagePrecharged = BMS_IsPrechargeVoltageBelowLimit(stringNumber, pPackValues);
        currentPrecharged = STD_OK;
    } else {
        /* monitoringParameters == BS_PRECHARGE_MONITOR_CURRENT_AND_VOLTAGE */
        currentPrecharged = BMS_IsPrechargeCurrentBelowLimit(stringNumber, pPackValues);
        voltagePrecharged = BMS_IsPrechargeVoltageBelowLimit(stringNumber, pPackValues);
    }

    if ((currentPrecharged == STD_OK) && (voltagePrecharged == STD_OK)) {
        prechargingState = BMS_PRECHARGING_FINISHED;
        (void)DIAG_Handler(DIAG_ID_PRECHARGE_ABORT_REASON_VOLTAGE, DIAG_EVENT_OK, DIAG_STRING, stringNumber);
        (void)DIAG_Handler(DIAG_ID_PRECHARGE_ABORT_REASON_CURRENT, DIAG_EVENT_OK, DIAG_STRING, stringNumber);
    } else {
        /* 检查是否达到预充超时以指示失败 */
        if (bms_state.currentSystick - bms_state.startOfPrecharging > timeout_ms) {
            prechargingState = BMS_PRECHARGING_FAILED;
            DIAG_CheckEvent(currentPrecharged, DIAG_ID_PRECHARGE_ABORT_REASON_CURRENT, DIAG_STRING, stringNumber);
            DIAG_CheckEvent(voltagePrecharged, DIAG_ID_PRECHARGE_ABORT_REASON_VOLTAGE, DIAG_STRING, stringNumber);
        }
    }
    return prechargingState;
}

static STD_RETURN_TYPE_e BMS_IsPrechargeCurrentBelowLimit(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    /* AXIVION 常规 Generic-MissingParameterAssert: stringNumber: 函数参数由调用者检查 */
    /* AXIVION 常规 Generic-MissingParameterAssert: pPackValues: 函数参数由调用者检查 */
    STD_RETURN_TYPE_e retval = STD_NOT_OK;
    /* 仅检查电流，不检查电流方向 */
    if ((pPackValues->invalidStringCurrent[stringNumber] == 0u) &&
        ((MATH_AbsInt32_t(pPackValues->stringCurrent_mA[stringNumber]) < BMS_PRECHARGE_CURRENT_THRESHOLD_mA))) {
        retval = STD_OK;
    }
    return retval;
}

static STD_RETURN_TYPE_e BMS_IsPrechargeVoltageBelowLimit(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    /* AXIVION 常规 Generic-MissingParameterAssert: stringNumber: 函数参数由调用者检查 */
    /* AXIVION 常规 Generic-MissingParameterAssert: pPackValues: 函数参数由调用者检查 */
    STD_RETURN_TYPE_e retval = STD_NOT_OK;
    if ((pPackValues->invalidStringVoltage[stringNumber] == 0u) && (pPackValues->invalidHvBusVoltage == 0u)) {
        const int64_t cont_prechargeVoltDiff_mV = MATH_AbsInt64_t(
            (int64_t)pPackValues->stringVoltage_mV[stringNumber] - (int64_t)pPackValues->highVoltageBusVoltage_mV);
        if (cont_prechargeVoltDiff_mV < BMS_PRECHARGE_VOLTAGE_THRESHOLD_mV) {
            retval = STD_OK;
        }
    }
    return retval;
}

static bool BMS_IsAnyFatalErrorFlagSet(void) {
    bool fatalErrorActive = false;

    for (uint16_t entry = 0u; entry < diag_device.numberOfFatalErrors; entry++) {
        const STD_RETURN_TYPE_e diagnosisState =
            DIAG_GetDiagnosisEntryState(diag_device.pFatalErrorLinkTable[entry]->id);
        if (STD_NOT_OK == diagnosisState) {
            /* 检测到致命错误 -> 获取此错误直到接触器应断开的延迟 */
            const uint32_t kDelay_ms = DIAG_GetDelay(diag_device.pFatalErrorLinkTable[entry]->id);
            /* 检查检测到的故障延迟是否小于先前检测到的故障延迟 */
            if (bms_state.minimumActiveDelay_ms > kDelay_ms) {
                bms_state.minimumActiveDelay_ms = kDelay_ms;
            }
            fatalErrorActive = true;
        }
    }
    return fatalErrorActive;
}

static STD_RETURN_TYPE_e BMS_IsBatterySystemStateOkay(void) {
    STD_RETURN_TYPE_e retVal          = STD_OK; /* 如果检测到错误则设置为 STD_NOT_OK */
    static uint32_t previousTimestamp = 0u;
    uint32_t timestamp                = OS_GetTickCount();

    /* 检查是否检测到任何致命错误 */
    const bool isErrorActive = BMS_IsAnyFatalErrorFlagSet();

    /** 检查之前是否检测到致命错误。如果是，检查延迟 */
    if (bms_state.transitionToErrorState == true) {
        /* 减少自上次调用以来的活动延迟 */
        const uint32_t timeSinceLastCall_ms = timestamp - previousTimestamp;
        if (timeSinceLastCall_ms <= bms_state.remainingDelay_ms) {
            bms_state.remainingDelay_ms -= timeSinceLastCall_ms;
        } else {
            bms_state.remainingDelay_ms = 0u;
        }

        /* 检查新错误的延迟是否短于 BMS 状态机中先前检测到错误的活动延迟 */
        if (bms_state.remainingDelay_ms >= bms_state.minimumActiveDelay_ms) {
            bms_state.remainingDelay_ms = bms_state.minimumActiveDelay_ms;
        }
    } else {
        /* 延迟未激活，检查是否应激活 */
        if (isErrorActive == true) {
            bms_state.transitionToErrorState = true;
            bms_state.remainingDelay_ms      = bms_state.minimumActiveDelay_ms;
        }
    }

    /** 设置上次时间戳以供下次调用 */
    previousTimestamp = timestamp;

    /* 检查 bms 状态机是否应切换到错误状态。如果延迟被激活且剩余延迟降至 0，则属于这种情况 */
    if ((bms_state.transitionToErrorState == true) && (bms_state.remainingDelay_ms == 0u)) {
        retVal = STD_NOT_OK;
    }

    return retVal;
}

static bool BMS_IsContactorFeedbackValid(uint8_t stringNumber, CONT_TYPE_e contactorType) {
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    FAS_ASSERT(contactorType != CONT_UNDEFINED);
    bool feedbackValid = false;
    /* 从数据库读取最新的错误标志 */
    DATA_BLOCK_ERROR_STATE_s tableErrorFlags = {.header.uniqueId = DATA_BLOCK_ID_ERROR_STATE};
    DATA_READ_DATA(&tableErrorFlags);
    /* 检查接触器反馈是否有效 */
    switch (contactorType) {
        case CONT_PLUS:
            if (tableErrorFlags.contactorInPositivePathOfStringFeedbackError[stringNumber] == false) {
                feedbackValid = true;
            }
            break;
        case CONT_MINUS:
            if (tableErrorFlags.contactorInNegativePathOfStringFeedbackError[stringNumber] == false) {
                feedbackValid = true;
            }
            break;
        case CONT_PRECHARGE:
            if (tableErrorFlags.prechargeContactorFeedbackError[stringNumber] == false) {
                feedbackValid = true;
            }
            break;
        default:
            /* CONT_UNDEFINED 已通过断言阻止 */
            break;
    }
    return feedbackValid;
}

static uint8_t BMS_GetHighestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);
    uint8_t highest_string_index = BMS_NO_STRING_AVAILABLE;
    int32_t max_stringVoltage_mV = INT32_MIN;

    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
        if ((pPackValues->stringVoltage_mV[s] >= max_stringVoltage_mV) &&
            (pPackValues->invalidStringVoltage[s] == 0u)) {
            if (bms_state.deactivatedStrings[s] == false) {
                if (precharge == BMS_DO_NOT_TAKE_PRECHARGE_INTO_ACCOUNT) {
                    max_stringVoltage_mV = pPackValues->stringVoltage_mV[s];
                    highest_string_index = s;
                } else {
                    if (bs_stringsWithPrecharge[s] == BS_STRING_WITH_PRECHARGE) {
                        max_stringVoltage_mV = pPackValues->stringVoltage_mV[s];
                        highest_string_index = s;
                    }
                }
            }
        }
    }

    return highest_string_index;
}

static uint8_t BMS_GetClosestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);
    uint8_t closestStringIndex     = BMS_NO_STRING_AVAILABLE;
    int32_t closedStringVoltage_mV = 0;
    bool searchString              = false;

    /* 获取首个闭合串的电压 */
    if (pPackValues->invalidStringVoltage[bms_state.firstClosedString] == 0u) {
        closedStringVoltage_mV = pPackValues->stringVoltage_mV[bms_state.firstClosedString];
        searchString           = true;
    } else if (pPackValues->invalidHvBusVoltage == 0u) {
        /* 如果测量的串电压无效，则使用高压母线电压 */
        closedStringVoltage_mV = pPackValues->highVoltageBusVoltage_mV;
        searchString           = true;
    } else {
        /* 如果无法测量有效电压，则不搜索下一个串 */
        searchString = false;
    }

    if (searchString == true) {
        for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
            const bool isStringClosed          = BMS_IsStringClosed(s);
            const uint8_t isStringVoltageValid = pPackValues->invalidStringVoltage[s];
            if ((isStringClosed == false) && (isStringVoltageValid == 0u)) {
                /* 仅检查具有有效电压的断开串 */
                int32_t minimumVoltageDifference_mV = INT32_MAX;
                int32_t voltageDifference_mV        = labs(closedStringVoltage_mV - pPackValues->stringVoltage_mV[s]);
                if (voltageDifference_mV <= minimumVoltageDifference_mV) {
                    if (bms_state.deactivatedStrings[s] == false) {
                        if (precharge == BMS_TAKE_PRECHARGE_INTO_ACCOUNT) {
                            if (bs_stringsWithPrecharge[s] == BS_STRING_WITH_PRECHARGE) {
                                minimumVoltageDifference_mV = voltageDifference_mV;
                                closestStringIndex          = s;
                            }
                        } else {
                            minimumVoltageDifference_mV = voltageDifference_mV;
                            closestStringIndex          = s;
                        }
                    }
                }
            }
        }
    }
    return closestStringIndex;
}

static uint8_t BMS_GetLowestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);
    uint8_t lowest_string_index  = BMS_NO_STRING_AVAILABLE;
    int32_t min_stringVoltage_mV = INT32_MAX;

    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
        if ((pPackValues->stringVoltage_mV[s] <= min_stringVoltage_mV) &&
            (pPackValues->invalidStringVoltage[s] == 0u)) {
            if (bms_state.deactivatedStrings[s] == false) {
                if (precharge == BMS_DO_NOT_TAKE_PRECHARGE_INTO_ACCOUNT) {
                    min_stringVoltage_mV = pPackValues->stringVoltage_mV[s];
                    lowest_string_index  = s;
                } else {
                    if (bs_stringsWithPrecharge[s] == BS_STRING_WITH_PRECHARGE) {
                        min_stringVoltage_mV = pPackValues->stringVoltage_mV[s];
                        lowest_string_index  = s;
                    }
                }
            }
        }
    }
    return lowest_string_index;
}

static int32_t BMS_GetStringVoltageDifference(uint8_t string, const DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(string < BS_NR_OF_STRINGS);
    FAS_ASSERT(pPackValues != NULL_PTR);
    int32_t voltageDifference_mV = INT32_MAX;
    if ((pPackValues->invalidStringVoltage[string] == 0u) &&
        (pPackValues->invalidStringVoltage[bms_state.firstClosedString] == 0u)) {
        /* 计算串电压之间的差值 */
        voltageDifference_mV = MATH_AbsInt32_t(
            pPackValues->stringVoltage_mV[string] - pPackValues->stringVoltage_mV[bms_state.firstClosedString]);
    } else if ((pPackValues->invalidStringVoltage[string] == 0u) && (pPackValues->invalidHvBusVoltage == 0u)) {
        /* 计算串与高压母线电压之间的差值 */
        voltageDifference_mV =
            MATH_AbsInt32_t(pPackValues->stringVoltage_mV[string] - pPackValues->highVoltageBusVoltage_mV);
    } else {
        /* 没有有效电压可供比较 -> 不计算差值并返回 INT32_MAX */
        voltageDifference_mV = INT32_MAX;
    }
    return voltageDifference_mV;
}

static int32_t BMS_GetAverageStringCurrent(DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);
    int32_t average_current = pPackValues->packCurrent_mA / (int32_t)BS_NR_OF_STRINGS;
    if (pPackValues->invalidPackCurrent == 1u) {
        average_current = INT32_MAX;
    }
    return average_current;
}

static void BMS_UpdateBatterySystemState(DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);

    /* 仅当电流值有效时更新系统状态 */
    if (pPackValues->invalidPackCurrent == 0u) {
        if (BS_POSITIVE_DISCHARGE_CURRENT == true) {
            /* 正电流值等于电池系统放电 */
            if (pPackValues->packCurrent_mA >= BS_REST_CURRENT_mA) { /* TODO: 串使用包电流 */
                bms_state.currentFlowState = BMS_DISCHARGING;
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms;
            } else if (pPackValues->packCurrent_mA <= -BS_REST_CURRENT_mA) {
                bms_state.currentFlowState = BMS_CHARGING;
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms;
            } else {
                /* 电流低于静置电流：电池系统处于静置状态或弛豫过程仍在进行 */
                if (bms_state.restTimer_10ms == 0u) {
                    /* 静置计时器已过 -> 电池系统处于静置状态 */
                    bms_state.currentFlowState = BMS_AT_REST;
                } else {
                    bms_state.restTimer_10ms--;
                    bms_state.currentFlowState = BMS_RELAXATION;
                }
            }
        } else {
            /* 负电流值等于电池系统放电 */
            if (pPackValues->packCurrent_mA <= -BS_REST_CURRENT_mA) {
                bms_state.currentFlowState = BMS_DISCHARGING;
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms;
            } else if (pPackValues->packCurrent_mA >= BS_REST_CURRENT_mA) {
                bms_state.currentFlowState = BMS_CHARGING;
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms;
            } else {
                /* 电流低于静置电流：电池系统处于静置状态或弛豫过程仍在进行 */
                if (bms_state.restTimer_10ms == 0u) {
                    /* 静置计时器已过 -> 电池系统处于静置状态 */
                    bms_state.currentFlowState = BMS_AT_REST;
                } else {
                    bms_state.restTimer_10ms--;
                    bms_state.currentFlowState = BMS_RELAXATION;
                }
            }
        }
    }
}

static CONT_TYPE_e BMS_GetFirstContactorToBeOpened(uint8_t stringNumber, BMS_CURRENT_FLOW_STATE_e flowDirection) {
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    /* AXIVION 常规 Generic-MissingParameterAssert: flowDirection: 参数接受所有定义的枚举 */
    CONT_TYPE_e contactorToBeOpened                     = CONT_UNDEFINED;
    CONT_CURRENT_BREAKING_DIRECTION_e breakingDirection = CONT_BIDIRECTIONAL;
    /* 根据电流方向所需的优先断开方向 */
    if (flowDirection == BMS_CHARGING) {
        breakingDirection = CONT_CHARGING_DIRECTION;
    } else {
        breakingDirection = CONT_DISCHARGING_DIRECTION;
    }
    /* 遍历接触器数组并搜索所需的接触器 */
    uint8_t contactor = 0u;
    for (; contactor < BS_NR_OF_CONTACTORS; contactor++) {
        /* 搜索：
         * 1. 来自请求串的接触器
         * 2. 沿优先断开方向安装或双向的接触器
         * 3. 不是预充接触器 */
        bool correctString           = (bool)(stringNumber == cont_contactorStates[contactor].stringIndex);
        bool inPreferredDirection    = (bool)(breakingDirection == cont_contactorStates[contactor].breakingDirection);
        bool hasNoPreferredDirection = (bool)(cont_contactorStates[contactor].breakingDirection == CONT_BIDIRECTIONAL);
        bool noPrechargeContactor    = (bool)(cont_contactorStates[contactor].type != CONT_PRECHARGE);
        if (correctString && noPrechargeContactor && (inPreferredDirection || hasNoPreferredDirection)) {
            contactorToBeOpened = cont_contactorStates[contactor].type;
            break;
        }
    }
    if (contactor == BS_NR_OF_CONTACTORS) {
        /* 未找到沿首选电流方向安装的接触器。在数组 cont_contactorStates 中
         * 选择来自传递串的 PLUS 接触器 */
        for (contactor = 0u; contactor < BS_NR_OF_CONTACTORS; contactor++) {
            /* 搜索：
             * 1. 来自请求串的接触器
             * 2. 是 PLUS 接触器 */
            if ((stringNumber == cont_contactorStates[contactor].stringIndex) &&
                (cont_contactorStates[contactor].type == CONT_PLUS)) {
                contactorToBeOpened = cont_contactorStates[contactor].type;
                break;
            }
        }
    }
    if (contactor == BS_NR_OF_CONTACTORS) {
        /* 未找到 PLUS 接触器。在数组 cont_contactorStates 中选择来自传递串的 MINUS 接触器 */
        for (contactor = 0u; contactor < BS_NR_OF_CONTACTORS; contactor++) {
            /* 搜索：
             * 1. 来自请求串的接触器
             * 2. 是 PLUS 接触器 */
            if ((stringNumber == cont_contactorStates[contactor].stringIndex) &&
                (cont_contactorStates[contactor].type == CONT_MINUS)) {
                contactorToBeOpened = cont_contactorStates[contactor].type;
                break;
            }
        }
    }
    if (contactor == BS_NR_OF_CONTACTORS) {
        /* 在请求的串中未找到 PLUS 或 MAIN_MINUS 接触器。 */
        FAS_ASSERT(FAS_TRAP);
    }
    return contactorToBeOpened;
}

static CONT_TYPE_e BMS_GetSecondContactorToBeOpened(uint8_t stringNumber, CONT_TYPE_e firstOpenedContactorType) {
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    FAS_ASSERT((firstOpenedContactorType != CONT_UNDEFINED) && (firstOpenedContactorType != CONT_PRECHARGE));
    CONT_TYPE_e contactorToBeOpened = CONT_UNDEFINED;
    /* 检查哪个接触器已经断开并选择另一个 */
    if (firstOpenedContactorType == CONT_PLUS) {
        contactorToBeOpened = CONT_MINUS;
    } else {
        contactorToBeOpened = CONT_PLUS;
    }
    /* 遍历接触器数组并搜索所需的接触器 */
    uint8_t contactor = 0u;
    for (; contactor < BS_NR_OF_CONTACTORS; contactor++) {
        /* 搜索来自请求串的特定接触器 */
        if ((stringNumber == cont_contactorStates[contactor].stringIndex) &&
            (contactorToBeOpened == cont_contactorStates[contactor].type)) {
            contactorToBeOpened = cont_contactorStates[contactor].type;
            break;
        }
    }
    if (contactor == BS_NR_OF_CONTACTORS) {
        /* 在请求的串中未找到 PLUS 或 MAIN_MINUS 接触器。
         * 显然，该串只定义了一个接触器 */
        FAS_ASSERT(FAS_TRAP);
    }
    return contactorToBeOpened;
}

/*========== 外部函数实现 ================================*/

extern STD_RETURN_TYPE_e BMS_GetInitializationState(void) {
    return bms_state.initFinished;
}

extern BMS_FSM_STATES_e BMS_GetState(void) {
    return bms_state.state;
}

extern BMS_FSM_SUB_e BMS_GetSubstate(void) {
    return bms_state.substate;
}

BMS_RETURN_TYPE_e BMS_SetStateRequest(BMS_STATE_REQUEST_e statereq) {
    BMS_RETURN_TYPE_e retVal = BMS_OK;

    OS_EnterTaskCritical();
    retVal = BMS_CheckStateRequest(statereq);

    if (retVal == BMS_OK) {
        bms_state.stateRequest = statereq;
    }
    OS_ExitTaskCritical();

    return retVal;
}

void BMS_Trigger(void) {
    BMS_STATE_REQUEST_e statereq                   = BMS_STATE_NO_REQUEST;
    DATA_BLOCK_SYSTEM_STATE_s systemState          = {.header.uniqueId = DATA_BLOCK_ID_SYSTEM_STATE};
    bms_state.currentSystick                       = OS_GetTickCount();
    static uint32_t nextOpenWireCheck              = 0;
    static uint8_t stringNumber                    = 0u;
    static uint8_t nextStringNumber                = 0u;
    CONT_ELECTRICAL_STATE_TYPE_e contactorState    = CONT_SWITCH_UNDEFINED;
    BMS_RESULT_PRECHARGE_PROCESS_e prechargeRetval = BMS_PRECHARING_HAS_NOT_STARTED;
    bool contactorFeedbackValid                    = false;
    STD_RETURN_TYPE_e contRetVal                   = STD_NOT_OK;

    if (bms_state.state != BMS_FSM_STATE_UNINITIALIZED) {
        BMS_GetMeasurementValues();
        BMS_UpdateBatterySystemState(&bms_tablePackValues);
        SOA_CheckVoltages(&bms_tableMinMax);
        SOA_CheckTemperatures(&bms_tableMinMax, &bms_tablePackValues);
        SOA_CheckCurrent(&bms_tablePackValues);
        SOA_CheckSlaveTemperatures();
        BMS_CheckOpenSenseWire();
        CONT_CheckFeedback();
    }
    /* 检查函数的重入 */
    if (BMS_CheckReEntrance() > 0u) {
        return;
    }

    if (bms_state.nextStringClosedTimer > 0u) {
        bms_state.nextStringClosedTimer--;
    }
    if (bms_state.stringOpenTimeout > 0u) {
        bms_state.stringOpenTimeout--;
    }

    if (bms_state.stringCloseTimeout > 0u) {
        bms_state.stringCloseTimeout--;
    }

    if (bms_state.OscillationTimeout > 0u) {
        bms_state.OscillationTimeout--;
    }

    if (bms_state.timer > 0u) {
        if ((--bms_state.timer) > 0u) {
            bms_state.triggerentry--;
            return; /* 仅在计时器到期时处理状态机 */
        }
    }

    /****每次触发状态机时发生**************/
    switch (bms_state.state) {
        /****************************未初始化****************************/
        case BMS_FSM_STATE_UNINITIALIZED:
            /* 等待初始化请求 */
            statereq = BMS_TransferStateRequest();
            if (statereq == BMS_STATE_INITIALIZATION_REQUEST) {
                BMS_SAVE_LAST_STATES();
                bms_state.timer    = BMS_FSM_SHORTTIME;
                bms_state.state    = BMS_FSM_STATE_INITIALIZATION;
                bms_state.substate = BMS_FSM_SUBSTATE_ENTRY;
            } else if (statereq == BMS_STATE_NO_REQUEST) {
                /* 无实际请求挂起 */
            } else {
                bms_state.ErrRequestCounter++; /* 非法请求挂起 */
            }
            break;

        /****************************初始化***************************/
        case BMS_FSM_STATE_INITIALIZATION:
            BMS_SAVE_LAST_STATES();
            /* 重置 ALERT 模式标志 */
            DIAG_Handler(DIAG_ID_ALERT_MODE, DIAG_EVENT_OK, DIAG_SYSTEM, 0u);
            bms_state.initFinished = STD_OK;
            bms_state.timer        = BMS_FSM_LONGTIME;
            bms_state.state        = BMS_FSM_STATE_INITIALIZED;
            bms_state.substate     = BMS_FSM_SUBSTATE_ENTRY;
            break;

        /****************************已初始化******************************/
        case BMS_FSM_STATE_INITIALIZED:
            BMS_SAVE_LAST_STATES();
            if (IMD_RequestInsulationMeasurement() == IMD_ILLEGAL_REQUEST) {
                /* IMD 设备初始化尚未完成 -> 等待完成后再继续 */
                bms_state.timer = BMS_FSM_LONGTIME;
            } else {
                bms_state.timer    = BMS_FSM_SHORTTIME;
                bms_state.state    = BMS_FSM_STATE_IDLE;
                bms_state.substate = BMS_FSM_SUBSTATE_ENTRY;
            }
            break;

        /****************************空闲*************************************/
        case BMS_FSM_STATE_IDLE:
            BMS_SAVE_LAST_STATES();

            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                DATA_READ_DATA(&systemState);
                systemState.bmsCanState = BMS_CAN_STATE_IDLE;
                DATA_WRITE_DATA(&systemState);
                bms_state.timer    = BMS_FSM_SHORTTIME;
                bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS) {
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS) {
                if (BMS_CheckCanRequests() == BMS_REQ_ID_STANDBY) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_STANDBY;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                }
                break;
            }
            break;

        /****************************断开接触器**************************/
        case BMS_FSM_STATE_OPEN_CONTACTORS:
            BMS_SAVE_LAST_STATES();

            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                BAL_SetStateRequest(BAL_STATE_NO_BALANCING_REQUEST);
                /* 检查错误原因是否是电源电压钳位 30C 丢失 */
                if (DIAG_GetDiagnosisEntryState(DIAG_ID_SUPPLY_VOLTAGE_CLAMP_30C_LOST) == STD_NOT_OK) {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_HANDLE_SUPPLY_VOLTAGE_30C_LOSS;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_OPEN_ALL_PRECHARGE_CONTACTORS;
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_OPEN_ALL_PRECHARGE_CONTACTORS) {
                /* 预充接触器总是可以断开，因为预充电阻限制了最大电流 */
                CONT_OpenAllPrechargeContactors();

                /* 常规串断开 - 逐个断开串，从最高串索引开始 */
                stringNumber       = BS_NR_OF_STRINGS - 1u;
                bms_state.substate = BMS_FSM_SUBSTATE_OPEN_FIRST_STRING_CONTACTOR;
                bms_state.timer    = BMS_TIME_WAIT_AFTER_OPENING_PRECHARGE;

            } else if (bms_state.substate == BMS_FSM_SUBSTATE_OPEN_FIRST_STRING_CONTACTOR) {
                /* 预充接触器已断开 -> 开始断开第一个串接触器 */
                /* TODO: 检查预充接触器是否已断开？ */
                if ((bms_tablePackValues.invalidStringCurrent[stringNumber] == 0u) &&
                    (MATH_AbsInt32_t(bms_tablePackValues.stringCurrent_mA[stringNumber]) <
                     BS_MAIN_CONTACTORS_MAXIMUM_BREAK_CURRENT_mA)) {
                    /* 电流低于最大分断电流 -> 断开第一个接触器
                     * 检查接触器的安装方向并断开沿首选电流方向安装的接触器。
                     * 如果没有沿首选电流方向断开可用的接触器，则先断开正极接触器。
                     * 这可能是因为两个接触器安装在同一方向，或者接触器是双向的。
                     */
                    const BMS_CURRENT_FLOW_STATE_e flowDirection =
                        BMS_GetCurrentFlowDirection(bms_tablePackValues.stringCurrent_mA[stringNumber]);
                    bms_state.contactorToBeOpened = BMS_GetFirstContactorToBeOpened(stringNumber, flowDirection);
                    bms_state.stringToBeOpened    = stringNumber;
                    /* 断开第一个接触器 */
                    CONT_OpenContactor(stringNumber, bms_state.contactorToBeOpened);
                    bms_state.timer             = BMS_WAIT_TIME_AFTER_OPENING_STRING_CONTACTOR;
                    bms_state.substate          = BMS_FSM_SUBSTATE_OPEN_SECOND_STRING_CONTACTOR;
                    bms_state.stringOpenTimeout = BMS_STRING_OPEN_TIMEOUT;
                } else {
                    /* 电流高于接触器最大分断电流 -> 接触器无法断开 */
                    bms_state.timeAboveContactorBreakCurrent_ms += BMS_STATEMACHINE_TASK_CYCLE_CONTEXT_MS;
                    if (bms_state.timeAboveContactorBreakCurrent_ms > BS_MAIN_FUSE_MAXIMUM_TRIGGER_DURATION_ms) {
                        /* 熔断器此时本应已触发但显然尚未触发。不再等待。
                         * 激活 ALERT 模式并仍然开始断开接触器 */
                        DIAG_Handler(DIAG_ID_ALERT_MODE, DIAG_EVENT_NOT_OK, DIAG_SYSTEM, 0u);
                        const BMS_CURRENT_FLOW_STATE_e flowDirection =
                            BMS_GetCurrentFlowDirection(bms_tablePackValues.stringCurrent_mA[stringNumber]);
                        bms_state.contactorToBeOpened = BMS_GetFirstContactorToBeOpened(stringNumber, flowDirection);
                        bms_state.stringToBeOpened    = stringNumber;
                        /* 断开第一个接触器 */
                        CONT_OpenContactor(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                        bms_state.timer    = BMS_WAIT_TIME_AFTER_OPENING_STRING_CONTACTOR;
                        bms_state.substate = BMS_FSM_SUBSTATE_OPEN_SECOND_STRING_CONTACTOR;
                    }
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_OPEN_SECOND_STRING_CONTACTOR) {
                /* 检查第一个接触器是否已正确断开 */
                contactorState = CONT_GetContactorState(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                contactorFeedbackValid =
                    BMS_IsContactorFeedbackValid(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                /* 如果我们想因为该接触器的反馈错误而断开接触器，该语句将永远不会为真。
                 * 因此，如果检测到该接触器的反馈错误也继续，因为此时我们无法获得
                 * 有效的反馈信息 */
                if ((contactorState == CONT_SWITCH_OFF) || (contactorFeedbackValid == false)) {
                    /* 第一个接触器已正确断开。
                     * 断开第二个接触器。将首先断开的接触器传入函数 */
                    bms_state.contactorToBeOpened =
                        BMS_GetSecondContactorToBeOpened(stringNumber, bms_state.contactorToBeOpened);
                    /* 断开第二个接触器 */
                    CONT_OpenContactor(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                    bms_state.timer    = BMS_WAIT_TIME_AFTER_OPENING_STRING_CONTACTOR;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_SECOND_STRING_CONTACTOR;
                } else {
                    /* 串未断开，重新发出断开请求 */
                    CONT_OpenContactor(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                    bms_state.timer = BMS_FSM_SHORTTIME;
                    /* TODO: 添加超时 */
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_SECOND_STRING_CONTACTOR) {
                /* 检查第二个接触器是否已正确断开 */
                contactorState = CONT_GetContactorState(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                contactorFeedbackValid =
                    BMS_IsContactorFeedbackValid(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                /* 如果我们想因为该接触器的反馈错误而断开接触器，该语句将永远不会为真。
                 * 因此，如果检测到该接触器的反馈错误也继续，因为此时我们无法获得
                 * 有效的反馈信息 */
                if ((contactorState == CONT_SWITCH_OFF) || (contactorFeedbackValid == false)) {
                    /* 该串的断开完成。重置用于断开的状态变量 */
                    bms_state.contactorToBeOpened = CONT_UNDEFINED;
                    bms_state.stringToBeOpened    = 0u;
                    /* 串已断开。递减串计数器 */
                    if (bms_state.numberOfClosedStrings > 0u) {
                        bms_state.numberOfClosedStrings--;
                    }
                    bms_state.closedStrings[stringNumber] = false;
                    if (stringNumber > 0u) {
                        /* 并非所有串都已断开 -> 断开下一个串 */
                        stringNumber--;
                        bms_state.substate = BMS_FSM_SUBSTATE_OPEN_FIRST_STRING_CONTACTOR;
                        bms_state.timer    = BMS_FSM_SHORTTIME;
                        break;
                    } else {
                        /* 所有串都已断开 -> 准备离开状态 BMS_FSM_OPEN_CONTACTORS */
                        bms_state.substate = BMS_FSM_SUBSTATE_OPEN_STRINGS_EXIT;
                        bms_state.timer    = BMS_FSM_SHORTTIME;
                    }
                    break;
                } else if (bms_state.stringOpenTimeout == 0u) {
                    /* 串断开花费时间过长，转到下一个串 */
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_OPEN_FIRST_STRING_CONTACTOR;
                    break;
                } else {
                    /* 串未断开，重新发出断开请求 */
                    CONT_OpenContactor(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                    bms_state.timer = BMS_FSM_SHORTTIME;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_HANDLE_SUPPLY_VOLTAGE_30C_LOSS) {
                CONT_OpenAllContactors();
                SPS_SwitchOffAllGeneralIoChannels();
                bms_state.substate = BMS_FSM_SUBSTATE_OPEN_STRINGS_EXIT;
                bms_state.timer    = BMS_FSM_SHORTTIME;
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_OPEN_STRINGS_EXIT) {
                if (bms_state.nextState == BMS_FSM_STATE_STANDBY) {
                    /* 由于 STANDBY 请求而断开 -> 切换到 BMS_FSM_STANDBY */
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.state    = BMS_FSM_STATE_STANDBY;
                    bms_state.substate = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    /* 由于检测到错误而断开 -> 切换到 BMS_FSM_STATE_ERROR */
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.state    = BMS_FSM_STATE_ERROR;
                    bms_state.substate = BMS_FSM_SUBSTATE_ENTRY;
                }
            } else {
                FAS_ASSERT(FAS_TRAP);
            }
            break;

        /****************************待机**********************************/
        case BMS_FSM_STATE_STANDBY:
            BMS_SAVE_LAST_STATES();
            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                BAL_SetStateRequest(BAL_STATE_ALLOW_BALANCING_REQUEST);
#if BS_STANDBY_PERIODIC_OPEN_WIRE_CHECK == TRUE
                nextOpenWireCheck = timestamp + BS_STANDBY_OPEN_WIRE_PERIOD_ms;
#endif /* BS_STANDBY_PERIODIC_OPEN_WIRE_CHECK == TRUE */
                bms_state.timer    = BMS_FSM_MEDIUM_TIME;
                bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_INTERLOCK;
                DATA_READ_DATA(&systemState);
                systemState.bmsCanState = BMS_CAN_STATE_STANDBY;
                DATA_WRITE_DATA(&systemState);
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_INTERLOCK) {
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_INTERLOCK_CHECKED;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_INTERLOCK_CHECKED) {
                bms_state.timer    = BMS_FSM_SHORTTIME;
                bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS) {
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS) {
                if (BMS_CheckCanRequests() == BMS_REQ_ID_NORMAL) {
                    bms_state.powerPath = BMS_POWER_PATH_0;
                    bms_state.nextState = BMS_FSM_STATE_DISCHARGE;
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_PRECHARGE;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                }
                if (BMS_CheckCanRequests() == BMS_REQ_ID_CHARGE) {
                    bms_state.powerPath = BMS_POWER_PATH_1;
                    bms_state.nextState = BMS_FSM_STATE_CHARGE;
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_PRECHARGE;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
#if BS_STANDBY_PERIODIC_OPEN_WIRE_CHECK == TRUE
                    if (nextOpenWireCheck <= timestamp) {
                        MEAS_RequestOpenWireCheck();
                        nextOpenWireCheck = timestamp + BS_STANDBY_OPEN_WIRE_PERIOD_ms;
                    }
#endif /* BS_STANDBY_PERIODIC_OPEN_WIRE_CHECK == TRUE */
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                    break;
                }
            } else {
                FAS_ASSERT(FAS_TRAP);
            }
            break;

        /****************************预充********************************/
        case BMS_FSM_STATE_PRECHARGE:
            BMS_SAVE_LAST_STATES();

            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                DATA_READ_DATA(&systemState);
                systemState.bmsCanState = BMS_CAN_STATE_PRECHARGE;
                DATA_WRITE_DATA(&systemState);
                if (bms_state.nextState == BMS_FSM_STATE_CHARGE) {
                    stringNumber = BMS_GetLowestString(BMS_TAKE_PRECHARGE_INTO_ACCOUNT, &bms_tablePackValues);
                } else {
                    stringNumber = BMS_GetHighestString(BMS_TAKE_PRECHARGE_INTO_ACCOUNT, &bms_tablePackValues);
                }
                if (stringNumber == BMS_NO_STRING_AVAILABLE) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                }
                bms_state.prechargeTryCounter = 0u;
                bms_state.firstClosedString   = stringNumber;
                if (bms_state.OscillationTimeout == 0u) {
                    /* 闭合 MINUS 串接触器 */
                    if (CONT_CloseContactor(bms_state.firstClosedString, CONT_MINUS) == STD_OK) {
                        bms_state.stringCloseTimeout = BMS_STRING_CLOSE_TIMEOUT;
                        bms_state.timer              = BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR;
                        bms_state.substate           = BMS_FSM_SUBSTATE_PRECHARGE_CLOSE_PRECHARGE;
                    } else {
                        /* 请求了无效的接触器 */
                        bms_state.timer     = BMS_FSM_SHORTTIME;
                        bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                        bms_state.nextState = BMS_FSM_STATE_ERROR;
                        bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    }
                } else if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    /* 如果预充重入超时未过，则等待（并在等待时检查错误） */
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_PRECHARGE_CLOSE_PRECHARGE) {
                /* 检查 MINUS 接触器是否已成功闭合 */
                contactorState = CONT_GetContactorState(bms_state.firstClosedString, CONT_MINUS);
                if (contactorState == CONT_SWITCH_ON) {
                    bms_state.OscillationTimeout = BMS_OSCILLATION_TIMEOUT;
                    contRetVal                   = CONT_ClosePrecharge(bms_state.firstClosedString);
                    bms_state.closedPrechargeContactors[stringNumber] = true;
                    if (contRetVal == STD_OK) {
                        /* 负极接触器成功闭合且已发送闭合预充接触器的请求
                         * -> 保存时间戳并监控预充过程 */
                        bms_state.startOfPrecharging = bms_state.currentSystick;
                        bms_state.timer              = BMS_FSM_SHORTTIME;
                        bms_state.substate           = BMS_FSM_SUBSTATE_PRECHARGE_CHECK_PRECHARGE_PROCESS;
                    } else {
                        bms_state.timer     = BMS_FSM_SHORTTIME;
                        bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                        bms_state.nextState = BMS_FSM_STATE_ERROR;
                        bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    }
                } else if (bms_state.stringCloseTimeout == 0u) {
                    /* 串闭合花费时间过长 */
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                } else {
                    /* 串未闭合，重新发出闭合请求 */
                    CONT_CloseContactor(bms_state.firstClosedString, CONT_MINUS);
                    bms_state.timer = BMS_FSM_SHORTTIME;
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_PRECHARGE_CHECK_PRECHARGE_PROCESS) {
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    /* 检测到错误：中止并且不再继续监控预充过程 */
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                }
                if (BMS_CheckCanRequests() == BMS_REQ_ID_STANDBY) {
                    /* 接收到接触器断开请求：在此中止并且不再继续监控预充过程 */
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_STANDBY;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                }

                contactorState = CONT_GetContactorState(bms_state.firstClosedString, CONT_PRECHARGE);
                /* 监控预充 */
                prechargeRetval = BMS_MonitorPrechargeProcess(
                    bms_state.firstClosedString,
                    &bms_tablePackValues,
                    BMS_PRECHARGE_MONITORING_PARAMETERS,
                    BMS_MAXIMUM_PRECHARGE_DURATION_ms);
                /* 检查预充接触器是否闭合且预充已完成 */
                if ((contactorState == CONT_SWITCH_ON) && (prechargeRetval == BMS_PRECHARGING_FINISHED)) {
                    /* 预充成功。闭合串 PLUS 接触器 */
                    CONT_CloseContactor(bms_state.firstClosedString, CONT_PLUS);
                    bms_state.stringCloseTimeout = BMS_STRING_CLOSE_TIMEOUT;
                    bms_state.timer              = BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR;
                    bms_state.substate           = BMS_FSM_SUBSTATE_CHECK_CLOSE_SECOND_STRING_CONTACTOR_PRECHARGE_STATE;
                    break;
                } else if (prechargeRetval == BMS_PRECHARGING_ONGOING) {
                    /* 保持此状态直到预充成功或达到超时 */
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_PRECHARGE_CHECK_PRECHARGE_PROCESS;
                    break;
                } else {
                    /* 预充失败。达到超时。断开预充接触器。 */
                    contRetVal = CONT_OpenPrecharge(bms_state.firstClosedString);
                    /* 检查是否达到重试限制 */
                    if (bms_state.prechargeTryCounter < (BMS_PRECHARGE_TRIES - 1u)) {
                        bms_state.closedPrechargeContactors[stringNumber] = false;
                        if (contRetVal == STD_OK) {
                            bms_state.timer    = BMS_TIME_WAIT_AFTER_PRECHARGE_FAIL;
                            bms_state.substate = BMS_FSM_SUBSTATE_PRECHARGE_CLOSE_PRECHARGE;
                            bms_state.prechargeTryCounter++;
                        } else {
                            bms_state.timer     = BMS_FSM_SHORTTIME;
                            bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                            bms_state.nextState = BMS_FSM_STATE_ERROR;
                            bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                        }
                        break;
                    } else {
                        bms_state.closedPrechargeContactors[stringNumber] = false;
                        bms_state.timer                                   = BMS_FSM_SHORTTIME;
                        bms_state.state                                   = BMS_FSM_STATE_OPEN_CONTACTORS;
                        bms_state.nextState                               = BMS_FSM_STATE_ERROR;
                        bms_state.substate                                = BMS_FSM_SUBSTATE_ENTRY;
                        break;
                    }
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_CLOSE_SECOND_STRING_CONTACTOR_PRECHARGE_STATE) {
                contactorState = CONT_GetContactorState(bms_state.firstClosedString, CONT_PLUS);
                if (contactorState == CONT_SWITCH_ON) {
                    bms_state.closedStrings[bms_state.firstClosedString] = true;
                    bms_state.numberOfClosedStrings++;
                    bms_state.timer    = BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_CLOSING_STRINGS;
                    break;
                } else if (bms_state.stringCloseTimeout == 0u) {
                    /* 串闭合花费时间过长 */
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    /* 串未闭合，重新发出闭合请求 */
                    CONT_CloseContactor(bms_state.firstClosedString, CONT_PLUS);
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_FIRST_STRING;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_FIRST_STRING) {
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_CLOSE_SECOND_STRING_CONTACTOR_PRECHARGE_STATE;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_CLOSING_STRINGS) {
                /* 在首个串成功闭合后始终进行一次错误检查 */
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_PRECHARGE_OPEN_PRECHARGE;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_PRECHARGE_OPEN_PRECHARGE) {
                contRetVal = CONT_OpenPrecharge(bms_state.firstClosedString);
                if (contRetVal == STD_OK) {
                    bms_state.closedPrechargeContactors[stringNumber] = false;
                    bms_state.timer                                   = BMS_TIME_WAIT_AFTER_OPENING_PRECHARGE;
                    bms_state.substate                                = BMS_FSM_SUBSTATE_PRECHARGE_CHECK_OPEN_PRECHARGE;
                } else {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_PRECHARGE_CHECK_OPEN_PRECHARGE) {
                contactorState = CONT_GetContactorState(bms_state.firstClosedString, CONT_PRECHARGE);
                if (contactorState == CONT_SWITCH_OFF) {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.state    = BMS_FSM_STATE_NORMAL;
                    bms_state.substate = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else if (bms_state.stringCloseTimeout == 0u) {
                    /* 预充接触器断开花费时间过长 */
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    /* 预充接触器未断开，重新发出断开请求 */
                    CONT_OpenPrecharge(bms_state.firstClosedString);
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_FIRST_STRING;
                    break;
                }
            } else {
                FAS_ASSERT(FAS_TRAP);
            }
            break;

        /****************************正常**************************************/
        case BMS_FSM_STATE_NORMAL:
            BMS_SAVE_LAST_STATES();

            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
#if BS_NORMAL_PERIODIC_OPEN_WIRE_CHECK == TRUE
                nextOpenWireCheck = timestamp + BS_NORMAL_OPEN_WIRE_PERIOD_ms;
#endif /* BS_NORMAL_PERIODIC_OPEN_WIRE_CHECK == TRUE */
                DATA_READ_DATA(&systemState);
                if (bms_state.nextState == BMS_FSM_STATE_CHARGE) {
                    systemState.bmsCanState = BMS_CAN_STATE_CHARGE;
                } else {
                    systemState.bmsCanState = BMS_CAN_STATE_NORMAL;
                }
                DATA_WRITE_DATA(&systemState);
                bms_state.timer                 = BMS_FSM_SHORTTIME;
                bms_state.substate              = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                bms_state.nextStringClosedTimer = 0u;
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS) {
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS) {
                if (BMS_CheckCanRequests() == BMS_REQ_ID_STANDBY) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_STANDBY;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
#if BS_NORMAL_PERIODIC_OPEN_WIRE_CHECK == TRUE
                    if (nextOpenWireCheck <= timestamp) {
                        MEAS_RequestOpenWireCheck();
                        nextOpenWireCheck = timestamp + BS_NORMAL_OPEN_WIRE_PERIOD_ms;
                    }
#endif /* BS_NORMAL_PERIODIC_OPEN_WIRE_CHECK == TRUE */
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_NORMAL_CLOSE_NEXT_STRING;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_NORMAL_CLOSE_NEXT_STRING) {
                if (bms_state.nextStringClosedTimer == 0u) {
                    nextStringNumber =
                        BMS_GetClosestString(BMS_DO_NOT_TAKE_PRECHARGE_INTO_ACCOUNT, &bms_tablePackValues);
                    if (nextStringNumber == BMS_NO_STRING_AVAILABLE) {
                        bms_state.timer    = BMS_FSM_SHORTTIME;
                        bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                        break;
                    } else if (
                        (BMS_GetStringVoltageDifference(nextStringNumber, &bms_tablePackValues) <=
                         BMS_NEXT_STRING_VOLTAGE_LIMIT_MV) &&
                        (BMS_GetAverageStringCurrent(&bms_tablePackValues) <= BMS_AVERAGE_STRING_CURRENT_LIMIT_MA)) {
                        /* 电压/电流条件适合闭合更多串。闭合第一个串接触器 */
                        CONT_CloseContactor(nextStringNumber, CONT_MINUS);
                        bms_state.nextStringClosedTimer = BMS_STRING_CLOSE_TIMEOUT;
                        bms_state.timer                 = BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR;
                        bms_state.substate              = BMS_FSM_SUBSTATE_NORMAL_CLOSE_SECOND_STRING_CONTACTOR;
                        break;
                    }
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_NORMAL_CLOSE_SECOND_STRING_CONTACTOR) {
                contactorState = CONT_GetContactorState(nextStringNumber, CONT_MINUS);
                if (contactorState == CONT_SWITCH_ON) {
                    /* 第一个串接触器已闭合。闭合第二个串接触器 */
                    CONT_CloseContactor(nextStringNumber, CONT_PLUS);
                    bms_state.timer    = BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR;
                    bms_state.substate = BMS_FSM_SUBSTATE_NORMAL_CLOSE_SECOND_STRING_CONTACTOR;
                } else if (bms_state.stringCloseTimeout == 0u) {
                    /* 串闭合花费时间过长 */
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    /* 串负极接触器未成功闭合。重新触发闭合 */
                    CONT_CloseContactor(nextStringNumber, CONT_MINUS);
                    bms_state.timer = BMS_FSM_SHORTTIME;
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_STRING_CLOSED) {
                contactorState = CONT_GetContactorState(nextStringNumber, CONT_PLUS);
                if (contactorState == CONT_SWITCH_ON) {
                    bms_state.numberOfClosedStrings++;
                    bms_state.closedStrings[nextStringNumber] = true;
                    bms_state.nextStringClosedTimer           = BMS_WAIT_TIME_BETWEEN_CLOSING_STRINGS;
                    /* 转到 NORMAL 情况的开头以重新执行带有错误检查和请求检查的完整过程 */
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                    break;
                } else if (bms_state.stringCloseTimeout == 0u) {
                    /* 串闭合花费时间过长 */
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else if (BMS_CheckCanRequests() == BMS_REQ_ID_STANDBY) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_STANDBY;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    /* 串未闭合，重新发出闭合请求 */
                    CONT_CloseContactor(nextStringNumber, CONT_PLUS);
                    bms_state.timer = BMS_FSM_SHORTTIME;
                    break;
                }
            } else {
                FAS_ASSERT(FAS_TRAP);
            }
            break;

        /****************************错误*************************************/
        case BMS_FSM_STATE_ERROR:
            BMS_SAVE_LAST_STATES();

            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                /* 将 BMS 系统状态设置为错误 */
                DATA_READ_DATA(&systemState);
                systemState.bmsCanState = BMS_CAN_STATE_ERROR;
                DATA_WRITE_DATA(&systemState);
                /* 停用均衡 */
                BAL_SetStateRequest(BAL_STATE_NO_BALANCING_REQUEST);
                /* 更改 LED 切换频率以指示错误 */
                LED_SetToggleTime(LED_ERROR_OPERATION_ON_OFF_TIME_ms);
                /* 设置下次开路检查的计时器 */
                nextOpenWireCheck = bms_state.currentSystick + AFE_ERROR_OPEN_WIRE_PERIOD_ms;
                /* 切换到下一个子状态 */
                bms_state.timer    = BMS_FSM_SHORTTIME;
                bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS) {
                if (DIAG_IsAnyFatalErrorSet() == true) {
                    /* 我们已处于请求的状态 */
                    if (nextOpenWireCheck <= bms_state.currentSystick) {
                        /* 定期执行开路检查 */
                        /* MEAS_RequestOpenWireCheck(); */ /*TODO: 检查串 */
                        nextOpenWireCheck = bms_state.currentSystick + AFE_ERROR_OPEN_WIRE_PERIOD_ms;
                    }
                } else {
                    /* 不再检测到错误 - 重置致命错误相关变量 */
                    bms_state.minimumActiveDelay_ms  = BMS_NO_ACTIVE_DELAY_TIME_ms;
                    bms_state.minimumActiveDelay_ms  = BMS_NO_ACTIVE_DELAY_TIME_ms;
                    bms_state.transitionToErrorState = false;
                    /* 检查 STANDBY 请求 */
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS) {
                if (BMS_CheckCanRequests() == BMS_REQ_ID_STANDBY) {
                    /* 再次激活均衡 */
                    BAL_SetStateRequest(BAL_STATE_ALLOW_BALANCING_REQUEST);
                    /* 将 LED 频率设置为正常操作，因为我们随后将离开错误状态 */
                    LED_SetToggleTime(LED_NORMAL_OPERATION_ON_OFF_TIME_ms);

                    /* 验证所有接触器是否已断开，然后切换到 STANDBY 状态 */
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_STANDBY;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                    break;
                }
            } else {
                /* 无效状态 -> 这永远不应达到 */
                FAS_ASSERT(FAS_TRAP);
            }
            break;
        default:
            /* 无效状态 */
            FAS_ASSERT(FAS_TRAP);
            break;
    } /* 结束 switch (bms_state.state) */

    /* 如果状态或子状态发生改变，发送异步 bms 状态消息 */
    if ((bms_state.state != bms_state.lastState) || (bms_state.substate != bms_state.lastSubstate)) {
        CANTX_TransmitBmsState();
    }

    bms_state.triggerentry--;
    bms_state.counter++;
}

extern BMS_CURRENT_FLOW_STATE_e BMS_GetBatterySystemState(void) {
    return bms_state.currentFlowState;
}

extern BMS_CURRENT_FLOW_STATE_e BMS_GetCurrentFlowDirection(int32_t current_mA) {
    /* AXIVION 常规 Generic-MissingParameterAssert: current_mA: 参数接受整个范围 */
    BMS_CURRENT_FLOW_STATE_e retVal = BMS_DISCHARGING;

    if (BS_POSITIVE_DISCHARGE_CURRENT == true) {
        if (current_mA >= BS_REST_CURRENT_mA) {
            retVal = BMS_DISCHARGING;
        } else if (current_mA <= -BS_REST_CURRENT_mA) {
            retVal = BMS_CHARGING;
        } else {
            retVal = BMS_AT_REST;
        }
    } else {
        if (current_mA <= -BS_REST_CURRENT_mA) {
            retVal = BMS_DISCHARGING;
        } else if (current_mA >= BS_REST_CURRENT_mA) {
            retVal = BMS_CHARGING;
        } else {
            retVal = BMS_AT_REST;
        }
    }
    return retVal;
}

extern bool BMS_IsStringClosed(uint8_t stringNumber) {
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    bool retval = false;
    if (bms_state.closedStrings[stringNumber] == true) {
        retval = true;
    }
    return retval;
}

extern bool BMS_IsStringPrecharging(uint8_t stringNumber) {
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    bool retval = false;
    if (bms_state.closedPrechargeContactors[stringNumber] == true) {
        retval = true;
    }
    return retval;
}

extern uint8_t BMS_GetNumberOfConnectedStrings(void) {
    return bms_state.numberOfClosedStrings;
}

extern bool BMS_IsTransitionToErrorStateActive(void) {
    return bms_state.transitionToErrorState;
}

/*========== 外部化的静态函数实现 (单元测试) =======*/
#ifdef UNITY_UNIT_TEST
extern BMS_RETURN_TYPE_e TEST_BMS_CheckStateRequest(BMS_STATE_REQUEST_e statereq) {
    return BMS_CheckStateRequest(statereq);
}
extern BMS_STATE_REQUEST_e TEST_BMS_TransferStateRequest(void) {
    return BMS_TransferStateRequest();
}
extern uint8_t TEST_BMS_CheckReEntrance(void) {
    return BMS_CheckReEntrance();
}
extern uint8_t TEST_BMS_CheckCanRequests(void) {
    return BMS_CheckCanRequests();
}
extern bool TEST_BMS_IsAnyFatalErrorFlagSet(void) {
    return BMS_IsAnyFatalErrorFlagSet();
}
extern STD_RETURN_TYPE_e TEST_BMS_IsBatterySystemStateOkay(void) {
    return BMS_IsBatterySystemStateOkay();
}
extern bool TEST_BMS_IsContactorFeedbackValid(uint8_t stringNumber, CONT_TYPE_e contactorType) {
    return BMS_IsContactorFeedbackValid(stringNumber, contactorType);
}
extern void TEST_BMS_GetMeasurementValues(void) {
    BMS_GetMeasurementValues();
}
extern void TEST_BMS_CheckOpenSenseWire(void) {
    BMS_CheckOpenSenseWire();
}
extern STD_RETURN_TYPE_e TEST_BMS_MonitorPrechargeProcess(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues,
    BS_PRECHARGE_MONITORING_e monitoringParameters,
    uint32_t timeout_ms) {
    return BMS_MonitorPrechargeProcess(stringNumber, pPackValues, monitoringParameters, timeout_ms);
}
extern uint8_t TEST_BMS_GetHighestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    return BMS_GetHighestString(precharge, pPackValues);
}
extern uint8_t TEST_BMS_GetClosestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    return BMS_GetClosestString(precharge, pPackValues);
}

extern uint8_t TEST_BMS_GetLowestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    return BMS_GetLowestString(precharge, pPackValues);
}
extern int32_t TEST_BMS_GetStringVoltageDifference(uint8_t string, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    return BMS_GetStringVoltageDifference(string, pPackValues);
}
extern int32_t TEST_BMS_GetAverageStringCurrent(DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    return BMS_GetAverageStringCurrent(pPackValues);
}
extern void TEST_BMS_UpdateBatterySystemState(DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    BMS_UpdateBatterySystemState(pPackValues);
}

#endif

