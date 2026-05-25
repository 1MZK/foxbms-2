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
 * @file    diag_cfg.h
 * @author  foxBMS 团队
 * @date    2019-11-28 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup ENGINE_CONFIGURATION
 * @prefix  DIAG
 *
 * @brief   诊断模块配置头文件
 * @details 在此头文件中，不同的诊断通道定义被分配给不同的诊断 ID。
 *          此外，诊断错误日志设置也在此处配置。
 */

#ifndef FOXBMS__DIAG_CFG_H_
#define FOXBMS__DIAG_CFG_H_

/*========== 包含文件 =======================================================*/

#include "battery_system_cfg.h"
#include "database_cfg.h"

#include <stdbool.h>
#include <stdint.h>

/*========== 宏和定义 =========================================================*/
/* 'sensitivity' 缩写为 'sen'，仅且仅限于这些定义，以保持诊断数组简短且易于理解。
 * 此处特例省略了不缩写的一般规则，以提高代码的可读性 */

#define DIAG_SEN_EVENT_1   (0u)   /*!< 在第 1 次事件时记录 */
#define DIAG_SEN_EVENT_3   (2u)   /*!< 在第 3 次事件时记录 */
#define DIAG_SEN_EVENT_5   (4u)   /*!< 在第 5 次事件时记录 */
#define DIAG_SEN_EVENT_10  (9u)   /*!< 在第 10 次事件时记录 */
#define DIAG_SEN_EVENT_20  (19u)  /*!< 在第 20 次事件时记录 */
#define DIAG_SEN_EVENT_50  (49u)  /*!< 在第 50 次事件时记录 */
#define DIAG_SEN_EVENT_100 (99u)  /*!< 在第 100 次事件时记录 */
#define DIAG_SEN_EVENT_500 (499u) /*!< 在第 500 次事件时记录 */

/** ---------------- 错误状态转换延迟定义 ----------------
 * 这些定义配置了在检测到故障后、转换为错误状态之前的延迟。
 * 在此期间，BMS 可以警告上级控制单元接触器即将断开。
 * 上级控制单元可以采取措施，例如，在转换为错误状态之前降低电流。
 *
 * 如果在配置数组 #diag_diagnosisIdConfiguration 中配置了 #DIAG_SEVERITY_LEVEL_e 
 * 类型的 #DIAG_FATAL_ERROR 严重级别，则不会考虑此延迟。
 * 对于任何其他严重级别，#DIAG_DELAY_DISCARD 可用作虚拟值。
 */
#define DIAG_DELAY_DISCARD (UINT32_MAX)
/** 检测到错误后无延迟，立即断开接触器 */
#define DIAG_NO_DELAY (0u)

/** 各种延迟时间的定义 */
#define DIAG_DELAY_100ms  (100u)
#define DIAG_DELAY_200ms  (200u)
#define DIAG_DELAY_1000ms (1000u)
#define DIAG_DELAY_2000ms (1000u)

/** 记录的同一错误的最大数量 */
#define DIAG_MAX_ENTRIES_OF_ERROR (5)

/** 用于存储和传递本地数据库表句柄的复合类型 */
typedef struct {
    DATA_BLOCK_ERROR_STATE_s *pTableError; /*!< 包含错误状态的数据库表 */
    DATA_BLOCK_MOL_FLAG_s *pTableMol;      /*!< 包含 MOL 标志的数据库表 */
    DATA_BLOCK_RSL_FLAG_s *pTableRsl;      /*!< 包含 RSL 标志的数据库表 */
    DATA_BLOCK_MSL_FLAG_s *pTableMsl;      /*!< 包含 MSL 标志的数据库表 */
} DIAG_DATABASE_SHIM_s;

/** 用于存储和传递本地数据库表句柄的变量 */
extern const DIAG_DATABASE_SHIM_s diag_kDatabaseShim;

