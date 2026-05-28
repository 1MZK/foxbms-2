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

// BMS_STATE_s 是BMS状态机的核心结构体类型。
// static 表示该变量仅限当前 .c 文件使用，这是状态机封装的标准做法，防止外部文件直接篡改状态。
static BMS_STATE_s bms_state = {
    // --- 时间与节拍 ---
    .currentSystick                    = 0u,  // 当前系统滴答计时器，记录系统运行的总时间基础
    .timer                             = 0u,  // 状态机通用定时器（就是你要赋值 BMS_FSM_MEDIUM_TIME 的那个变量），用于状态内计时/超时判断
    .nextStringClosedTimer             = 0u,  // 下一个电池簇闭合的等待计时器
    .stringOpenTimeout                 = 0u,  // 电池簇断开超时计时器
    .stringCloseTimeout                = 0u,  // 电池簇闭合超时计时器
    .restTimer_10ms                    = BS_RELAXATION_PERIOD_10ms, // 静置延时计时器（10ms为单位），用于电芯电压弛豫/恢复期，防止误判
    .timeAboveContactorBreakCurrent_ms = 0u,  // 电流超过接触器分断电流的持续时间（保护接触器，防止带载拉弧断开）

    // --- 状态机核心状态 ---
    .stateRequest                      = BMS_STATE_NO_REQUEST,     // 外部请求的状态转换标志（如请求进入Standby），初始化为无请求
    .state                             = BMS_FSM_STATE_UNINITIALIZED, // 当前主状态：未初始化
    .substate                          = BMS_FSM_SUBSTATE_ENTRY,   // 当前子状态：入口状态（状态机进入某个主状态时的第一个动作）
    .lastState                         = BMS_FSM_STATE_UNINITIALIZED, // 上一次的主状态，用于追溯和防抖
    .lastSubstate                      = BMS_FSM_SUBSTATE_ENTRY,   // 上一次的子状态
    .nextState                         = BMS_FSM_STATE_STANDBY,    // 准备跳转的下一个状态，初始化指向待机状态

    // --- 标志位与计数器 ---
    .triggerentry                      = 0u,   // 状态触发入口标志
    .ErrRequestCounter                 = 0u,   // 错误请求计数器，可能用于错误防抖（连续检测到N次错误才真正报错）
    .counter                           = 0u,   // 通用计数器
    .OscillationTimeout                = 0u,   // 状态振荡超时（如果状态频繁在两个状态间来回跳转，触发保护）
    .prechargeTryCounter               = 0u,   // 预充尝试计数器（预充失败后允许重试几次，超过次数则彻底报错）
    .initFinished                      = STD_NOT_OK, // 初始化完成标志，初始化为未完成(STD_NOT_OK)
    .startOfPrecharging                = 0u,   // 记录预充开始时的时间戳，用于计算预充耗时

    // --- 电池簇与接触器控制 ---
    .powerPath                         = BMS_POWER_PATH_OPEN, // 当前功率路径状态：断开（未充放电）
    .numberOfClosedStrings             = 0u,   // 当前已闭合的电池簇数量
    .firstClosedString                 = 0u,   // 第一个闭合的电池簇编号
    .stringToBeOpened                  = 0u,   // 需要被断开的电池簇编号
    .contactorToBeOpened               = CONT_UNDEFINED, // 需要被断开的接触器类型（主正/主负/预充），初始化为未定义

    // --- 数组（使用宏生成初始化值）---
    // GEN_REPEAT_U 是一个高级宏，用于生成重复的数值。
    // 例如 BS_NR_OF_STRINGS 为3，则 {GEN_REPEAT_U(false, 3)} 展开后就是 {false, false, false}
    .closedStrings                     = {GEN_REPEAT_U(false, GEN_STRIP(BS_NR_OF_STRINGS))}, // 记录每个簇的主接触器是否闭合
    .closedPrechargeContactors         = {GEN_REPEAT_U(false, GEN_STRIP(BS_NR_OF_STRINGS))}, // 记录每个簇的预充接触器是否闭合
    .deactivatedStrings                = {GEN_REPEAT_U(false, GEN_STRIP(BS_NR_OF_STRINGS))}, // 记录被禁用/隔离的电池簇（如电压过低或温度过高被切除）

    // --- 延时与电流状态 ---
    .currentFlowState                  = BMS_RELAXATION, // 当前电流流向状态：弛豫（静置，无电流）
    .remainingDelay_ms                 = BMS_NO_ACTIVE_DELAY_TIME_ms, // 剩余延时时间
    .minimumActiveDelay_ms             = BMS_NO_ACTIVE_DELAY_TIME_ms, // 最小激活延时时间
    .transitionToErrorState            = false, // 是否需要跳转到错误状态的标志
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

/**
 * @brief  BMS状态机重入检查函数（防并发调用保护）
 * @retval 0: 函数未被占用，成功获取执行权
 * @retval 0xFF: 函数已被占用，发生了重入（多次调用），获取执行权失败
 */
static uint8_t BMS_CheckReEntrance(void) {
    uint8_t retval = 0; // 默认返回值：0表示正常，允许执行

    /* 进入临界区：关闭系统中断或锁定任务调度器。
     * 这一步极其关键！确保接下来的判断和修改不会被任何其他任务或中断打断。*/
    OS_EnterTaskCritical();

    /* 检查触发入口标志位 triggerentry */
    if (!bms_state.triggerentry) { 
        /* 如果 triggerentry 为 0，说明当前没有其他任务在执行BMS状态机核心逻辑 */
        bms_state.triggerentry++; // 将标志位置 1（加锁），表示“我正在使用，别人别进来”
    } else {
        /* 如果 triggerentry 不为 0（已经是1或更大），说明已经有任务在执行了，
         * 但现在又被另一个任务/中断触发了，发生了“重入”！*/
        retval = 0xFF; /* 返回错误码 0xFF，告诉调用者：我正忙着，别烦我 */
    }

    /* 退出临界区：恢复系统中断和任务调度 */
    OS_ExitTaskCritical();

    return retval;
}


/**
 * @brief  转移（读取并清零）BMS状态请求
 * @retval 最近一次的状态请求枚举值
 * @note   这个函数采用了“读取即销毁”的原子操作模式，非常经典
 */
static BMS_STATE_REQUEST_e BMS_TransferStateRequest(void) {
    BMS_STATE_REQUEST_e retval = BMS_STATE_NO_REQUEST; // 默认无请求

    /* 进入临界区：关中断/锁调度器。
     * 为什么这里必须加锁？因为 stateRequest 可能被异步中断（如CAN接收）修改。
     * 如果不加锁，可能出现：刚把 stateRequest 读给 retval，还没来得及清零，
     * 突然中断发生，修改了 stateRequest，等回来后再清零，就把新的请求给误删了！*/
    OS_EnterTaskCritical();
    
    retval                 = bms_state.stateRequest; // 1. 取出当前的请求
    bms_state.stateRequest = BMS_STATE_NO_REQUEST;  // 2. 立刻清空请求（类似取信箱里的信，取走后信箱就空了）
    
    OS_ExitTaskCritical(); // 退出临界区

    return retval; // 返回取到的请求，交给状态机去处理
}


/**
 * @brief  获取底层传感器/从控采集的实时数据
 * @note   这是一个典型的数据解耦设计
 */
static void BMS_GetMeasurementValues(void) {
    /* DATA_READ_DATA 是一个底层数据访问接口（通常操作的是共享RAM或数据库）。
     * 这里一次性读取了三组数据到全局/静态变量中供状态机使用：
     * 
     * 1. bms_tablePackValues: 电池包总数据（总压、总流、绝缘阻抗等）
     * 2. bms_tableOpenWire:   开路/断线检测表（检测传感器连线是否脱落）
     * 3. bms_tableMinMax:     极值数据表（最高单体电压、最低单体电压、最高/低温度等）
     * 
     * 这种设计将“数据采集”（在后台周期性执行）和“状态机逻辑”解耦，
     * 状态机不需要关心数据是怎么通过SPI/CAN从从控(AFE)读上来的，只管拿现成的结果。*/
    DATA_READ_DATA(&bms_tablePackValues, &bms_tableOpenWire, &bms_tableMinMax);
}


/**
 * @brief  检查VCU（整车控制器）通过CAN网络发来的状态切换请求
 * @retval 请求ID：BMS_REQ_ID_STANDBY(待机), NORMAL(正常工作), CHARGE(充电), NOREQ(无请求)
 */
static uint8_t BMS_CheckCanRequests(void) {
    uint8_t retVal = BMS_REQ_ID_NOREQ; // 默认无请求
    
    /* 定义一个本地结构体，并初始化其唯一ID。
     * DATA_BLOCK_ID_STATE_REQUEST 告诉底层数据库：“我要读的是CAN请求状态这块数据”*/
    DATA_BLOCK_STATE_REQUEST_s request = {.header.uniqueId = DATA_BLOCK_ID_STATE_REQUEST};

    /* 从数据库中读取最新的CAN请求报文内容到本地变量 request 中 */
    DATA_READ_DATA(&request);

    /* 解析CAN报文中的请求字段，转换为BMS内部的状态请求标识 */
    if (request.stateRequestViaCan == BMS_REQ_ID_STANDBY) {
        retVal = BMS_REQ_ID_STANDBY;     // VCU要求BMS进入待机（高压下电）
    } else if (request.stateRequestViaCan == BMS_REQ_ID_NORMAL) {
        retVal = BMS_REQ_ID_NORMAL;      // VCU要求BMS进入正常行车模式（高压上电，准备放电）
    } else if (request.stateRequestViaCan == BMS_REQ_ID_CHARGE) {
        retVal = BMS_REQ_ID_CHARGE;      // VCU要求BMS进入充电模式（连接充电枪）
    } else if (request.stateRequestViaCan == BMS_REQ_ID_NOREQ) {
        retVal = BMS_REQ_ID_NOREQ;       // VCU没有发送任何切换请求
    } else {
        /* 接收到了未定义的请求ID，可能是报文错误，默认当作无请求处理 */
    }

    return retVal; // 返回解析后的请求
}


/**
 * @brief  检查所有电池簇的电压采样线（飞线/感测线）是否断开
 * @note   采样线断路是极其危险的故障，会导致BMS无法感知真实电压，可能引发过充爆炸。
 */
static void BMS_CheckOpenSenseWire(void) {
    uint8_t openWireDetected = 0; // 断线计数器

    /* 遍历所有电池簇 */
    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
        /* 遍历当前簇的所有模组 */
        for (uint8_t m = 0u; m < BS_NR_OF_MODULES_PER_STRING; m++) {
            /* 遍历模组上的所有感测线。
             * 注意：N个电芯需要N+1根感测线（最两端各1根，中间每两个电芯共享1根）*/
            for (uint8_t wire = 0u; wire < (BS_NR_OF_CELL_BLOCKS_PER_MODULE + 1); wire++) {
                
                /* ⚠️ 警告：这里原代码有一处疑似Bug的括号位置问题！
                 * 原代码: bms_tableOpenWire.openWire[s][(wire + (m * ...)) == 1] > 0u
                 * 实际执行: (wire + (m * ...)) == 1 会先计算，结果为布尔值(0或1)作为数组索引！
                 * 正确逻辑应该是: bms_tableOpenWire.openWire[s][wire + (m * ...)] > 0u */
                if (bms_tableOpenWire.openWire[s][(wire + (m * (BS_NR_OF_CELL_BLOCKS_PER_MODULE + 1)))] > 0u) {
                    openWireDetected++; // 检测到开路，计数增加
                    /* 在此处添加额外的错误处理，如记录具体是哪根线断了 */
                }
            }
        }
        
        /* 针对当前簇，将断线结果上报给诊断管理模块 */
        if (openWireDetected == 0u) {
            // 没有断线，上报正常
            DIAG_Handler(DIAG_ID_AFE_OPEN_WIRE, DIAG_EVENT_OK, DIAG_STRING, s);
        } else {
            // 存在断线，上报故障
            DIAG_Handler(DIAG_ID_AFE_OPEN_WIRE, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
        }
    }
}


/**
 * @brief  监控预充过程是否成功或超时失败
 * @param  stringNumber: 簇编号
 * @param  pPackValues:  实时采样的电压电流数据指针
 * @param  monitoringParameters: 监控策略（仅监控电流/仅监控电压/两者都监控）
 * @param  timeout_ms: 预充超时时间（毫秒）
 * @retval 预充状态：正在进行 / 已完成 / 失败
 */
static BMS_RESULT_PRECHARGE_PROCESS_e BMS_MonitorPrechargeProcess(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues,
    BS_PRECHARGE_MONITORING_e monitoringParameters,
    uint32_t timeout_ms) {

    /* 入参防御性编程，FAS_ASSERT 类似断言，调试阶段若越界直接死机暴露问题 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    FAS_ASSERT(pPackValues != NULL_PTR);
    FAS_ASSERT((monitoringParameters == BS_PRECHARGE_MONITOR_CURRENT) ||
               (monitoringParameters == BS_PRECHARGE_MONITOR_VOLTAGE) ||
               (monitoringParameters == BS_PRECHARGE_MONITOR_CURRENT_AND_VOLTAGE));

    BMS_RESULT_PRECHARGE_PROCESS_e prechargingState = BMS_PRECHARGING_ONGOING; // 默认：预充正在进行
    STD_RETURN_TYPE_e currentPrecharged             = STD_NOT_OK; // 电流条件未满足
    STD_RETURN_TYPE_e voltagePrecharged             = STD_NOT_OK; // 电压条件未满足

    /* 根据监控策略，调用对应的判据函数 */
    if (monitoringParameters == BS_PRECHARGE_MONITOR_CURRENT) {
        currentPrecharged = BMS_IsPrechargeCurrentBelowLimit(stringNumber, pPackValues);
        voltagePrecharged = STD_OK; // 不监控电压，直接默认满足
    } else if (monitoringParameters == BS_PRECHARGE_MONITOR_VOLTAGE) {
        voltagePrecharged = BMS_IsPrechargeVoltageBelowLimit(stringNumber, pPackValues);
        currentPrecharged = STD_OK; // 不监控电流，直接默认满足
    } else {
        /* 两者都监控 */
        currentPrecharged = BMS_IsPrechargeCurrentBelowLimit(stringNumber, pPackValues);
        voltagePrecharged = BMS_IsPrechargeVoltageBelowLimit(stringNumber, pPackValues);
    }

    /* 判定预充成功条件：电流和电压条件均满足 */
    if ((currentPrecharged == STD_OK) && (voltagePrecharged == STD_OK)) {
        prechargingState = BMS_PRECHARGING_FINISHED; // 预充完成
        /* 上报诊断：预充正常结束，清除可能的超时故障标志 */
        (void)DIAG_Handler(DIAG_ID_PRECHARGE_ABORT_REASON_VOLTAGE, DIAG_EVENT_OK, DIAG_STRING, stringNumber);
        (void)DIAG_Handler(DIAG_ID_PRECHARGE_ABORT_REASON_CURRENT, DIAG_EVENT_OK, DIAG_STRING, stringNumber);
    } else {
        /* 条件未满足，检查是否超时 */
        if (bms_state.currentSystick - bms_state.startOfPrecharging > timeout_ms) {
            prechargingState = BMS_PRECHARGING_FAILED; // 预充超时失败
            /* 上报诊断：明确告诉系统是因为电压不达标还是电流不达标导致的失败 */
            DIAG_CheckEvent(currentPrecharged, DIAG_ID_PRECHARGE_ABORT_REASON_CURRENT, DIAG_STRING, stringNumber);
            DIAG_CheckEvent(voltagePrecharged, DIAG_ID_PRECHARGE_ABORT_REASON_VOLTAGE, DIAG_STRING, stringNumber);
        }
    }
    return prechargingState;
}


/**
 * @brief  判断预充电流是否低于门限（预充接近完成时，电流会趋近于0）
 */
static STD_RETURN_TYPE_e BMS_IsPrechargeCurrentBelowLimit(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues) {

    STD_RETURN_TYPE_e retval = STD_NOT_OK;
    /* 条件1：电流采样值有效；条件2：电流绝对值小于设定阈值 (MATH_AbsInt32_t取绝对值，不区分充放电方向) */
    if ((pPackValues->invalidStringCurrent[stringNumber] == 0u) &&
        ((MATH_AbsInt32_t(pPackValues->stringCurrent_mA[stringNumber]) < BMS_PRECHARGE_CURRENT_THRESHOLD_mA))) {
        retval = STD_OK;
    }
    return retval;
}

/**
 * @brief  判断预充电压差是否低于门限（母线电压逼近电池电压，压差趋近于0）
 */
static STD_RETURN_TYPE_e BMS_IsPrechargeVoltageBelowLimit(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues) {

    STD_RETURN_TYPE_e retval = STD_NOT_OK;
    /* 条件1：簇电压有效；条件2：母线高压有效 */
    if ((pPackValues->invalidStringVoltage[stringNumber] == 0u) && (pPackValues->invalidHvBusVoltage == 0u)) {
        /* 计算电池簇电压与高压母线电压之间的压差 (绝对值) */
        const int64_t cont_prechargeVoltDiff_mV = MATH_AbsInt64_t(
            (int64_t)pPackValues->stringVoltage_mV[stringNumber] - (int64_t)pPackValues->highVoltageBusVoltage_mV);
        
        /* 条件3：压差小于设定阈值 */
        if (cont_prechargeVoltDiff_mV < BMS_PRECHARGE_VOLTAGE_THRESHOLD_mV) {
            retval = STD_OK;
        }
    }
    return retval;
}


/**
 * @brief  检查系统中是否存在致命错误，并找出最短的安全动作延迟
 */
static bool BMS_IsAnyFatalErrorFlagSet(void) {
    bool fatalErrorActive = false;

    /* 遍历所有致命错误诊断表 */
    for (uint16_t entry = 0u; entry < diag_device.numberOfFatalErrors; entry++) {
        const STD_RETURN_TYPE_e diagnosisState =
            DIAG_GetDiagnosisEntryState(diag_device.pFatalErrorLinkTable[entry]->id);
            
        if (STD_NOT_OK == diagnosisState) { // 发现致命错误
            /* 获取该故障对应的接触器断开延迟时间（不同故障允许的延时可能不同） */
            const uint32_t kDelay_ms = DIAG_GetDelay(diag_device.pFatalErrorLinkTable[entry]->id);
            
            /* 找出当前所有激活故障中，最短的那个延迟（木桶效应，按最紧急的处理） */
            if (bms_state.minimumActiveDelay_ms > kDelay_ms) {
                bms_state.minimumActiveDelay_ms = kDelay_ms;
            }
            fatalErrorActive = true;
        }
    }
    return fatalErrorActive;
}

/**
 * @brief  评估电池系统状态是否安全，核心是处理"故障安全延时倒计时"
 * @retval STD_OK: 系统安全或延时未到; STD_NOT_OK: 延时耗尽，必须立刻进入错误状态断开高压
 */
static STD_RETURN_TYPE_e BMS_IsBatterySystemStateOkay(void) {
    STD_RETURN_TYPE_e retVal          = STD_OK; 
    static uint32_t previousTimestamp = 0u; // 静态变量，记录上次调用的时间戳
    uint32_t timestamp                = OS_GetTickCount(); // 获取当前系统时间

    /* 第一步：检查是否有致命错误，并更新最短延时 */
    const bool isErrorActive = BMS_IsAnyFatalErrorFlagSet();

    /* 第二步：状态机逻辑 - 处理延时倒计时 */
    if (bms_state.transitionToErrorState == true) {
        /* 情况A：之前已经检测到故障，正在倒计时中 */
        
        /* 计算距离上次调用过去的时间 */
        const uint32_t timeSinceLastCall_ms = timestamp - previousTimestamp;
        /* 递减剩余延时 */
        if (timeSinceLastCall_ms <= bms_state.remainingDelay_ms) {
            bms_state.remainingDelay_ms -= timeSinceLastCall_ms;
        } else {
            bms_state.remainingDelay_ms = 0u; // 防止下溢出
        }

        /* 如果在倒计时期间，又新发生了更严重的故障（延时更短），则将剩余延时缩短为新故障的延时 */
        if (bms_state.remainingDelay_ms >= bms_state.minimumActiveDelay_ms) {
            bms_state.remainingDelay_ms = bms_state.minimumActiveDelay_ms;
        }
    } else {
        /* 情况B：之前没有故障，这是新检测到的故障 */
        if (isErrorActive == true) {
            bms_state.transitionToErrorState = true; // 标记准备进入错误状态
            bms_state.remainingDelay_ms      = bms_state.minimumActiveDelay_ms; // 装载最短延时
        }
    }

    previousTimestamp = timestamp; // 更新时间戳

    /* 第三步：判断延时是否归零。一旦归零，返回系统不安全(STD_NOT_OK) */
    if ((bms_state.transitionToErrorState == true) && (bms_state.remainingDelay_ms == 0u)) {
        retVal = STD_NOT_OK; // 通知状态机：立刻断开接触器！
    }

    return retVal;
}


/**
 * @brief  检查指定接触器的状态反馈（辅助触点/回采）是否有效
 * @param  stringNumber: 簇编号
 * @param  contactorType: 接触器类型（主正/主负/预充）
 * @retval true: 反馈有效; false: 反馈存在错误
 */
static bool BMS_IsContactorFeedbackValid(uint8_t stringNumber, CONT_TYPE_e contactorType) {
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    FAS_ASSERT(contactorType != CONT_UNDEFINED); // 防御性编程，不允许传入未定义类型
    bool feedbackValid = false;

    /* 从全局数据库读取最新的硬件错误标志 */
    DATA_BLOCK_ERROR_STATE_s tableErrorFlags = {.header.uniqueId = DATA_BLOCK_ID_ERROR_STATE};
    DATA_READ_DATA(&tableErrorFlags);

    /* 根据接触器类型，检查对应的反馈错误标志位 */
    switch (contactorType) {
        case CONT_PLUS: // 主正接触器
            if (tableErrorFlags.contactorInPositivePathOfStringFeedbackError[stringNumber] == false) {
                feedbackValid = true; // 没有错误标志，说明反馈有效
            }
            break;
        case CONT_MINUS: // 主负接触器
            if (tableErrorFlags.contactorInNegativePathOfStringFeedbackError[stringNumber] == false) {
                feedbackValid = true;
            }
            break;
        case CONT_PRECHARGE: // 预充接触器
            if (tableErrorFlags.prechargeContactorFeedbackError[stringNumber] == false) {
                feedbackValid = true;
            }
            break;
        default:
            break;
    }
    return feedbackValid;
}


/**
 * @brief  在所有未禁用的簇中，找到电压最高的那个簇
 * @param  precharge: 是否只考虑带有预充支路的簇
 * @param  pPackValues: 实时电压数据
 * @retval 电压最高簇的索引，如果没有可用簇则返回 BMS_NO_STRING_AVAILABLE
 */
static uint8_t BMS_GetHighestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);
    uint8_t highest_string_index = BMS_NO_STRING_AVAILABLE;
    int32_t max_stringVoltage_mV = INT32_MIN; // 初始化为最小值，确保第一个有效电压能覆盖它

    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
        /* 条件1：当前簇电压必须大于或等于已记录的最大电压 */
        /* 条件2：当前簇电压采样值必须有效 (invalidStringVoltage == 0) */
        if ((pPackValues->stringVoltage_mV[s] >= max_stringVoltage_mV) &&
            (pPackValues->invalidStringVoltage[s] == 0u)) {
            
            /* 条件3：当前簇没有被禁用（比如因为严重故障被隔离） */
            if (bms_state.deactivatedStrings[s] == false) {
                
                /* 条件4：根据入参决定是否需要过滤掉没有预充支路的簇 */
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


/**
 * @brief  找到与当前已闭合簇（或母线）电压差最小的一个未闭合簇
 * @note   这是多簇依次并机最核心的算法：压差最小 = 闭合时的冲击电流最小
 */
static uint8_t BMS_GetClosestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);
    uint8_t closestStringIndex     = BMS_NO_STRING_AVAILABLE;
    int32_t closedStringVoltage_mV = 0; // 基准电压（已闭合簇或母线的电压）
    bool searchString              = false;

    /* 第一步：获取基准电压。优先使用首个闭合簇的电压，若无效则退求母线电压 */
    if (pPackValues->invalidStringVoltage[bms_state.firstClosedString] == 0u) {
        closedStringVoltage_mV = pPackValues->stringVoltage_mV[bms_state.firstClosedString];
        searchString           = true;
    } else if (pPackValues->invalidHvBusVoltage == 0u) {
        closedStringVoltage_mV = pPackValues->highVoltageBusVoltage_mV;
        searchString           = true;
    } else {
        searchString = false; // 采样都挂了，放弃搜索
    }

    /* 第二步：遍历所有簇，找压差最小的 */
    if (searchString == true) {
        int32_t minimumVoltageDifference_mV = INT32_MAX; // 初始化为最大值

        for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
            const bool isStringClosed          = BMS_IsStringClosed(s);
            const uint8_t isStringVoltageValid = pPackValues->invalidStringVoltage[s];
            
            /* 仅检查：未闭合的 && 电压采样有效的簇 */
            if ((isStringClosed == false) && (isStringVoltageValid == 0u)) {
                
                /* 计算当前簇与基准电压的绝对差值 */
                int32_t voltageDifference_mV = labs(closedStringVoltage_mV - pPackValues->stringVoltage_mV[s]);
                
                /* 如果压差小于或等于当前记录的最小压差 */
                if (voltageDifference_mV <= minimumVoltageDifference_mV) {
                    if (bms_state.deactivatedStrings[s] == false) { // 未被禁用
                        if (precharge == BMS_TAKE_PRECHARGE_INTO_ACCOUNT) {
                            if (bs_stringsWithPrecharge[s] == BS_STRING_WITH_PRECHARGE) {
                                minimumVoltageDifference_mV = voltageDifference_mV; // 更新最小压差
                                closestStringIndex          = s;                    // 记录候选簇
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


/**
 * @brief  找到电压最低的有效簇
 * @note   常用于充电场景：优先给最低电压的簇充电，有助于簇间均衡
 */
static uint8_t BMS_GetLowestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);
    uint8_t lowest_string_index  = BMS_NO_STRING_AVAILABLE;
    int32_t min_stringVoltage_mV = INT32_MAX; // 初始化为最大值

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


/**
 * @brief  计算指定簇与当前已并网基准之间的电压差绝对值
 * @retval 压差，若采样无效返回 INT32_MAX
 */
static int32_t BMS_GetStringVoltageDifference(uint8_t string, const DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(string < BS_NR_OF_STRINGS);
    FAS_ASSERT(pPackValues != NULL_PTR);
    int32_t voltageDifference_mV = INT32_MAX;

    /* 优先方案：计算目标簇与首个闭合簇之间的压差 */
    if ((pPackValues->invalidStringVoltage[string] == 0u) &&
        (pPackValues->invalidStringVoltage[bms_state.firstClosedString] == 0u)) {
        voltageDifference_mV = MATH_AbsInt32_t(
            pPackValues->stringVoltage_mV[string] - pPackValues->stringVoltage_mV[bms_state.firstClosedString]);
    } 
    /* 备用方案：如果首个闭合簇电压无效，则使用高压母线电压作为基准 */
    else if ((pPackValues->invalidStringVoltage[string] == 0u) && (pPackValues->invalidHvBusVoltage == 0u)) {
        voltageDifference_mV =
            MATH_AbsInt32_t(pPackValues->stringVoltage_mV[string] - pPackValues->highVoltageBusVoltage_mV);
    } else {
        /* 采样均无效，返回极大值，阻止闭合动作 */
        voltageDifference_mV = INT32_MAX;
    }
    return voltageDifference_mV;
}


/**
 * @brief  计算整个电池包的平均单簇电流
 * @retval 平均电流，若总流采样无效返回 INT32_MAX
 */
static int32_t BMS_GetAverageStringCurrent(DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);
    /* 简单的算术平均：总电流 / 簇数量 */
    int32_t average_current = pPackValues->packCurrent_mA / (int32_t)BS_NR_OF_STRINGS;
    
    /* 如果总电流采样失效，返回极值标识错误 */
    if (pPackValues->invalidPackCurrent == 1u) {
        average_current = INT32_MAX;
    }
    return average_current;
}


/**
 * @brief  根据当前电流大小和方向，更新电池系统的电流流状态（放电、充电、弛豫、静置）
 * @param  pPackValues: 实时打包数据指针
 * @note   这个函数实现了电化学中的"弛豫效应"：电池大电流充放电后，电压不会瞬间恢复稳定，
 *         需要等待一段时间（弛豫时间）才能认为真正进入静置状态，这对SOC/SOH估算极其重要。
 */
static void BMS_UpdateBatterySystemState(DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);

    /* 仅当电流采样值有效时才更新状态，避免因传感器故障导致误判 */
    if (pPackValues->invalidPackCurrent == 0u) {
        
        if (BS_POSITIVE_DISCHARGE_CURRENT == true) {
            /* 硬件配置A：正电流代表放电，负电流代表充电 */
            if (pPackValues->packCurrent_mA >= BS_REST_CURRENT_mA) { 
                bms_state.currentFlowState = BMS_DISCHARGING; // 电流 >= 静置阈值：放电中
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms; // 重载（重新装填）弛豫计时器
            } else if (pPackValues->packCurrent_mA <= -BS_REST_CURRENT_mA) {
                bms_state.currentFlowState = BMS_CHARGING;   // 电流 <= -静置阈值：充电中
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms; // 重载弛豫计时器
            } else {
                /* 电流绝对值低于静置阈值：说明既没充电也没放电 */
                if (bms_state.restTimer_10ms == 0u) {
                    /* 弛豫倒计时已归零，电芯电压已稳定，真正进入静置状态 */
                    bms_state.currentFlowState = BMS_AT_REST;
                } else {
                    /* 弛豫倒计时未归零，电芯电压正在恢复中，处于弛豫状态 */
                    bms_state.restTimer_10ms--; // 倒计时递减
                    bms_state.currentFlowState = BMS_RELAXATION;
                }
            }
        } else {
            /* 硬件配置B：负电流代表放电，正电流代表充电 (电流传感器安装方向相反) */
            if (pPackValues->packCurrent_mA <= -BS_REST_CURRENT_mA) {
                bms_state.currentFlowState = BMS_DISCHARGING;
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms;
            } else if (pPackValues->packCurrent_mA >= BS_REST_CURRENT_mA) {
                bms_state.currentFlowState = BMS_CHARGING;
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms;
            } else {
                if (bms_state.restTimer_10ms == 0u) {
                    bms_state.currentFlowState = BMS_AT_REST;
                } else {
                    bms_state.restTimer_10ms--;
                    bms_state.currentFlowState = BMS_RELAXATION;
                }
            }
        }
    }
}


/**
 * @brief  根据当前电流大小和方向，更新电池系统的电流流状态（放电、充电、弛豫、静置）
 * @param  pPackValues: 实时打包数据指针
 * @note   这个函数实现了电化学中的"弛豫效应"：电池大电流充放电后，电压不会瞬间恢复稳定，
 *         需要等待一段时间（弛豫时间）才能认为真正进入静置状态，这对SOC/SOH估算极其重要。
 */
static void BMS_UpdateBatterySystemState(DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    FAS_ASSERT(pPackValues != NULL_PTR);

    /* 仅当电流采样值有效时才更新状态，避免因传感器故障导致误判 */
    if (pPackValues->invalidPackCurrent == 0u) {
        
        if (BS_POSITIVE_DISCHARGE_CURRENT == true) {
            /* 硬件配置A：正电流代表放电，负电流代表充电 */
            if (pPackValues->packCurrent_mA >= BS_REST_CURRENT_mA) { 
                bms_state.currentFlowState = BMS_DISCHARGING; // 电流 >= 静置阈值：放电中
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms; // 重载（重新装填）弛豫计时器
            } else if (pPackValues->packCurrent_mA <= -BS_REST_CURRENT_mA) {
                bms_state.currentFlowState = BMS_CHARGING;   // 电流 <= -静置阈值：充电中
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms; // 重载弛豫计时器
            } else {
                /* 电流绝对值低于静置阈值：说明既没充电也没放电 */
                if (bms_state.restTimer_10ms == 0u) {
                    /* 弛豫倒计时已归零，电芯电压已稳定，真正进入静置状态 */
                    bms_state.currentFlowState = BMS_AT_REST;
                } else {
                    /* 弛豫倒计时未归零，电芯电压正在恢复中，处于弛豫状态 */
                    bms_state.restTimer_10ms--; // 倒计时递减
                    bms_state.currentFlowState = BMS_RELAXATION;
                }
            }
        } else {
            /* 硬件配置B：负电流代表放电，正电流代表充电 (电流传感器安装方向相反) */
            if (pPackValues->packCurrent_mA <= -BS_REST_CURRENT_mA) {
                bms_state.currentFlowState = BMS_DISCHARGING;
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms;
            } else if (pPackValues->packCurrent_mA >= BS_REST_CURRENT_mA) {
                bms_state.currentFlowState = BMS_CHARGING;
                bms_state.restTimer_10ms   = BS_RELAXATION_PERIOD_10ms;
            } else {
                if (bms_state.restTimer_10ms == 0u) {
                    bms_state.currentFlowState = BMS_AT_REST;
                } else {
                    bms_state.restTimer_10ms--;
                    bms_state.currentFlowState = BMS_RELAXATION;
                }
            }
        }
    }
}


/**
 * @brief  当第一个接触器断开后，获取该簇需要断开的第二个接触器
 * @param  stringNumber: 簇编号
 * @param  firstOpenedContactorType: 第一个断开的接触器类型
 * @retval 第二个需要断开的接触器类型
 * @note   断开第一个接触器后，电流被切断，此时断开第二个接触器是不带载的（无电弧风险），
 *         目的是形成物理隔离，确保高压彻底断开。
 */
static CONT_TYPE_e BMS_GetSecondContactorToBeOpened(uint8_t stringNumber, CONT_TYPE_e firstOpenedContactorType) {
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    /* 第一个断开的不能是未定义，也不能是预充（预充不能作为第一断开点） */
    FAS_ASSERT((firstOpenedContactorType != CONT_UNDEFINED) && (firstOpenedContactorType != CONT_PRECHARGE));
    
    CONT_TYPE_e contactorToBeOpened = CONT_UNDEFINED;

    /* 逻辑很简单：如果先断开的是主正，那后断开的就是主负；反之亦然 */
    if (firstOpenedContactorType == CONT_PLUS) {
        contactorToBeOpened = CONT_MINUS;
    } else {
        contactorToBeOpened = CONT_PLUS;
    }

    /* 在数据库中验证这个接触器是否真实存在 */
    uint8_t contactor = 0u;
    for (; contactor < BS_NR_OF_CONTACTORS; contactor++) {
        if ((stringNumber == cont_contactorStates[contactor].stringIndex) &&
            (contactorToBeOpened == cont_contactorStates[contactor].type)) {
            contactorToBeOpened = cont_contactorStates[contactor].type;
            break;
        }
    }

    /* 如果找不到对应的接触器，说明该硬件拓扑只有一个接触器（极简设计），触发断言 */
    if (contactor == BS_NR_OF_CONTACTORS) {
        FAS_ASSERT(FAS_TRAP);
    }

    return contactorToBeOpened;
}


/*========== 外部函数实现 ================================*/

/* 获取BMS初始化是否完成的状态 */
extern STD_RETURN_TYPE_e BMS_GetInitializationState(void) {
    return bms_state.initFinished;
}

/* 获取BMS当前主状态 */
extern BMS_FSM_STATES_e BMS_GetState(void) {
    return bms_state.state;
}

/* 获取BMS当前子状态 */
extern BMS_FSM_SUB_e BMS_GetSubstate(void) {
    return bms_state.substate;
}

/**
 * @brief  外部请求设置BMS状态的接口（如VCU发来指令要求进入待机）
 * @param  statereq: 请求的状态枚举
 * @retval BMS_OK: 请求合法并已接受; 其他: 请求非法被拒绝
 */
BMS_RETURN_TYPE_e BMS_SetStateRequest(BMS_STATE_REQUEST_e statereq) {
    BMS_RETURN_TYPE_e retVal = BMS_OK;

    /* 临界区保护：防止多个任务同时发送请求造成冲突 */
    OS_EnterTaskCritical();
    
    /* 检查请求是否合法（比如当前处于故障状态时，不允许请求进入充电状态） */
    retVal = BMS_CheckStateRequest(statereq);

    /* 如果检查通过，将请求写入状态机变量 */
    if (retVal == BMS_OK) {
        bms_state.stateRequest = statereq; // 这里写入的变量，正是之前 BMS_TransferStateRequest 读取并清空的变量
    }
    
    OS_ExitTaskCritical();

    return retVal;
}


/**
 * @brief BMS状态机主触发函数，通常由RTOS以固定周期（如10ms）调用
 */
void BMS_Trigger(void) {
    /* ===== 局部变量声明与初始化 ===== */
    BMS_STATE_REQUEST_e statereq                   = BMS_STATE_NO_REQUEST; // 状态请求暂存
    DATA_BLOCK_SYSTEM_STATE_s systemState          = {.header.uniqueId = DATA_BLOCK_ID_SYSTEM_STATE}; // 系统状态数据库镜像
    bms_state.currentSystick                       = OS_GetTickCount(); // 获取当前系统时间戳(用于超时计算)
    static uint32_t nextOpenWireCheck              = 0;   // 下次开路(断线)检查的时间点
    static uint8_t stringNumber                    = 0u;  // 当前正在操作的电池簇编号
    static uint8_t nextStringNumber                = 0u;  // 下一个待操作的电池簇编号
    CONT_ELECTRICAL_STATE_TYPE_e contactorState    = CONT_SWITCH_UNDEFINED; // 接触器当前物理状态
    BMS_RESULT_PRECHARGE_PROCESS_e prechargeRetval = BMS_PRECHARING_HAS_NOT_STARTED; // 预充过程结果
    bool contactorFeedbackValid                    = false; // 接触器反馈(辅助触点)是否有效
    STD_RETURN_TYPE_e contRetVal                   = STD_NOT_OK; // 接触器控制函数返回值

    /* ===== 周期性后台任务（不受状态机锁限制） ===== */
    // 只要系统不是刚上电的未初始化状态，就必须持续读取数据、更新状态、检查安全限值
    if (bms_state.state != BMS_FSM_STATE_UNINITIALIZED) {
        BMS_GetMeasurementValues();      // 1. 获取最新电压电流温度数据
        BMS_UpdateBatterySystemState(&bms_tablePackValues); // 2. 更新充放电/静置/弛豫状态
        SOA_CheckVoltages(&bms_tableMinMax);    // 3. 安全运行区检查：电压越限
        SOA_CheckTemperatures(&bms_tableMinMax, &bms_tablePackValues); // 4. 安全运行区检查：温度越限
        SOA_CheckCurrent(&bms_tablePackValues); // 5. 安全运行区检查：电流越限
        SOA_CheckSlaveTemperatures();     // 6. 检查从控板温度
        BMS_CheckOpenSenseWire();         // 7. 检查采样线是否断开
        CONT_CheckFeedback();             // 8. 检查所有接触器辅助触点反馈是否异常
    }

    /* ===== 防重入检查（核心安全机制） ===== */
    // 如果triggerentry > 0，说明上一次状态机循环还没跑完（被中断打断等），直接退出，防止数据被篡改
    if (BMS_CheckReEntrance() > 0u) {
        return;
    }

    /* ===== 通用定时器倒计时 ===== */
    // 这些定时器是状态机跳转的时间基础，每次Trigger周期递减
    if (bms_state.nextStringClosedTimer > 0u) {
        bms_state.nextStringClosedTimer--; // 闭合下一个簇的间隔计时器
    }
    if (bms_state.stringOpenTimeout > 0u) {
        bms_state.stringOpenTimeout--; // 断开簇的超时计时器
    }
    if (bms_state.stringCloseTimeout > 0u) {
        bms_state.stringCloseTimeout--; // 闭合簇的超时计时器
    }
    if (bms_state.OscillationTimeout > 0u) {
        bms_state.OscillationTimeout--; // 状态振荡(频繁跳转)保护计时器
    }

    /* ===== 状态机通用定时器处理 ===== */
    if (bms_state.timer > 0u) {
        if ((--bms_state.timer) > 0u) {
            // 如果timer倒计时还没归零，释放重入锁并退出当前周期。
            // 这意味着：状态机在等待时间到达，期间不执行switch里的逻辑。
            bms_state.triggerentry--;
            return; 
        }
        // 如果timer归零，代码继续往下走，执行状态机逻辑
    }

    /**** 核心状态机 Switch 分支 ****/
    switch (bms_state.state) {

        /**************************** 1. 未初始化状态 ****************************/
        case BMS_FSM_STATE_UNINITIALIZED:
            /* 等待外部(如VCU)发来初始化请求 */
            statereq = BMS_TransferStateRequest(); // 原子读取并清除请求
            if (statereq == BMS_STATE_INITIALIZATION_REQUEST) {
                BMS_SAVE_LAST_STATES(); // 保存上一状态用于追溯
                bms_state.timer    = BMS_FSM_SHORTTIME; // 设定短延时进入下一步
                bms_state.state    = BMS_FSM_STATE_INITIALIZATION; // 跳转到初始化中
                bms_state.substate = BMS_FSM_SUBSTATE_ENTRY; // 子状态设为入口
            } else if (statereq == BMS_STATE_NO_REQUEST) {
                /* 无请求，原地等待 */
            } else {
                bms_state.ErrRequestCounter++; /* 收到非法请求(如在未初始化时要求充电)，错误计数+1 */
            }
            break;

        /**************************** 2. 初始化中状态 ****************************/
        case BMS_FSM_STATE_INITIALIZATION:
            BMS_SAVE_LAST_STATES();
            DIAG_Handler(DIAG_ID_ALERT_MODE, DIAG_EVENT_OK, DIAG_SYSTEM, 0u); // 重置ALERT模式标志
            bms_state.initFinished = STD_OK; // 标记初始化完成
            bms_state.timer        = BMS_FSM_LONGTIME; // 长延时等待
            bms_state.state        = BMS_FSM_STATE_INITIALIZED; // 转移到已初始化状态
            bms_state.substate     = BMS_FSM_SUBSTATE_ENTRY;
            break;

        /**************************** 3. 已初始化状态 ****************************/
        case BMS_FSM_STATE_INITIALIZED:
            BMS_SAVE_LAST_STATES();
            // 检查绝缘监测设备(IMD)是否就绪
            if (IMD_RequestInsulationMeasurement() == IMD_ILLEGAL_REQUEST) {
                bms_state.timer = BMS_FSM_LONGTIME; // IMD没好，继续等
            } else {
                bms_state.timer    = BMS_FSM_SHORTTIME; // IMD就绪，准备进入空闲状态
                bms_state.state    = BMS_FSM_STATE_IDLE;
                bms_state.substate = BMS_FSM_SUBSTATE_ENTRY;
            }
            break;

        /**************************** 4. 空闲状态 ****************************/
        case BMS_FSM_STATE_IDLE:
            BMS_SAVE_LAST_STATES();

            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                DATA_READ_DATA(&systemState);
                systemState.bmsCanState = BMS_CAN_STATE_IDLE; // 通过CAN向整车广播当前为IDLE状态
                DATA_WRITE_DATA(&systemState);
                bms_state.timer    = BMS_FSM_SHORTTIME;
                bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS; // 下一步：查故障
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS) {
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    // 有致命故障！跳转到断开接触器状态，并标记断开后进入ERROR状态
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS; // 无故障，查请求
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS) {
                if (BMS_CheckCanRequests() == BMS_REQ_ID_STANDBY) {
                    // 收到VCU的Standby请求，跳转断开接触器，随后进入STANDBY
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_STANDBY;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    // 没有新请求，循环检查故障
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                }
                break;
            }
            break;

        /**************************** 5. 断开接触器通用状态 (高压下电核心) ****************************/
        case BMS_FSM_STATE_OPEN_CONTACTORS:
            BMS_SAVE_LAST_STATES();

            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                BAL_SetStateRequest(BAL_STATE_NO_BALANCING_REQUEST); // 下电必须停止均衡
                // 检查是否是因为30C控制电源丢失导致的下电(极其危险，需特殊处理)
                if (DIAG_GetDiagnosisEntryState(DIAG_ID_SUPPLY_VOLTAGE_CLAMP_30C_LOST) == STD_NOT_OK) {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_HANDLE_SUPPLY_VOLTAGE_30C_LOSS;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_OPEN_ALL_PRECHARGE_CONTACTORS;
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_OPEN_ALL_PRECHARGE_CONTACTORS) {
                // 预充接触器带载能力弱(有电阻限流)，可以直接断开
                CONT_OpenAllPrechargeContactors();
                // 从最高索引的簇开始断开(倒序断开)
                stringNumber       = BS_NR_OF_STRINGS - 1u;
                bms_state.substate = BMS_FSM_SUBSTATE_OPEN_FIRST_STRING_CONTACTOR;
                bms_state.timer    = BMS_TIME_WAIT_AFTER_OPENING_PRECHARGE; // 等待预充断开生效

            } else if (bms_state.substate == BMS_FSM_SUBSTATE_OPEN_FIRST_STRING_CONTACTOR) {
                // 检查当前簇电流是否低于接触器最大分断电流(防止拉弧烧毁)
                if ((bms_tablePackValues.invalidStringCurrent[stringNumber] == 0u) &&
                    (MATH_AbsInt32_t(bms_tablePackValues.stringCurrent_mA[stringNumber]) <
                     BS_MAIN_CONTACTORS_MAXIMUM_BREAK_CURRENT_mA)) {
                    
                    // 【安全策略】根据电流方向选择优先断开的接触器(利用磁吹灭弧)
                    const BMS_CURRENT_FLOW_STATE_e flowDirection =
                        BMS_GetCurrentFlowDirection(bms_tablePackValues.stringCurrent_mA[stringNumber]);
                    bms_state.contactorToBeOpened = BMS_GetFirstContactorToBeOpened(stringNumber, flowDirection);
                    bms_state.stringToBeOpened    = stringNumber;
                    
                    CONT_OpenContactor(stringNumber, bms_state.contactorToBeOpened); // 断开第一个主接触器
                    bms_state.timer             = BMS_WAIT_TIME_AFTER_OPENING_STRING_CONTACTOR;
                    bms_state.substate          = BMS_FSM_SUBSTATE_OPEN_SECOND_STRING_CONTACTOR;
                    bms_state.stringOpenTimeout = BMS_STRING_OPEN_TIMEOUT; // 启动断开超时监控
                } else {
                    // 电流过大，接触器无法断开！累计超大电流持续时间
                    bms_state.timeAboveContactorBreakCurrent_ms += BMS_STATEMACHINE_TASK_CYCLE_CONTEXT_MS;
                    // 如果持续时间超过了主熔断器熔断时间，说明熔断器也没断，强制拉弧断开(ALERT模式)
                    if (bms_state.timeAboveContactorBreakCurrent_ms > BS_MAIN_FUSE_MAXIMUM_TRIGGER_DURATION_ms) {
                        DIAG_Handler(DIAG_ID_ALERT_MODE, DIAG_EVENT_NOT_OK, DIAG_SYSTEM, 0u); // 激活ALERT
                        const BMS_CURRENT_FLOW_STATE_e flowDirection =
                            BMS_GetCurrentFlowDirection(bms_tablePackValues.stringCurrent_mA[stringNumber]);
                        bms_state.contactorToBeOpened = BMS_GetFirstContactorToBeOpened(stringNumber, flowDirection);
                        bms_state.stringToBeOpened    = stringNumber;
                        CONT_OpenContactor(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                        bms_state.timer    = BMS_WAIT_TIME_AFTER_OPENING_STRING_CONTACTOR;
                        bms_state.substate = BMS_FSM_SUBSTATE_OPEN_SECOND_STRING_CONTACTOR;
                    }
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_OPEN_SECOND_STRING_CONTACTOR) {
                // 检查第一个接触器是否真的断开了
                contactorState = CONT_GetContactorState(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                contactorFeedbackValid =
                    BMS_IsContactorFeedbackValid(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                
                // 如果反馈已断开，或者反馈传感器坏了(无法确认，只能假设断开)，则断开第二个
                if ((contactorState == CONT_SWITCH_OFF) || (contactorFeedbackValid == false)) {
                    bms_state.contactorToBeOpened =
                        BMS_GetSecondContactorToBeOpened(stringNumber, bms_state.contactorToBeOpened);
                    CONT_OpenContactor(bms_state.stringToBeOpened, bms_state.contactorToBeOpened); // 断开第二个接触器
                    bms_state.timer    = BMS_WAIT_TIME_AFTER_OPENING_STRING_CONTACTOR;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_SECOND_STRING_CONTACTOR;
                } else {
                    // 没断开，重发断开指令
                    CONT_OpenContactor(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                    bms_state.timer = BMS_FSM_SHORTTIME;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_SECOND_STRING_CONTACTOR) {
                // 检查第二个接触器是否断开
                contactorState = CONT_GetContactorState(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                contactorFeedbackValid =
                    BMS_IsContactorFeedbackValid(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                    
                if ((contactorState == CONT_SWITCH_OFF) || (contactorFeedbackValid == false)) {
                    // 当前簇的两个主接触器都断开了，清理状态
                    bms_state.contactorToBeOpened = CONT_UNDEFINED;
                    bms_state.stringToBeOpened    = 0u;
                    if (bms_state.numberOfClosedStrings > 0u) {
                        bms_state.numberOfClosedStrings--; // 已闭合簇计数减1
                    }
                    bms_state.closedStrings[stringNumber] = false;
                    
                    if (stringNumber > 0u) {
                        stringNumber--; // 簇索引减1，准备断开下一个簇
                        bms_state.substate = BMS_FSM_SUBSTATE_OPEN_FIRST_STRING_CONTACTOR;
                        bms_state.timer    = BMS_FSM_SHORTTIME;
                        break;
                    } else {
                        // 所有簇都断开了，准备退出OPEN_CONTACTORS状态
                        bms_state.substate = BMS_FSM_SUBSTATE_OPEN_STRINGS_EXIT;
                        bms_state.timer    = BMS_FSM_SHORTTIME;
                    }
                    break;
                } else if (bms_state.stringOpenTimeout == 0u) {
                    // 断开超时，放弃当前簇，强制去断下一个簇
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_OPEN_FIRST_STRING_CONTACTOR;
                    break;
                } else {
                    // 重发断开指令
                    CONT_OpenContactor(bms_state.stringToBeOpened, bms_state.contactorToBeOpened);
                    bms_state.timer = BMS_FSM_SHORTTIME;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_HANDLE_SUPPLY_VOLTAGE_30C_LOSS) {
                // 30C电源丢失：控制板即将断电，必须暴力断开所有接触器并关闭所有IO
                CONT_OpenAllContactors();
                SPS_SwitchOffAllGeneralIoChannels();
                bms_state.substate = BMS_FSM_SUBSTATE_OPEN_STRINGS_EXIT;
                bms_state.timer    = BMS_FSM_SHORTTIME;
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_OPEN_STRINGS_EXIT) {
                // 根据之前记录的nextState，决定下电后是去STANDBY还是ERROR
                if (bms_state.nextState == BMS_FSM_STATE_STANDBY) {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.state    = BMS_FSM_STATE_STANDBY;
                    bms_state.substate = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.state    = BMS_FSM_STATE_ERROR;
                    bms_state.substate = BMS_FSM_SUBSTATE_ENTRY;
                }
            } else {
                FAS_ASSERT(FAS_TRAP); // 非法子状态，死机保护
            }
            break;

        /**************************** 6. 待机状态 ****************************/
        case BMS_FSM_STATE_STANDBY:
            BMS_SAVE_LAST_STATES();
            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                BAL_SetStateRequest(BAL_STATE_ALLOW_BALANCING_REQUEST); // 待机时允许均衡
                bms_state.timer    = BMS_FSM_MEDIUM_TIME;
                bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_INTERLOCK;
                DATA_READ_DATA(&systemState);
                systemState.bmsCanState = BMS_CAN_STATE_STANDBY;
                DATA_WRITE_DATA(&systemState);
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_INTERLOCK) {
                // 检查互锁状态和故障
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
                // 检查CAN请求，决定是进入放电还是充电
                if (BMS_CheckCanRequests() == BMS_REQ_ID_NORMAL) {
                    bms_state.powerPath = BMS_POWER_PATH_0; // 放电路径
                    bms_state.nextState = BMS_FSM_STATE_DISCHARGE;
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_PRECHARGE; // 必须先预充！
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                }
                if (BMS_CheckCanRequests() == BMS_REQ_ID_CHARGE) {
                    bms_state.powerPath = BMS_POWER_PATH_1; // 充电路径
                    bms_state.nextState = BMS_FSM_STATE_CHARGE;
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_PRECHARGE; // 充电也要先预充！
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    // 没有高压请求，周期性检查采样线断线
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                    break;
                }
            } else {
                FAS_ASSERT(FAS_TRAP);
            }
            break;

        /**************************** 7. 预充状态 (高压上电核心) ****************************/
        case BMS_FSM_STATE_PRECHARGE:
            BMS_SAVE_LAST_STATES();

            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                DATA_READ_DATA(&systemState);
                systemState.bmsCanState = BMS_CAN_STATE_PRECHARGE;
                DATA_WRITE_DATA(&systemState);
                
                // 【多簇策略】充电选最低电压簇先闭合，放电选最高电压簇先闭合
                if (bms_state.nextState == BMS_FSM_STATE_CHARGE) {
                    stringNumber = BMS_GetLowestString(BMS_TAKE_PRECHARGE_INTO_ACCOUNT, &bms_tablePackValues);
                } else {
                    stringNumber = BMS_GetHighestString(BMS_TAKE_PRECHARGE_INTO_ACCOUNT, &bms_tablePackValues);
                }
                
                if (stringNumber == BMS_NO_STRING_AVAILABLE) {
                    // 找不到可用簇，报错下电
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                }
                
                bms_state.prechargeTryCounter = 0u; // 预充重试计数清零
                bms_state.firstClosedString   = stringNumber; // 记录第一个闭合的簇
                
                if (bms_state.OscillationTimeout == 0u) {
                    // 先闭合主负接触器(无电弧风险，因为回路未通)
                    if (CONT_CloseContactor(bms_state.firstClosedString, CONT_MINUS) == STD_OK) {
                        bms_state.stringCloseTimeout = BMS_STRING_CLOSE_TIMEOUT;
                        bms_state.timer              = BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR;
                        bms_state.substate           = BMS_FSM_SUBSTATE_PRECHARGE_CLOSE_PRECHARGE;
                    } else {
                        bms_state.timer     = BMS_FSM_SHORTTIME;
                        bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                        bms_state.nextState = BMS_FSM_STATE_ERROR;
                        bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    }
                } else if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_PRECHARGE_CLOSE_PRECHARGE) {
                // 检查主负是否闭合成功
                contactorState = CONT_GetContactorState(bms_state.firstClosedString, CONT_MINUS);
                if (contactorState == CONT_SWITCH_ON) {
                    bms_state.OscillationTimeout = BMS_OSCILLATION_TIMEOUT;
                    contRetVal                   = CONT_ClosePrecharge(bms_state.firstClosedString); // 闭合预充接触器，回路接通，预充开始！
                    bms_state.closedPrechargeContactors[stringNumber] = true;
                    if (contRetVal == STD_OK) {
                        bms_state.startOfPrecharging = bms_state.currentSystick; // 记录预充开始时间戳
                        bms_state.timer              = BMS_FSM_SHORTTIME;
                        bms_state.substate           = BMS_FSM_SUBSTATE_PRECHARGE_CHECK_PRECHARGE_PROCESS;
                    } else {
                        bms_state.timer     = BMS_FSM_SHORTTIME;
                        bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                        bms_state.nextState = BMS_FSM_STATE_ERROR;
                        bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    }
                } else if (bms_state.stringCloseTimeout == 0u) {
                    bms_state.timer     = BMS_FSM_SHORTTIME;
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                } else {
                    CONT_CloseContactor(bms_state.firstClosedString, CONT_MINUS);
                    bms_state.timer = BMS_FSM_SHORTTIME;
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_PRECHARGE_CHECK_PRECHARGE_PROCESS) {
                // 预充过程监控：随时检查故障和中断请求
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                }
                if (BMS_CheckCanRequests() == BMS_REQ_ID_STANDBY) {
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_STANDBY;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                }

                contactorState = CONT_GetContactorState(bms_state.firstClosedString, CONT_PRECHARGE);
                // 调用监控函数：检查压差和电流是否达标，或是否超时
                prechargeRetval = BMS_MonitorPrechargeProcess(
                    bms_state.firstClosedString,
                    &bms_tablePackValues,
                    BMS_PRECHARGE_MONITORING_PARAMETERS,
                    BMS_MAXIMUM_PRECHARGE_DURATION_ms);
                    
                if ((contactorState == CONT_SWITCH_ON) && (prechargeRetval == BMS_PRECHARGING_FINISHED)) {
                    // 预充成功！立刻闭合主正接触器(此时压差极小，不会拉弧)
                    CONT_CloseContactor(bms_state.firstClosedString, CONT_PLUS);
                    bms_state.stringCloseTimeout = BMS_STRING_CLOSE_TIMEOUT;
                    bms_state.timer              = BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR;
                    bms_state.substate           = BMS_FSM_SUBSTATE_CHECK_CLOSE_SECOND_STRING_CONTACTOR_PRECHARGE_STATE;
                    break;
                } else if (prechargeRetval == BMS_PRECHARGING_ONGOING) {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_PRECHARGE_CHECK_PRECHARGE_PROCESS; // 继续监控
                    break;
                } else {
                    // 预充失败(超时)。断开预充接触器，尝试重试
                    contRetVal = CONT_OpenPrecharge(bms_state.firstClosedString);
                    if (bms_state.prechargeTryCounter < (BMS_PRECHARGE_TRIES - 1u)) {
                        bms_state.closedPrechargeContactors[stringNumber] = false;
                        if (contRetVal == STD_OK) {
                            bms_state.timer    = BMS_TIME_WAIT_AFTER_PRECHARGE_FAIL;
                            bms_state.substate = BMS_FSM_SUBSTATE_PRECHARGE_CLOSE_PRECHARGE; // 重试
                            bms_state.prechargeTryCounter++;
                        } else {
                            bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                            bms_state.nextState = BMS_FSM_STATE_ERROR;
                            bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                        }
                        break;
                    } else {
                        // 达到最大重试次数，彻底失败，报错下电
                        bms_state.closedPrechargeContactors[stringNumber] = false;
                        bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                        bms_state.nextState = BMS_FSM_STATE_ERROR;
                        bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                        break;
                    }
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_CLOSE_SECOND_STRING_CONTACTOR_PRECHARGE_STATE) {
                // 检查预充后主正接触器是否闭合
                contactorState = CONT_GetContactorState(bms_state.firstClosedString, CONT_PLUS);
                if (contactorState == CONT_SWITCH_ON) {
                    bms_state.closedStrings[bms_state.firstClosedString] = true;
                    bms_state.numberOfClosedStrings++;
                    bms_state.timer    = BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_CLOSING_STRINGS;
                    break;
                } else if (bms_state.stringCloseTimeout == 0u) {
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    CONT_CloseContactor(bms_state.firstClosedString, CONT_PLUS); // 重发闭合
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_FIRST_STRING;
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_FIRST_STRING) {
                // 闭合主正过程中的故障检查
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
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
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    // 第一个簇预充完全成功，接下来断开预充接触器(主正主负已闭合，预充不再需要)
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
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_PRECHARGE_CHECK_OPEN_PRECHARGE) {
                contactorState = CONT_GetContactorState(bms_state.firstClosedString, CONT_PRECHARGE);
                if (contactorState == CONT_SWITCH_OFF) {
                    // 预充接触器成功断开，第一个簇上电完成！进入NORMAL状态(后续在NORMAL里并联其他簇)
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.state    = BMS_FSM_STATE_NORMAL;
                    bms_state.substate = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else if (bms_state.stringCloseTimeout == 0u) {
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    CONT_OpenPrecharge(bms_state.firstClosedString); // 重发断开
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS_PRECHARGE_FIRST_STRING;
                    break;
                }
            } else {
                FAS_ASSERT(FAS_TRAP);
            }
            break;

        /**************************** 8. 正常工作状态 (多簇并联与循环检测) ****************************/
        case BMS_FSM_STATE_NORMAL:
            BMS_SAVE_LAST_STATES();

            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                DATA_READ_DATA(&systemState);
                if (bms_state.nextState == BMS_FSM_STATE_CHARGE) {
                    systemState.bmsCanState = BMS_CAN_STATE_CHARGE;
                } else {
                    systemState.bmsCanState = BMS_CAN_STATE_NORMAL;
                }
                DATA_WRITE_DATA(&systemState);
                bms_state.timer                 = BMS_FSM_SHORTTIME;
                bms_state.substate              = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                bms_state.nextStringClosedTimer = 0u; // 闭合下一簇的冷却计时器清零
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS) {
                if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
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
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_STANDBY;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_NORMAL_CLOSE_NEXT_STRING; // 尝试并联下一个簇
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_NORMAL_CLOSE_NEXT_STRING) {
                if (bms_state.nextStringClosedTimer == 0u) {
                    // 寻找与当前母线压差最小的未闭合簇(无需考虑预充支路，因为母线已带电)
                    nextStringNumber =
                        BMS_GetClosestString(BMS_DO_NOT_TAKE_PRECHARGE_INTO_ACCOUNT, &bms_tablePackValues);
                    if (nextStringNumber == BMS_NO_STRING_AVAILABLE) {
                        bms_state.timer    = BMS_FSM_SHORTTIME;
                        bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS; // 没簇可并，去查故障
                        break;
                    } else if (
                        (BMS_GetStringVoltageDifference(nextStringNumber, &bms_tablePackValues) <=
                         BMS_NEXT_STRING_VOLTAGE_LIMIT_MV) &&
                        (BMS_GetAverageStringCurrent(&bms_tablePackValues) <= BMS_AVERAGE_STRING_CURRENT_LIMIT_MA)) {
                        // 压差和电流都在安全范围内，直接闭合主负(无需预充，直接并网)
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
                    CONT_CloseContactor(nextStringNumber, CONT_PLUS); // 主负闭合，接着闭合主正
                    bms_state.timer    = BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR;
                    bms_state.substate = BMS_FSM_SUBSTATE_NORMAL_CLOSE_SECOND_STRING_CONTACTOR;
                } else if (bms_state.stringCloseTimeout == 0u) {
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    CONT_CloseContactor(nextStringNumber, CONT_MINUS); // 重发
                    bms_state.timer = BMS_FSM_SHORTTIME;
                }
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_STRING_CLOSED) {
                contactorState = CONT_GetContactorState(nextStringNumber, CONT_PLUS);
                if (contactorState == CONT_SWITCH_ON) {
                    bms_state.numberOfClosedStrings++;
                    bms_state.closedStrings[nextStringNumber] = true;
                    bms_state.nextStringClosedTimer           = BMS_WAIT_TIME_BETWEEN_CLOSING_STRINGS; // 簇间闭合冷却时间
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS; // 循环去检查故障和下一个簇
                    break;
                } else if (bms_state.stringCloseTimeout == 0u) {
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else if (BMS_IsBatterySystemStateOkay() == STD_NOT_OK) {
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_ERROR;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else if (BMS_CheckCanRequests() == BMS_REQ_ID_STANDBY) {
                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_STANDBY;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    CONT_CloseContactor(nextStringNumber, CONT_PLUS); // 重发
                    bms_state.timer = BMS_FSM_SHORTTIME;
                    break;
                }
            } else {
                FAS_ASSERT(FAS_TRAP);
            }
            break;

        /**************************** 9. 错误状态 ****************************/
        case BMS_FSM_STATE_ERROR:
            BMS_SAVE_LAST_STATES();

            if (bms_state.substate == BMS_FSM_SUBSTATE_ENTRY) {
                DATA_READ_DATA(&systemState);
                systemState.bmsCanState = BMS_CAN_STATE_ERROR;
                DATA_WRITE_DATA(&systemState);
                BAL_SetStateRequest(BAL_STATE_NO_BALANCING_REQUEST); // 故障停止均衡
                LED_SetToggleTime(LED_ERROR_OPERATION_ON_OFF_TIME_ms); // LED快闪报警
                nextOpenWireCheck = bms_state.currentSystick + AFE_ERROR_OPEN_WIRE_PERIOD_ms;
                bms_state.timer    = BMS_FSM_SHORTTIME;
                bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS;
                break;
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS) {
                if (DIAG_IsAnyFatalErrorSet() == true) {
                    // 故障仍在，原地停留并周期性检查断线
                    if (nextOpenWireCheck <= bms_state.currentSystick) {
                        nextOpenWireCheck = bms_state.currentSystick + AFE_ERROR_OPEN_WIRE_PERIOD_ms;
                    }
                } else {
                    // 故障已消失！清除致命错误相关变量
                    bms_state.minimumActiveDelay_ms  = BMS_NO_ACTIVE_DELAY_TIME_ms;
                    bms_state.transitionToErrorState = false;
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS; // 准备恢复
                    break;
                }
            } else if (bms_state.substate == BMS_FSM_SUBSTATE_CHECK_STATE_REQUESTS) {
                if (BMS_CheckCanRequests() == BMS_REQ_ID_STANDBY) {
                    // VCU允许恢复，进入待机状态(需先确认接触器全断开)
                    BAL_SetStateRequest(BAL_STATE_ALLOW_BALANCING_REQUEST);
                    LED_SetToggleTime(LED_NORMAL_OPERATION_ON_OFF_TIME_ms);

                    bms_state.state     = BMS_FSM_STATE_OPEN_CONTACTORS;
                    bms_state.nextState = BMS_FSM_STATE_STANDBY;
                    bms_state.substate  = BMS_FSM_SUBSTATE_ENTRY;
                    break;
                } else {
                    bms_state.timer    = BMS_FSM_SHORTTIME;
                    bms_state.substate = BMS_FSM_SUBSTATE_CHECK_ERROR_FLAGS; // 继续查故障
                    break;
                }
            } else {
                FAS_ASSERT(FAS_TRAP);
            }
            break;
            
        default:
            FAS_ASSERT(FAS_TRAP); // 非法主状态，死机保护
            break;
    } /* 结束 switch (bms_state.state) */

    /* ===== 状态变化异步通知 ===== */
    // 如果主状态或子状态发生了跳转，立刻通过CAN发送当前BMS状态给整车
    if ((bms_state.state != bms_state.lastState) || (bms_state.substate != bms_state.lastSubstate)) {
        CANTX_TransmitBmsState();
    }

    /* ===== 释放重入锁 ===== */
    bms_state.triggerentry--;
    bms_state.counter++; // 状态机运行周期计数
}


/**
 * @brief    获取电池系统当前的电流流动状态（充电、放电或静置）
 * @retval   BMS_CURRENT_FLOW_STATE_e 枚举类型，表示当前的电流流动状态
 */
extern BMS_CURRENT_FLOW_STATE_e BMS_GetBatterySystemState(void) {
    /* 直接从全局状态结构体 bms_state 中读取当前的电流流动状态 */
    return bms_state.currentFlowState;
}

/**
 * @brief    根据当前电流值判断电流流动方向（充电、放电或静置）
 * @param    current_mA: 当前电流值，单位为毫安。正负值代表不同方向。
 * @retval   BMS_CURRENT_FLOW_STATE_e 枚举类型，表示计算得出的电流流动状态
 * @note     AXIVION 常规 Generic-MissingParameterAssert: 
 *           current_mA 参数接受整个 int32_t 范围，因此无需进行参数断言校验
 */
extern BMS_CURRENT_FLOW_STATE_e BMS_GetCurrentFlowDirection(int32_t current_mA) {
    /* 默认将返回值初始化为放电状态 */
    BMS_CURRENT_FLOW_STATE_e retVal = BMS_DISCHARGING;

    /* 
     * 根据系统配置宏 BS_POSITIVE_DISCHARGE_CURRENT 判断正电流代表放电还是充电。
     * 这是为了兼容不同的硬件采样设计（有的硬件放电电流为正，有的为负）。
     */
    if (BS_POSITIVE_DISCHARGE_CURRENT == true) {
        /* 情况A：放电电流为正方向 */
        if (current_mA >= BS_REST_CURRENT_mA) {
            /* 电流大于等于静置阈值，判定为放电 */
            retVal = BMS_DISCHARGING;
        } else if (current_mA <= -BS_REST_CURRENT_mA) {
            /* 电流小于等于负静置阈值，判定为充电 */
            retVal = BMS_CHARGING;
        } else {
            /* 电流处于 (-BS_REST_CURRENT_mA, BS_REST_CURRENT_mA) 之间，判定为静置 */
            retVal = BMS_AT_REST;
        }
    } else {
        /* 情况B：放电电流为负方向（充电电流为正方向） */
        if (current_mA <= -BS_REST_CURRENT_mA) {
            /* 电流小于等于负静置阈值，判定为放电 */
            retVal = BMS_DISCHARGING;
        } else if (current_mA >= BS_REST_CURRENT_mA) {
            /* 电流大于等于静置阈值，判定为充电 */
            retVal = BMS_CHARGING;
        } else {
            /* 电流处于 (-BS_REST_CURRENT_mA, BS_REST_CURRENT_mA) 之间，判定为静置 */
            retVal = BMS_AT_REST;
        }
    }
    return retVal;
}

/**
 * @brief    检查指定的电池串是否已闭合（即主接触器已吸合，串已投入运行）
 * @param    stringNumber: 电池串的编号
 * @retval   true: 该串已闭合; false: 该串未闭合
 */
extern bool BMS_IsStringClosed(uint8_t stringNumber) {
    /* 
     * FAS_ASSERT 是安全断言，确保传入的串编号没有超过系统配置的最大串数，
     * 防止数组越界访问。如果越界，程序会在此处报错或停机。
     */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    
    bool retval = false;
    /* 检查该串对应的主接触器状态数组标志位 */
    if (bms_state.closedStrings[stringNumber] == true) {
        retval = true;
    }
    return retval;
}

/**
 * @brief    检查指定的电池串是否正在进行预充
 * @param    stringNumber: 电池串的编号
 * @retval   true: 该串正在预充; false: 该串未在预充
 */
extern bool BMS_IsStringPrecharging(uint8_t stringNumber) {
    /* 安全断言：防止串编号越界 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    
    bool retval = false;
    /* 检查该串对应的预充接触器状态数组标志位 */
    if (bms_state.closedPrechargeContactors[stringNumber] == true) {
        retval = true;
    }
    return retval;
}

/**
 * @brief    获取当前已闭合（已连接/投入运行）的电池串总数
 * @retval   已闭合的电池串数量
 */
extern uint8_t BMS_GetNumberOfConnectedStrings(void) {
    return bms_state.numberOfClosedStrings;
}

/**
 * @brief    检查系统是否正在向错误状态转换
 * @retval   true: 正在进入错误状态; false: 否
 */
extern bool BMS_IsTransitionToErrorStateActive(void) {
    return bms_state.transitionToErrorState;
}


/*========== 外部化的静态函数实现 (单元测试) =======*/
/* 预处理指令：只有在定义了 UNITY_UNIT_TEST 宏时，以下代码才会被编译器编译。
 * UNITY 是一种广泛用于嵌入式C语言的单元测试框架。
 * 这说明这段代码专门是为了配合单元测试而存在的，在正式发布的生产代码（Release版本）中不会被编译进去。*/
#ifdef UNITY_UNIT_TEST

/* 桥接函数：为静态(static)函数暴露对外接口
 * 原因：在C语言中，如果模块内部的函数被声明为 static，它就被限制在了当前的 .c 文件内，
 * 外部的单元测试文件（如 test_bms.c）是无法直接调用这些函数进行测试的。
 * 解决方案：提供一组带有 extern 前缀的包装函数，函数名统一加上 TEST_ 前缀，
 * 在包装函数内部去调用真正的 static 函数。这样，测试代码就可以通过调用这组
 * TEST_ 函数来间接测试那些原本私有的内部逻辑了。*/

/* 桥接：状态请求合法性检查函数 */
extern BMS_RETURN_TYPE_e TEST_BMS_CheckStateRequest(BMS_STATE_REQUEST_e statereq) {
    return BMS_CheckStateRequest(statereq);
}

/* 桥接：状态请求转移（读取并清零）函数 */
extern BMS_STATE_REQUEST_e TEST_BMS_TransferStateRequest(void) {
    return BMS_TransferStateRequest();
}

/* 桥接：防重入检查函数 */
extern uint8_t TEST_BMS_CheckReEntrance(void) {
    return BMS_CheckReEntrance();
}

/* 桥接：CAN网络请求检查函数 */
extern uint8_t TEST_BMS_CheckCanRequests(void) {
    return BMS_CheckCanRequests();
}

/* 桥接：致命错误标志检查函数 */
extern bool TEST_BMS_IsAnyFatalErrorFlagSet(void) {
    return BMS_IsAnyFatalErrorFlagSet();
}

/* 桥接：电池系统状态（含安全延时）检查函数 */
extern STD_RETURN_TYPE_e TEST_BMS_IsBatterySystemStateOkay(void) {
    return BMS_IsBatterySystemStateOkay();
}

/* 桥接：接触器反馈有效性检查函数 */
extern bool TEST_BMS_IsContactorFeedbackValid(uint8_t stringNumber, CONT_TYPE_e contactorType) {
    return BMS_IsContactorFeedbackValid(stringNumber, contactorType);
}

/* 桥接：获取底层测量数据函数 */
extern void TEST_BMS_GetMeasurementValues(void) {
    BMS_GetMeasurementValues();
}

/* 桥接：电压采样线断线检查函数 */
extern void TEST_BMS_CheckOpenSenseWire(void) {
    BMS_CheckOpenSenseWire();
}

/* 桥接：预充过程监控函数 */
extern STD_RETURN_TYPE_e TEST_BMS_MonitorPrechargeProcess(
    uint8_t stringNumber,
    const DATA_BLOCK_PACK_VALUES_s *pPackValues,
    BS_PRECHARGE_MONITORING_e monitoringParameters,
    uint32_t timeout_ms) {
    return BMS_MonitorPrechargeProcess(stringNumber, pPackValues, monitoringParameters, timeout_ms);
}

/* 桥接：获取最高电压簇函数 */
extern uint8_t TEST_BMS_GetHighestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    return BMS_GetHighestString(precharge, pPackValues);
}

/* 桥接：获取最接近电压簇函数 */
extern uint8_t TEST_BMS_GetClosestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    return BMS_GetClosestString(precharge, pPackValues);
}

/* 桥接：获取最低电压簇函数 */
extern uint8_t TEST_BMS_GetLowestString(BMS_CONSIDER_PRECHARGE_e precharge, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    return BMS_GetLowestString(precharge, pPackValues);
}

/* 桥接：计算簇间压差函数 */
extern int32_t TEST_BMS_GetStringVoltageDifference(uint8_t string, DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    return BMS_GetStringVoltageDifference(string, pPackValues);
}

/* 桥接：计算平均簇电流函数 */
extern int32_t TEST_BMS_GetAverageStringCurrent(DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    return BMS_GetAverageStringCurrent(pPackValues);
}

/* 桥接：更新电池系统电流状态（充放电/弛豫）函数 */
extern void TEST_BMS_UpdateBatterySystemState(DATA_BLOCK_PACK_VALUES_s *pPackValues) {
    BMS_UpdateBatterySystemState(pPackValues);
}

#endif /* 结束 #ifdef UNITY_UNIT_TEST */


