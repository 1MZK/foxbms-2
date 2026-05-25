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
 * @file    sof_trapezoid_cfg.h
 * @author  foxBMS 团队
 * @date    2020-10-07 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup APPLICATION_CONFIGURATION
 * @prefix  SOF
 *
 * @brief   SOF（安全工作区）配置头文件
 * @details TODO
 */

#ifndef FOXBMS__SOF_TRAPEZOID_CFG_H_
#define FOXBMS__SOF_TRAPEZOID_CFG_H_

/*========== 包含文件 =======================================================*/

#include "battery_cell_cfg.h"
#include "battery_system_cfg.h"

#include <math.h>
#include <stdint.h>

/*========== 宏和定义 =========================================================*/

/**
 * 串在充电方向上可以承受的最大电流（单位：毫安）。
 * 通常根据具体电池单体的数据手册设定一次
 */
#define SOF_STRING_CURRENT_CONTINUOUS_CHARGE_mA \
    ((float_t)BC_CURRENT_MAX_CHARGE_MOL_mA * BS_NR_OF_PARALLEL_CELLS_PER_CELL_BLOCK)

/**
 * 串在放电方向上可以输出的最大电流（单位：毫安）。
 * 通常根据具体电池单体的数据手册设定一次。
 */
#define SOF_STRING_CURRENT_CONTINUOUS_DISCHARGE_mA \
    ((float_t)BC_CURRENT_MAX_DISCHARGE_MOL_mA * BS_NR_OF_PARALLEL_CELLS_PER_CELL_BLOCK)

/**
 * 在跛行回家模式下，串应该能够输出的电流（单位：毫安），
 * 即发生了非关键性故障，但车辆仍应能开回家。该值由系统工程师选定。
 */
#define SOF_STRING_CURRENT_LIMP_HOME_mA (20000.00f)

/**
 * 开始对最大放电电流进行降额的低温阈值（单位：0.1 摄氏度），
 * 即低于此温度时，电池包不应输出完整的放电电流。
 */
#define SOF_TEMPERATURE_LOW_CUTOFF_DISCHARGE_ddegC (BC_TEMPERATURE_MIN_DISCHARGE_MOL_ddegC)

/**
 * 对最大放电电流完全施加降额的低温阈值（单位：0.1 摄氏度），
 * 即低于此温度时，电池包不应在放电方向上输出任何电流。
 */
#define SOF_TEMPERATURE_LOW_LIMIT_DISCHARGE_ddegC (BC_TEMPERATURE_MIN_DISCHARGE_MSL_ddegC)

/**
 * 开始对最大充电电流进行降额的低温阈值（单位：0.1 摄氏度），
 * 即低于此温度时，电池包不应输入完整的充电电流。
 */
#define SOF_TEMPERATURE_LOW_CUTOFF_CHARGE_ddegC (BC_TEMPERATURE_MIN_CHARGE_MOL_ddegC)

/**
 * 对最大充电电流完全施加降额的低温阈值（单位：0.1 摄氏度），
 * 即低于此温度时，电池包不应在充电方向上输入任何电流。
 */
#define SOF_TEMPERATURE_LOW_LIMIT_CHARGE_ddegC (BC_TEMPERATURE_MIN_CHARGE_MSL_ddegC)

/**
 * 开始对最大放电电流进行降额的高温阈值（单位：0.1 摄氏度），
 * 即高于此温度时，电池包不应输出完整的放电电流。
 */
#define SOF_TEMPERATURE_HIGH_CUTOFF_DISCHARGE_ddegC (BC_TEMPERATURE_MAX_DISCHARGE_MOL_ddegC)

/**
 * 对最大放电电流完全施加降额的高温阈值（单位：0.1 摄氏度），
 * 即高于此温度时，电池包不应在放电方向上输出任何电流。
 */
#define SOF_TEMPERATURE_HIGH_LIMIT_DISCHARGE_ddegC (BC_TEMPERATURE_MAX_DISCHARGE_MSL_ddegC)