/** 诊断 ID 列表 */
typedef enum {
    DIAG_ID_SYSTEM_MONITORING, /*!< 系统监控模块检测到偏离任务时间限制 */
    DIAG_ID_AFE_SPI,           /*!< AFE 的 SPI 通信出现问题 */
    DIAG_ID_AFE_COMMUNICATION_INTEGRITY, /*!< AFE 的通信完整性出错，例如 AFE 的 PEC 错误 */
    DIAG_ID_AFE_MUX,    /*!< 连接到 AFE 的多路复用器未按预期方式反应 */
    DIAG_ID_AFE_CONFIG, /*!< AFE 驱动程序识别到配置错误 */
    DIAG_ID_CAN_TIMING, /*!< BMS 根本未接收到 CAN 消息，或未在预期时间范围内接收到 */
    DIAG_ID_CAN_RX_QUEUE_FULL, /*!< 驱动程序的接收队列已满；无法接收新消息 */
    DIAG_ID_CAN_TX_QUEUE_FULL, /*!< 驱动程序的发送队列已满；所有新消息都将丢失 */
    DIAG_ID_CURRENT_SENSOR_CC_RESPONDING, /*!< CAN 总线上的电流计数器测量值缺失或不在预期时间约束内 */
    DIAG_ID_CURRENT_SENSOR_EC_RESPONDING, /*!< CAN 总线上的能量计数器测量值缺失或不在预期时间约束内 */
    DIAG_ID_CURRENT_SENSOR_RESPONDING, /*!< CAN 总线上的电流传感器测量值缺失或不在预期时间约束内 */
    DIAG_ID_PLAUSIBILITY_CELL_VOLTAGE, /*!< 电池电压的冗余测量返回了不合理的值 */
    DIAG_ID_AFE_CELL_VOLTAGE_MEAS_ERROR, /*!< AFE 驱动程序确定电池电压测量值不合理 */
    DIAG_ID_AFE_CELL_TEMPERATURE_MEAS_ERROR, /*!< AFE 驱动程序确定电池温度测量值不合理 */
    DIAG_ID_PLAUSIBILITY_CELL_TEMP, /*!< 电池温度的冗余测量返回了不合理的值 */
    DIAG_ID_PLAUSIBILITY_CELL_VOLTAGE_SPREAD,     /*!< 电池电压的分布（最小值和最大值之间的差异）高得不合理 */
    DIAG_ID_PLAUSIBILITY_CELL_TEMPERATURE_SPREAD, /*!< 电池温度的分布（最小值和最大值之间的差异）高得不合理 */
    DIAG_ID_CELL_VOLTAGE_OVERVOLTAGE_MSL,         /*!< 电池电压超限 */
    DIAG_ID_CELL_VOLTAGE_OVERVOLTAGE_RSL,         /*!< 电池电压超限 */
    DIAG_ID_CELL_VOLTAGE_OVERVOLTAGE_MOL,         /*!< 电池电压超限 */
    DIAG_ID_CELL_VOLTAGE_UNDERVOLTAGE_MSL,        /*!< 电池电压超限 */
    DIAG_ID_CELL_VOLTAGE_UNDERVOLTAGE_RSL,        /*!< 电池电压超限 */
    DIAG_ID_CELL_VOLTAGE_UNDERVOLTAGE_MOL,        /*!< 电池电压超限 */
    DIAG_ID_TEMP_OVERTEMPERATURE_CHARGE_MSL,      /*!< 温度超限 */
    DIAG_ID_TEMP_OVERTEMPERATURE_CHARGE_RSL,      /*!< 温度超限 */
    DIAG_ID_TEMP_OVERTEMPERATURE_CHARGE_MOL,      /*!< 温度超限 */
    DIAG_ID_TEMP_OVERTEMPERATURE_DISCHARGE_MSL,   /*!< 温度超限 */
    DIAG_ID_TEMP_OVERTEMPERATURE_DISCHARGE_RSL,   /*!< 温度超限 */
    DIAG_ID_TEMP_OVERTEMPERATURE_DISCHARGE_MOL,   /*!< 温度超限 */
    DIAG_ID_TEMP_UNDERTEMPERATURE_CHARGE_MSL,     /*!< 温度超限 */
    DIAG_ID_TEMP_UNDERTEMPERATURE_CHARGE_RSL,     /*!< 温度超限 */
    DIAG_ID_TEMP_UNDERTEMPERATURE_CHARGE_MOL,     /*!< 温度超限 */
    DIAG_ID_TEMP_UNDERTEMPERATURE_DISCHARGE_MSL,  /*!< 温度超限 */
    DIAG_ID_TEMP_UNDERTEMPERATURE_DISCHARGE_RSL,  /*!< 温度超限 */
    DIAG_ID_TEMP_UNDERTEMPERATURE_DISCHARGE_MOL,  /*!< 温度超限 */
    DIAG_ID_OVERCURRENT_CHARGE_CELL_MSL,          /*!< 电芯级别过流 */
    DIAG_ID_OVERCURRENT_CHARGE_CELL_RSL,          /*!< 电芯级别过流 */
    DIAG_ID_OVERCURRENT_CHARGE_CELL_MOL,          /*!< 电芯级别过流 */
    DIAG_ID_OVERCURRENT_DISCHARGE_CELL_MSL,       /*!< 电芯级别过流 */
    DIAG_ID_OVERCURRENT_DISCHARGE_CELL_RSL,       /*!< 电芯级别过流 */
    DIAG_ID_OVERCURRENT_DISCHARGE_CELL_MOL,       /*!< 电芯级别过流 */
    DIAG_ID_STRING_OVERCURRENT_CHARGE_MSL,        /*!< 串级别过流 */
    DIAG_ID_STRING_OVERCURRENT_CHARGE_RSL,        /*!< 串级别过流 */
    DIAG_ID_STRING_OVERCURRENT_CHARGE_MOL,        /*!< 串级别过流 */
    DIAG_ID_STRING_OVERCURRENT_DISCHARGE_MSL,     /*!< 串级别过流 */
    DIAG_ID_STRING_OVERCURRENT_DISCHARGE_RSL,     /*!< 串级别过流 */
    DIAG_ID_STRING_OVERCURRENT_DISCHARGE_MOL,     /*!< 串级别过流 */
    DIAG_ID_PACK_OVERCURRENT_CHARGE_MSL,          /*!< 串级别过流 */
    DIAG_ID_PACK_OVERCURRENT_DISCHARGE_MSL,       /*!< 电池包级别过流 */
    DIAG_ID_CURRENT_ON_OPEN_STRING,               /*!< 断开的串上有电流流过 */
    DIAG_ID_DEEP_DISCHARGE_DETECTED,              /*!< 在持久存储器中设置了深度放电标志 */
    DIAG_ID_AFE_OPEN_WIRE, /*!< 在电池单体测量中检测到开路（断线）的感测线 */
    DIAG_ID_PLAUSIBILITY_PACK_VOLTAGE, /*!< 合理性检查模块判定电池包电压不合理 */
    DIAG_ID_INTERLOCK_FEEDBACK, /*!< 联锁反馈指示其处于断开状态（但预期应为闭合） */
    DIAG_ID_STRING_MINUS_CONTACTOR_FEEDBACK, /*!< 串负极接触器的反馈与预期值不匹配 */
    DIAG_ID_STRING_PLUS_CONTACTOR_FEEDBACK,  /*!< 串正极接触器的反馈与预期值不匹配 */
    DIAG_ID_PRECHARGE_CONTACTOR_FEEDBACK, /*!< 预充接触器的反馈与预期值不匹配 */
    DIAG_ID_SBC_FIN_ERROR,                /*!< SBC 中 FIN 信号的状态不正常 */
    DIAG_ID_SBC_RSTB_ERROR,               /*!< 检测到 SBC 的 RSTB 引脚被激活 */
    DIAG_ID_BASE_CELL_VOLTAGE_MEASUREMENT_TIMEOUT, /*!< 冗余模块检测到基础电池电压测量值缺失 */
    DIAG_ID_REDUNDANCY0_CELL_VOLTAGE_MEASUREMENT_TIMEOUT, /*!< 冗余模块检测到冗余0电池电压测量值缺失 */
    DIAG_ID_BASE_CELL_TEMPERATURE_MEASUREMENT_TIMEOUT,    /*!< 冗余模块检测到基础电池温度测量值缺失 */
    DIAG_ID_REDUNDANCY0_CELL_TEMPERATURE_MEASUREMENT_TIMEOUT, /*!< 冗余模块检测到冗余0温度测量值缺失 */
    DIAG_ID_PRECHARGE_ABORT_REASON_VOLTAGE, /*!< 由于电压差过高导致预充中止 */
    DIAG_ID_PRECHARGE_ABORT_REASON_CURRENT, /*!< 由于测量的电流过高导致预充中止 */
    DIAG_ID_CURRENT_MEASUREMENT_TIMEOUT,    /*!< 冗余模块检测到某串的电流测量值未更新 */
    DIAG_ID_CURRENT_MEASUREMENT_ERROR, /*!< 冗余模块检测到电流测量值无效 */
    DIAG_ID_CURRENT_SENSOR_V1_MEASUREMENT_TIMEOUT,    /*!< 冗余模块检测到电流传感器的电压 1 测量值未更新 */
    DIAG_ID_CURRENT_SENSOR_V2_MEASUREMENT_TIMEOUT,    /*!< 冗余模块检测到电流传感器的电压 2 测量值未更新 */
    DIAG_ID_CURRENT_SENSOR_V3_MEASUREMENT_TIMEOUT,    /*!< 冗余模块检测到电流传感器的电压 3 测量值未更新 */
    DIAG_ID_CURRENT_SENSOR_POWER_MEASUREMENT_TIMEOUT, /*!< 冗余模块检测到电流传感器的功率测量值未更新 */
    DIAG_ID_POWER_MEASUREMENT_ERROR,      /*!< 冗余模块检测到功率测量值无效 */
    DIAG_ID_INSULATION_MEASUREMENT_VALID, /*!< 绝缘测量有效或无效 */
    DIAG_ID_LOW_INSULATION_RESISTANCE_ERROR,   /*!< 测量到极低的绝缘电阻 */
    DIAG_ID_LOW_INSULATION_RESISTANCE_WARNING, /*!< 测量到警告级别的低绝缘电阻 */
    DIAG_ID_INSULATION_GROUND_ERROR,           /*!< 绝缘监控检测到接地错误 */
    DIAG_ID_I2C_PEX_ERROR,                     /*!< 端口扩展器的一般错误 */
    DIAG_ID_I2C_RTC_ERROR,                     /*!< 与 RTC 的 i2c 通信错误 */
    DIAG_ID_RTC_CLOCK_INTEGRITY_ERROR,         /*!< RTC IC 中的时钟完整性无法保证错误 */
    DIAG_ID_RTC_BATTERY_LOW_ERROR,             /*!< RTC IC 电池电量低标志已设置 */
    DIAG_ID_FRAM_READ_CRC_ERROR,               /*!< 从 FRAM 读取时 CRC 不匹配 */
    DIAG_ID_ALERT_MODE,                    /*!< 断开接触器时发生严重错误。熔断器未触发 */
    DIAG_ID_AEROSOL_ALERT,                 /*!< 检测到高浓度气溶胶 */
    DIAG_ID_SUPPLY_VOLTAGE_CLAMP_30C_LOST, /*!< 夹紧 30C 的电源电压丢失 */
    DIAG_ID_AFE_ALARM,                     /*!< 警报线显示发生错误 */
    DIAG_ID_MAX,                           /*!< 最大指示符 - 请勿更改 */
} DIAG_ID_e;

/** 诊断检查结果（事件） */
typedef enum {
    DIAG_EVENT_OK,     /**< 诊断通道事件正常 */
    DIAG_EVENT_NOT_OK, /**< 诊断通道事件异常 */
    DIAG_EVENT_RESET,  /**< 将诊断通道事件计数器重置为 0 */
} DIAG_EVENT_e;

/** 启用或禁用事件的处理诊断 */
typedef enum {
    DIAG_EVALUATION_ENABLED,  /**< 启用诊断处理的评估 */
    DIAG_EVALUATION_DISABLED, /**< 禁用诊断处理的评估 */
} DIAG_EVALUATE_e;

/** 诊断事件的影响级别，例如，影响整个系统还是仅影响某个串 */
typedef enum {
    DIAG_SYSTEM, /**< 诊断事件影响与系统相关，例如 CAN 时序 */
    DIAG_STRING, /**< 诊断事件影响与串相关，例如串 x 过压 */
} DIAG_IMPACT_LEVEL_e;

/**
 * @def     DIAG_CAN_TIMING
 * @brief   写入该字段的值，用于描述是否应生成 CAN 时序诊断条目
 */
#if BS_CHECK_CAN_TIMING == true
#define DIAG_CAN_TIMING (DIAG_EVALUATION_ENABLED)
#else
#define DIAG_CAN_TIMING (DIAG_EVALUATION_DISABLED)
#endif