/**
 * 开始对最大充电电流进行降额的高温阈值（单位：0.1 摄氏度），
 * 即高于此温度时，电池包不应输入完整的充电电流。
 */
#define SOF_TEMPERATURE_HIGH_CUTOFF_CHARGE_ddegC (BC_TEMPERATURE_MAX_CHARGE_MOL_ddegC)

/**
 * 对最大充电电流完全施加降额的高温阈值（单位：0.1 摄氏度），
 * 即高于此温度时，电池包不应在充电方向上输入任何电流。
 */
#define SOF_TEMPERATURE_HIGH_LIMIT_CHARGE_ddegC (BC_TEMPERATURE_MAX_CHARGE_MSL_ddegC)

/**
 * 高于此电压值时，电池包不应在充电方向上承受完整电流。
 */
#define SOF_VOLTAGE_CUTOFF_CHARGE_mV (BC_VOLTAGE_MAX_MOL_mV)

/**
 * 高于此电压值时，电池包不应在充电方向上承受任何电流。
 */
#define SOF_VOLTAGE_LIMIT_CHARGE_mV (BC_VOLTAGE_MAX_RSL_mV)

/**
 * 低于此电压值时，电池包不应在放电方向上输出完整电流。
 */
#define SOF_VOLTAGE_CUTOFF_DISCHARGE_mV (BC_VOLTAGE_MIN_MOL_mV)

/**
 * 低于此电压值时，电池包不应在放电方向上输出任何电流。
 */
#define SOF_VOLTAGE_LIMIT_DISCHARGE_mV (BC_VOLTAGE_MIN_RSL_mV)

/**
 * SoF（安全工作区）计算的配置结构体
 */
typedef struct {
    /** 电流降额限制 @{ */
    float_t maximumDischargeCurrent_mA;      /*!< 最大放电电流 */
    float_t maximumChargeCurrent_mA;        /*!< 最大充电电流 */
    float_t limpHomeCurrent_mA;             /*!< 跛行回家电流 */
    /**@}*/

    /** 低温降额限制 @{ */
    int16_t cutoffLowTemperatureDischarge_ddegC;  /*!< 放电低温降额起始点 */
    int16_t limitLowTemperatureDischarge_ddegC;   /*!< 放电低温降额截止点（完全限制） */
    int16_t cutoffLowTemperatureCharge_ddegC;     /*!< 充电低温降额起始点 */
    int16_t limitLowTemperatureCharge_ddegC;      /*!< 充电低温降额截止点（完全限制） */
    /**@}*/

    /** 高温降额限制 @{ */
    int16_t cutoffHighTemperatureDischarge_ddegC;  /*!< 放电高温降额起始点 */
    int16_t limitHighTemperatureDischarge_ddegC;   /*!< 放电高温降额截止点（完全限制） */
    int16_t cutoffHighTemperatureCharge_ddegC;     /*!< 充电高温降额起始点 */
    int16_t limitHighTemperatureCharge_ddegC;      /*!< 充电高温降额截止点（完全限制） */
    /**@}*/

    /** 单体电压降额限制 @{ */
    int16_t cutoffUpperCellVoltage_mV;      /*!< 充电高压降额起始点 */
    int16_t limitUpperCellVoltage_mV;       /*!< 充电高压降额截止点（完全限制） */
    int16_t cutoffLowerCellVoltage_mV;      /*!< 放电低压降额起始点 */
    int16_t limitLowerCellVoltage_mV;       /*!< 放电低压降额截止点（完全限制） */
    /**@}*/
} SOF_CONFIG_s;

/*========== 外部常量和变量声明 ======================*/

/**
 * 推荐电池电流的 SOF 窗口的配置值
 */
extern const SOF_CONFIG_s sof_recommendedCurrent;

/*========== 外部函数原型 =====================================*/

/*========== 外部化的静态函数原型 (单元测试) ===========*/
#ifdef UNITY_UNIT_TEST
#endif

#endif /* FOXBMS__SOF_TRAPEZOID_CFG_H_ */