/** 诊断严重级别 */
typedef enum {
    DIAG_FATAL_ERROR, /*!< 严重级别：致命错误 */
    DIAG_WARNING,     /*!< 严重级别：警告 */
    DIAG_INFO,        /*!< 严重级别：信息 */
} DIAG_SEVERITY_LEVEL_e;

/**
 * @brief   诊断回调函数的类型
 * @param[in] diagId        诊断条目的 ID
 * @param[in] event         #DIAG_EVENT_e
 * @param[in] kpkDiagShim   指向数据库条目的垫片指针
 * @param[in] data          数据
 */
typedef void DIAG_CALLBACK_FUNCTION_f(
    DIAG_ID_e diagId,
    DIAG_EVENT_e event,
    const DIAG_DATABASE_SHIM_s *const kpkDiagShim,
    uint32_t data);

/** 一个诊断通道的通道配置 */
typedef struct {
    DIAG_ID_e id;       /**< 诊断事件 ID diag_id */
    uint16_t threshold; /**< 在生成通知之前容忍的事件数阈值，双向生效：
                         * threshold = 0：在第一次发生时报告该值，
                         * threshold = 1：在第二次发生时报告该值 */
    DIAG_SEVERITY_LEVEL_e
        severity;      /**< 诊断条目的严重性，#DIAG_FATAL_ERROR 将导致接触器断开 */
    uint32_t delay_ms; /**< 如果严重级别为 #DIAG_FATAL_ERROR，在检测到错误后延迟（毫秒）直到断开接触器 */
    DIAG_EVALUATE_e enable_evaluate;      /**< 如果启用，将评估诊断事件 */
    DIAG_CALLBACK_FUNCTION_f *fpCallback; /**< 如果事件数量在双向超过阈值，
                                           * 将使用参数 DIAG_EVENT_e、串 ID 或系统相关数据调用此函数 */
} DIAG_ID_CFG_s;

/** 诊断模块的设备配置结构体 */
typedef struct {
    uint8_t nrOfConfiguredDiagnosisEntries;          /*!< DIAG_ID_CFG_s 中的条目数 */
    DIAG_ID_CFG_s *pConfigurationOfDiagnosisEntries; /*!< 指向所有诊断条目的配置数组的指针 */
    uint16_t numberOfFatalErrors; /*!< 配置的严重级别为 #DIAG_FATAL_ERROR 的诊断条目数量 */
    DIAG_ID_CFG_s *pFatalErrorLinkTable[DIAG_ID_MAX]; /*!< 包含指向所有严重级别为 #DIAG_FATAL_ERROR 
                                                        的诊断条目指针的列表 */
} DIAG_DEV_s;

/*========== 外部常量和变量声明 ======================*/
/** 诊断设备配置结构体 */
extern DIAG_DEV_s diag_device;
extern DIAG_ID_CFG_s diag_diagnosisIdConfiguration[DIAG_ID_MAX];

/*========== 外部函数原型 =====================================*/
/**
 * @brief   诊断标志的更新函数
 * @details TODO
 */
extern void DIAG_UpdateFlags(void);

/*========== 外部化的静态函数原型 (单元测试) ===========*/
#ifdef UNITY_UNIT_TEST
#endif

#endif /* FOXBMS__DIAG_CFG_H_ */

