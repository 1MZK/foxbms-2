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
 * @file    sof_trapezoid.c
 * @author  foxBMS 团队
 * @date    2020-10-07 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup APPLICATION_CONFIGURATION
 * @prefix  SOF
 *
 * @brief   负责电流降额计算的 SOF 模块
 * @details TODO
 */

/*========== 包含文件 =======================================================*/
#include "sof_trapezoid.h"

#include "battery_cell_cfg.h"
#include "battery_system_cfg.h"

#include "bms.h"
#include "database.h"
#include "foxmath.h"
#include "state_estimation.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/*========== 宏和定义 =========================================================*/

/*========== 静态常量和变量定义 =======================*/
/** @{
 * 模块局部静态变量，在启动时计算，后续使用以避免运行时进行除法运算
 */
static SOF_CURVE_s sof_curveRecommendedOperatingCurrent;
/** @} */

/** 数据库表的本地副本 */
/**@{*/
static DATA_BLOCK_MIN_MAX_s sof_tableMinimumMaximumValues = {.header.uniqueId = DATA_BLOCK_ID_MIN_MAX};
static DATA_BLOCK_SOF_s sof_tableSofValues                = {.header.uniqueId = DATA_BLOCK_ID_SOF};
/**@}*/

/*========== 外部常量和变量定义 =======================*/

/*========== 静态函数原型 =====================================*/

/**
 * @brief   根据配置值计算 SOF 曲线
 *
 * @param[in]  pConfigurationValues         SOF 曲线配置值
 * @param[out] pCalculatedSofCurveValues    计算出的 SOF 曲线
 */
static void SOF_CalculateCurves(const SOF_CONFIG_s *pConfigurationValues, SOF_CURVE_s *pCalculatedSofCurveValues);

/**
 *  @brief  根据电压数据（即最小和最大电压）计算 SoF
 *
 * @param[in]  minimumCellVoltage_mV        最小单体电压
 * @param[in]  maximumCellVoltage_mV        最大单体电压
 * @param[out] pAllowedVoltageBasedCurrent  基于电压的 SOF
 * @param[in]  pConfigLimitValues           指向 SOF 配置结构体的指针
 * @param[in]  pCalculatedSofCurves         指向 SOF 曲线结构体的指针
 */
static void SOF_CalculateVoltageBasedCurrentLimit(
    int16_t minimumCellVoltage_mV,
    int16_t maximumCellVoltage_mV,
    SOF_CURRENT_LIMITS_s *pAllowedVoltageBasedCurrent,
    const SOF_CONFIG_s *pConfigLimitValues,
    SOF_CURVE_s *pCalculatedSofCurves);

/**
 * @brief   根据温度数据（即电芯的最小和最大温度）计算 SoF
 *
 * @param[in]  minimumCellTemperature_ddegC      电芯最低温度
 * @param[in]  maximumCellTemperature_ddegC      电芯最高温度
 * @param[out] pAllowedTemperatureBasedCurrent   存储结果的指针
 * @param[in]  pConfigLimitValues                指向用于 SOF 计算的结构体的指针
 * @param[in]  pCalculatedSofCurves              指向包含限值的结构体的指针
 */
static void SOF_CalculateTemperatureBasedCurrentLimit(
    int16_t minimumCellTemperature_ddegC,
    int16_t maximumCellTemperature_ddegC,
    SOF_CURRENT_LIMITS_s *pAllowedTemperatureBasedCurrent,
    const SOF_CONFIG_s *pConfigLimitValues,
    SOF_CURVE_s *pCalculatedSofCurves);

/**
 * @brief   获取所有 SoF 计算变体中的最小电流值
 *
 * @param[in]  voltageBasedLimits       电压约束的电流降额值
 * @param[in]  temperatureBasedLimits   温度约束的电流降额值
 *
 * @return 最小的 SoF 电流值
 */
static SOF_CURRENT_LIMITS_s SOF_MinimumOfTwoSofValues(
    SOF_CURRENT_LIMITS_s voltageBasedLimits,
    SOF_CURRENT_LIMITS_s temperatureBasedLimits);

/*========== 静态函数实现 ================================*/
static void SOF_CalculateCurves(const SOF_CONFIG_s *pConfigurationValues, SOF_CURVE_s *pCalculatedSofCurveValues) {
    FAS_ASSERT(pConfigurationValues != NULL_PTR);
    FAS_ASSERT(pCalculatedSofCurveValues != NULL_PTR);

    /* 计算 MOL/RSL/MSL 最大允许电流的 SOF 曲线 */
    pCalculatedSofCurveValues->slopeLowTemperatureDischarge =
        (pConfigurationValues->maximumDischargeCurrent_mA - pConfigurationValues->limpHomeCurrent_mA) /
        (pConfigurationValues->cutoffLowTemperatureDischarge_ddegC -
         pConfigurationValues->limitLowTemperatureDischarge_ddegC);
    pCalculatedSofCurveValues->offsetLowTemperatureDischarge =
        pConfigurationValues->limpHomeCurrent_mA - (pCalculatedSofCurveValues->slopeLowTemperatureDischarge *
                                                    pConfigurationValues->limitLowTemperatureDischarge_ddegC);

    pCalculatedSofCurveValues->slopeHighTemperatureDischarge =
        (0.0f - pConfigurationValues->maximumDischargeCurrent_mA) /
        (pConfigurationValues->limitHighTemperatureDischarge_ddegC -
         pConfigurationValues->cutoffHighTemperatureDischarge_ddegC);
    pCalculatedSofCurveValues->offsetHighTemperatureDischarge =
        0.0f - (pCalculatedSofCurveValues->slopeHighTemperatureDischarge *
                pConfigurationValues->limitHighTemperatureDischarge_ddegC);

    pCalculatedSofCurveValues->slopeLowTemperatureCharge = (pConfigurationValues->maximumChargeCurrent_mA - 0.0f) /
                                                           (pConfigurationValues->cutoffLowTemperatureCharge_ddegC -
                                                            pConfigurationValues->limitLowTemperatureCharge_ddegC);
    pCalculatedSofCurveValues->offsetLowTemperatureCharge =
        0.0f -
        (pCalculatedSofCurveValues->slopeLowTemperatureCharge * pConfigurationValues->limitLowTemperatureCharge_ddegC);

    pCalculatedSofCurveValues->slopeHighTemperatureCharge = (0.0f - pConfigurationValues->maximumChargeCurrent_mA) /
                                                            (pConfigurationValues->limitHighTemperatureCharge_ddegC -
                                                             pConfigurationValues->cutoffHighTemperatureCharge_ddegC);
    pCalculatedSofCurveValues->offsetHighTemperatureCharge = 0.0f -
                                                             (pCalculatedSofCurveValues->slopeHighTemperatureCharge *
                                                              pConfigurationValues->limitHighTemperatureCharge_ddegC);

    pCalculatedSofCurveValues->slopeUpperCellVoltage =
        (pConfigurationValues->maximumDischargeCurrent_mA - 0.0f) /
        (pConfigurationValues->cutoffLowerCellVoltage_mV - pConfigurationValues->limitLowerCellVoltage_mV);
    pCalculatedSofCurveValues->offsetUpperCellVoltage =
        0.0f - (pCalculatedSofCurveValues->slopeUpperCellVoltage * pConfigurationValues->limitLowerCellVoltage_mV);

    pCalculatedSofCurveValues->slopeLowerCellVoltage =
        (pConfigurationValues->maximumChargeCurrent_mA - 0.0f) /
        (pConfigurationValues->cutoffUpperCellVoltage_mV - pConfigurationValues->limitUpperCellVoltage_mV);
    pCalculatedSofCurveValues->offsetLowerCellVoltage =
        0.0f - (pCalculatedSofCurveValues->slopeLowerCellVoltage * pConfigurationValues->limitLowerCellVoltage_mV);
}

/**
 * @brief   根据电压数据计算基于电压的安全工作区（SOF）电流限制
 * @details 此函数通过比较电池单体的最小和最大电压与配置的电压降额阈值，
 *          计算在当前电压状态下电池允许的充电和放电电流。
 *          降额机制采用梯形曲线策略：
 *          - 当电压处于安全区间时，允许最大连续/峰值电流；
 *          - 当电压进入降额区间时，按线性比例降低允许电流；
 *          - 当电压达到极限值时，将允许电流降为 0（或限制值）。
 *
 * @param[in]  minimumCellVoltage_mV        电池组中当前的最小单体电压（单位：毫伏）
 * @param[in]  maximumCellVoltage_mV        电池组中当前的最大单体电压（单位：毫伏）
 * @param[out] pAllowedVoltageBasedCurrent  输出参数，指向存储计算结果的 SOF_CURRENT_LIMITS_s 结构体指针，
 *                                          包含计算得出的连续充电/放电电流和峰值充电/放电电流
 * @param[in]  pConfigLimitValues           输入参数，指向 SOF 配置结构体的指针，
 *                                          包含电压降额的截止点和限制点等配置阈值
 * @param[in]  pCalculatedSofCurves         输入参数，指向 SOF 曲线结构体的指针，
 *                                          包含预先计算好的降额斜率和偏移量，用于线性插值计算
 *
 * @note    最小单体电压主要用于限制放电电流（防止过放），最大单体电压主要用于限制充电电流（防止过充）。
 */
static void SOF_CalculateVoltageBasedCurrentLimit(
    int16_t minimumCellVoltage_mV,              // 最低单体电压，单位：毫伏
    int16_t maximumCellVoltage_mV,              // 最高单体电压，单位：毫伏
    SOF_CURRENT_LIMITS_s *pAllowedVoltageBasedCurrent, // 指针：用于存储计算出的允许电流结果
    const SOF_CONFIG_s *pConfigLimitValues,     // 指针：指向常量的配置参数（限制值）
    SOF_CURVE_s *pCalculatedSofCurves) {        // 指针：指向SOF曲线参数（斜率等）

    /* 断言检查，确保传入的指针不为空，防止空指针解引用引发崩溃 */
    FAS_ASSERT(pAllowedVoltageBasedCurrent != NULL_PTR);
    FAS_ASSERT(pConfigLimitValues != NULL_PTR);
    FAS_ASSERT(pCalculatedSofCurves != NULL_PTR);
    
    /* AXIVION 常规 Generic-MissingParameterAssert: minimumCellVoltage_mV: 参数接受整个范围 */
    /* AXIVION 常规 Generic-MissingParameterAssert: maximumCellVoltage_mV: 参数接受整个范围 */
    // 注：AXIVION是静态代码分析工具，这两行注释是为了告诉工具不需要对这两个int16_t参数做范围断言，因为它们合法地占用了int16_t的全量程范围

    /* ==================== 最小单体电压计算（放电电流限制） ==================== */
    // 逻辑分三段：1. 低于下限极限(禁止放电)；2. 介于下限极限与截止下限之间(线性降额)；3. 高于截止下限(满额放电)
    
    if (minimumCellVoltage_mV <= pConfigLimitValues->limitLowerCellVoltage_mV) {
        // 情况1：最低单体电压已经低于或等于"下限极限电压"（严重过放边缘）
        // 此时为了保护电池，不允许任何放电电流
        pAllowedVoltageBasedCurrent->continuousDischargeCurrent_mA = 0.0f; // 持续放电电流设为0
        pAllowedVoltageBasedCurrent->peakDischargeCurrent_mA       = 0.0f; // 峰值放电电流设为0
    } else {
        // 最低电压高于下限极限电压
        if (minimumCellVoltage_mV <= pConfigLimitValues->cutoffLowerCellVoltage_mV) {
            // 情况2：最低单体电压介于"下限极限电压"和"截止下限电压"之间（电压偏低，需要降额放电）
            // 使用线性斜率计算：斜率 * (当前电压 - 下限极限电压)，电流随电压上升而线性增加
            pAllowedVoltageBasedCurrent->continuousDischargeCurrent_mA =
                (pCalculatedSofCurves->slopeUpperCellVoltage *
                 (minimumCellVoltage_mV - pConfigLimitValues->limitLowerCellVoltage_mV));
            // 在此降额区间，峰值放电电流等于持续放电电流（不再允许额外的脉冲电流）
            pAllowedVoltageBasedCurrent->peakDischargeCurrent_mA =
                pAllowedVoltageBasedCurrent->continuousDischargeCurrent_mA;
        } else {
            // 情况3：最低单体电压高于"截止下限电压"（电压健康，可以满功率放电）
            // 允许的持续放电电流和峰值放电电流均取配置的最大放电电流
            pAllowedVoltageBasedCurrent->continuousDischargeCurrent_mA = pConfigLimitValues->maximumDischargeCurrent_mA;
            pAllowedVoltageBasedCurrent->peakDischargeCurrent_mA       = pConfigLimitValues->maximumDischargeCurrent_mA;
        }
    }

    /* ==================== 最大单体电压计算（充电电流限制） ==================== */
    // 逻辑分三段：1. 高于上限极限(禁止充电)；2. 介于上限极限与截止上限之间(线性降额)；3. 低于截止上限(满额充电)

    if (maximumCellVoltage_mV >= pConfigLimitValues->limitUpperCellVoltage_mV) {
        // 情况1：最高单体电压已经高于或等于"上限极限电压"（严重过充边缘）
        // 此时为了保护电池，不允许任何充电电流
        pAllowedVoltageBasedCurrent->continuousChargeCurrent_mA = 0.0f; // 持续充电电流设为0
        pAllowedVoltageBasedCurrent->peakChargeCurrent_mA       = 0.0f; // 峰值充电电流设为0
    } else {
        // 最高电压低于上限极限电压
        if (maximumCellVoltage_mV >= pConfigLimitValues->cutoffUpperCellVoltage_mV) {
            // 情况2：最高单体电压介于"上限极限电压"和"截止上限电压"之间（电压偏高，需要降额充电）
            // 使用线性斜率计算：斜率 * (当前电压 - 上限极限电压)，注意此时斜率为负值，电流随电压下降而线性增加
            pAllowedVoltageBasedCurrent->continuousChargeCurrent_mA =
                (pCalculatedSofCurves->slopeLowerCellVoltage *
                 (maximumCellVoltage_mV - pConfigLimitValues->limitUpperCellVoltage_mV));
            // 在此降额区间，峰值充电电流等于持续充电电流
            pAllowedVoltageBasedCurrent->peakChargeCurrent_mA = pAllowedVoltageBasedCurrent->continuousChargeCurrent_mA;
        } else {
            // 情况3：最高单体电压低于"截止上限电压"（电压健康，可以满功率充电）
            // 允许的持续充电电流和峰值充电电流均取配置的最大充电电流
            pAllowedVoltageBasedCurrent->continuousChargeCurrent_mA = pConfigLimitValues->maximumChargeCurrent_mA;
            pAllowedVoltageBasedCurrent->peakChargeCurrent_mA       = pConfigLimitValues->maximumChargeCurrent_mA;
        }
    }
}

/**
 * @brief 基于温度计算当前允许的电流限制（SOF - Safe Operating Area）
 * @param minimumCellTemperature_ddegC 当前最低单体温度，单位：0.1℃
 * @param maximumCellTemperature_ddegC 当前最高单体温度，单位：0.1℃
 * @param pAllowedTemperatureBasedCurrent 输出参数，基于温度计算出的允许电流限制值
 * @param pConfigLimitValues 输入参数，包含温度和电流限制的配置参数
 * @param pCalculatedSofCurves 输入参数，包含预先计算好的SOF曲线斜率和截距
 */
static void SOF_CalculateTemperatureBasedCurrentLimit(
    int16_t minimumCellTemperature_ddegC,              // 最低单体温度，单位：0.1℃ (例如 200 表示 20.0℃)
    int16_t maximumCellTemperature_ddegC,              // 最高单体温度，单位：0.1℃
    SOF_CURRENT_LIMITS_s *pAllowedTemperatureBasedCurrent, // 指针：用于存储计算出的允许电流结果
    const SOF_CONFIG_s *pConfigLimitValues,            // 指针：指向常量的配置参数（限制值）
    SOF_CURVE_s *pCalculatedSofCurves) {               // 指针：指向SOF曲线参数（斜率和截距）

    /* 断言检查，确保传入的指针不为空，防止空指针解引用引发崩溃 */
    FAS_ASSERT(pAllowedTemperatureBasedCurrent != NULL_PTR);
    FAS_ASSERT(pConfigLimitValues != NULL_PTR);
    FAS_ASSERT(pCalculatedSofCurves != NULL_PTR);
    
    /* AXIVION 常规 Generic-MissingParameterAssert: minimumCellTemperature_ddegC: 参数接受整个范围 */
    /* AXIVION 常规 Generic-MissingParameterAssert: maximumCellTemperature_ddegC: 参数接受整个范围 */
    // 注：AXIVION是静态代码分析工具，这两行注释是为了告诉工具不需要对这两个int16_t参数做范围断言

    /* 初始化临时变量，用于存储基于最高温度计算的中间结果 */
    SOF_CURRENT_LIMITS_s temporaryCurrentLimits = {0.0f, 0.0f, 0.0f, 0.0f};

    /* ==================== 低温放电计算 ==================== */
    // 逻辑分三段：1. 低于下限极限(跛行模式)；2. 介于下限极限与截止下限之间(线性降额)；3. 高于截止下限(满额放电)
    
    if (minimumCellTemperature_ddegC <= pConfigLimitValues->limitLowTemperatureDischarge_ddegC) {
        // 情况1：最低温度低于或等于"低温放电极限温度"（极冷状态）
        // 此时进入跛行模式，仅允许极小的电流维持基本功能，防止电池损坏
        pAllowedTemperatureBasedCurrent->continuousDischargeCurrent_mA = pConfigLimitValues->limpHomeCurrent_mA;
        pAllowedTemperatureBasedCurrent->peakDischargeCurrent_mA       = pConfigLimitValues->limpHomeCurrent_mA;
    } else {
        // 最低温度高于低温放电极限温度
        if (minimumCellTemperature_ddegC <= pConfigLimitValues->cutoffLowTemperatureDischarge_ddegC) {
            // 情况2：最低温度介于"低温放电极限温度"和"低温放电截止温度"之间（温度偏低，需要降额放电）
            // 使用线性方程计算：斜率 * 温度 + 截距，电流随温度升高而线性增加
            pAllowedTemperatureBasedCurrent->continuousDischargeCurrent_mA =
                (pCalculatedSofCurves->slopeLowTemperatureDischarge * minimumCellTemperature_ddegC) +
                pCalculatedSofCurves->offsetLowTemperatureDischarge;
            // 在此降额区间，峰值放电电流等于持续放电电流
            pAllowedTemperatureBasedCurrent->peakDischargeCurrent_mA =
                pAllowedTemperatureBasedCurrent->continuousDischargeCurrent_mA;
        } else {
            // 情况3：最低温度高于"低温放电截止温度"（温度适宜，可以满功率放电）
            pAllowedTemperatureBasedCurrent->continuousDischargeCurrent_mA =
                pConfigLimitValues->maximumDischargeCurrent_mA;
            pAllowedTemperatureBasedCurrent->peakDischargeCurrent_mA = pConfigLimitValues->maximumDischargeCurrent_mA;
        }
    }

    /* ==================== 低温充电计算 ==================== */
    // 逻辑分三段：1. 低于下限极限(禁止充电)；2. 介于下限极限与截止下限之间(线性降额)；3. 高于截止下限(满额充电)

    if (minimumCellTemperature_ddegC <= pConfigLimitValues->limitLowTemperatureCharge_ddegC) {
        // 情况1：最低温度低于或等于"低温充电极限温度"（极冷状态）
        // 此时禁止充电，防止析锂导致电池内部短路
        pAllowedTemperatureBasedCurrent->continuousChargeCurrent_mA = 0;
        pAllowedTemperatureBasedCurrent->peakChargeCurrent_mA       = 0;
    } else {
        // 最低温度高于低温充电极限温度
        if (minimumCellTemperature_ddegC <= pConfigLimitValues->cutoffLowTemperatureCharge_ddegC) {
            // 情况2：最低温度介于"低温充电极限温度"和"低温充电截止温度"之间（温度偏低，需要降额充电）
            // 使用线性方程计算：斜率 * 温度 + 截距，电流随温度升高而线性增加
            pAllowedTemperatureBasedCurrent->continuousChargeCurrent_mA =
                (pCalculatedSofCurves->slopeLowTemperatureCharge * minimumCellTemperature_ddegC) +
                pCalculatedSofCurves->offsetLowTemperatureCharge;
            // 在此降额区间，峰值充电电流等于持续充电电流
            pAllowedTemperatureBasedCurrent->peakChargeCurrent_mA =
                pAllowedTemperatureBasedCurrent->continuousChargeCurrent_mA;
        } else {
            // 情况3：最低温度高于"低温充电截止温度"（温度适宜，可以满功率充电）
            pAllowedTemperatureBasedCurrent->continuousChargeCurrent_mA = pConfigLimitValues->maximumChargeCurrent_mA;
            pAllowedTemperatureBasedCurrent->peakChargeCurrent_mA       = pConfigLimitValues->maximumChargeCurrent_mA;
        }
    }

    /* ==================== 高温放电计算 ==================== */
    // 逻辑分三段：1. 高于上限极限(禁止放电)；2. 介于上限极限与截止上限之间(线性降额)；3. 低于截止上限(满额放电)
    // 注意：这里需要与低温放电的结果取最小值，因为放电受限于最低温度和最高温度中的最严苛条件

    if (maximumCellTemperature_ddegC >= pConfigLimitValues->limitHighTemperatureDischarge_ddegC) {
        // 情况1：最高温度高于或等于"高温放电极限温度"（极热状态）
        // 此时禁止放电，防止电池热失控
        pAllowedTemperatureBasedCurrent->continuousDischargeCurrent_mA = 0.0f;
        pAllowedTemperatureBasedCurrent->peakDischargeCurrent_mA       = 0.0f;
    } else {
        // 最高温度低于高温放电极限温度
        if (maximumCellTemperature_ddegC >= pConfigLimitValues->cutoffHighTemperatureDischarge_ddegC) {
            // 情况2：最高温度介于"高温放电极限温度"和"高温放电截止温度"之间（温度偏高，需要降额放电）
            // 使用线性方程计算：斜率 * 温度 + 截距，电流随温度降低而线性增加
            temporaryCurrentLimits.continuousDischargeCurrent_mA =
                (pCalculatedSofCurves->slopeHighTemperatureDischarge * maximumCellTemperature_ddegC) +
                pCalculatedSofCurves->offsetHighTemperatureDischarge;
            // 在此降额区间，峰值放电电流等于持续放电电流
            temporaryCurrentLimits.peakDischargeCurrent_mA = temporaryCurrentLimits.continuousDischargeCurrent_mA;
        } else {
            // 情况3：最高温度低于"高温放电截止温度"（温度适宜，可以满功率放电）
            /* 不执行任何操作，因为这种情况已由 minimumCellTemperature_ddegC 处理 */
            // 注：这里将临时变量设为最大值，后续会通过取最小值逻辑保留低温限制的结果
            temporaryCurrentLimits.continuousDischargeCurrent_mA = pConfigLimitValues->maximumDischargeCurrent_mA;
            temporaryCurrentLimits.peakDischargeCurrent_mA       = pConfigLimitValues->maximumDischargeCurrent_mA;
        }
        /* 最低单体温度的降额值已经计算，结果保存在 pAllowedTemperatureBasedCurrentCheck 中。
           现在检查新计算的最高单体温度降额值是否小于先前计算的值 */
        // 取最小值逻辑：最终允许的电流 = min(基于最低温度的电流, 基于最高温度的电流)
        pAllowedTemperatureBasedCurrent->continuousDischargeCurrent_mA = MATH_MinimumOfTwoFloats(
            pAllowedTemperatureBasedCurrent->continuousDischargeCurrent_mA,
            temporaryCurrentLimits.continuousDischargeCurrent_mA);
        pAllowedTemperatureBasedCurrent->peakDischargeCurrent_mA = MATH_MinimumOfTwoFloats(
            pAllowedTemperatureBasedCurrent->peakDischargeCurrent_mA, temporaryCurrentLimits.peakDischargeCurrent_mA);
    }

    /* ==================== 高温充电计算 ==================== */
    // 逻辑分三段：1. 高于上限极限(禁止充电)；2. 介于上限极限与截止上限之间(线性降额)；3. 低于截止上限(满额充电)
    // 注意：这里需要与低温充电的结果取最小值，因为充电受限于最低温度和最高温度中的最严苛条件

    if (maximumCellTemperature_ddegC >= pConfigLimitValues->limitHighTemperatureCharge_ddegC) {
        // 情况1：最高温度高于或等于"高温充电极限温度"（极热状态）
        // 此时禁止充电，防止电池热失控
        pAllowedTemperatureBasedCurrent->continuousChargeCurrent_mA = 0.0f;
        pAllowedTemperatureBasedCurrent->peakChargeCurrent_mA       = 0.0f;
    } else {
        // 最高温度低于高温充电极限温度
        if (maximumCellTemperature_ddegC >= pConfigLimitValues->cutoffHighTemperatureCharge_ddegC) {
            // 情况2：最高温度介于"高温充电极限温度"和"高温充电截止温度"之间（温度偏高，需要降额充电）
            // 使用线性方程计算：斜率 * 温度 + 截距，电流随温度降低而线性增加
            temporaryCurrentLimits.continuousChargeCurrent_mA =
                (pCalculatedSofCurves->slopeHighTemperatureCharge * maximumCellTemperature_ddegC) +
                pCalculatedSofCurves->offsetHighTemperatureCharge;
            // 在此降额区间，峰值充电电流等于持续充电电流
            temporaryCurrentLimits.peakChargeCurrent_mA = temporaryCurrentLimits.continuousChargeCurrent_mA;
        } else {
            // 情况3：最高温度低于"高温充电截止温度"（温度适宜，可以满功率充电）
            /* 不执行任何操作，因为这种情况已由 minimumCellTemperature_ddegC 处理 */
            // 注：这里将临时变量设为最大值，后续会通过取最小值逻辑保留低温限制的结果
            temporaryCurrentLimits.continuousChargeCurrent_mA = pConfigLimitValues->maximumChargeCurrent_mA;
            temporaryCurrentLimits.peakChargeCurrent_mA       = pConfigLimitValues->maximumChargeCurrent_mA;
        }
        /* 最低单体温度的降额值已经计算，结果保存在 pAllowedTemperatureBasedCurrentCheck 中。
           现在检查新计算的最高单体温度降额值是否小于先前计算的值 */
        // 取最小值逻辑：最终允许的电流 = min(基于最低温度的电流, 基于最高温度的电流)
        pAllowedTemperatureBasedCurrent->continuousChargeCurrent_mA = MATH_MinimumOfTwoFloats(
            pAllowedTemperatureBasedCurrent->continuousChargeCurrent_mA,
            temporaryCurrentLimits.continuousChargeCurrent_mA);
        pAllowedTemperatureBasedCurrent->peakChargeCurrent_mA = MATH_MinimumOfTwoFloats(
            pAllowedTemperatureBasedCurrent->peakChargeCurrent_mA, temporaryCurrentLimits.peakChargeCurrent_mA);
    }
}


s/**
 * @brief 比较基于电压和基于温度的两组SOF电流限制值，取各自对应项的最小值合并
 * @param voltageBasedLimits 基于电压计算得出的电流限制值
 * @param temperatureBasedLimits 基于温度计算得出的电流限制值
 * @return SOF_CURRENT_LIMITS_s 合并后的最终电流限制值（各项取两者中的较小值）
 */
static SOF_CURRENT_LIMITS_s SOF_MinimumOfTwoSofValues(
    SOF_CURRENT_LIMITS_s voltageBasedLimits,      // 基于电压的电流限制结构体（值传递）
    SOF_CURRENT_LIMITS_s temperatureBasedLimits) { // 基于温度的电流限制结构体（值传递）

    /* AXIVION 常规 Generic-MissingParameterAssert: voltageBasedLimits: 参数接受整个范围 */
    /* AXIVION 常规 Generic-MissingParameterAssert: temperatureBasedLimits: 参数接受整个范围 */
    // 注：AXIVION是静态代码分析工具，这两行注释是为了告诉工具不需要对这两个结构体参数做范围断言，
    // 因为它们内部的成员变量合法地占用了数据类型的全量程范围。

    /* 初始化返回值结构体，将所有成员清零 */
    SOF_CURRENT_LIMITS_s retval       = {0};

    /* 计算持续充电电流：取电压限制和温度限制中的较小值 */
    retval.continuousChargeCurrent_mA = MATH_MinimumOfTwoFloats(
        voltageBasedLimits.continuousChargeCurrent_mA, temperatureBasedLimits.continuousChargeCurrent_mA);

    /* 计算峰值充电电流：取电压限制和温度限制中的较小值 */
    retval.peakChargeCurrent_mA =
        MATH_MinimumOfTwoFloats(voltageBasedLimits.peakChargeCurrent_mA, temperatureBasedLimits.peakChargeCurrent_mA);

    /* 计算持续放电电流：取电压限制和温度限制中的较小值 */
    retval.continuousDischargeCurrent_mA = MATH_MinimumOfTwoFloats(
        voltageBasedLimits.continuousDischargeCurrent_mA, temperatureBasedLimits.continuousDischargeCurrent_mA);

    /* 计算峰值放电电流：取电压限制和温度限制中的较小值 */
    retval.peakDischargeCurrent_mA = MATH_MinimumOfTwoFloats(
        voltageBasedLimits.peakDischargeCurrent_mA, temperatureBasedLimits.peakDischargeCurrent_mA);

    /* 返回合并后的最终安全电流限制值 */
    return retval;
}


/*========== 外部函数实现 ================================*/
extern void SOF_Init(void) {
    /* 计算推荐工作电流的 SOF 曲线 */
    SOF_CalculateCurves(&sof_recommendedCurrent, &sof_curveRecommendedOperatingCurrent);
}

/**
 * @brief SOF计算的主函数，计算整个电池包基于电压和温度的推荐充放电电流限制
 */
extern void SOF_Calculation(void) {
    /* 定义局部变量，用于存储单串合并后的最终允许电流，初始化为0 */
    SOF_CURRENT_LIMITS_s allowedCurrent = {0};

    /* 从数据库读取当前各串的最小/最大单体电压和最小/最大温度值 */
    DATA_READ_DATA(&sof_tableMinimumMaximumValues);

    /* 重置允许的电池包级别的电流值，清零以防残留旧数据 */
    sof_tableSofValues.recommendedContinuousPackChargeCurrent_mA    = 0.0f;
    sof_tableSofValues.recommendedContinuousPackDischargeCurrent_mA = 0.0f;
    sof_tableSofValues.recommendedPeakPackChargeCurrent_mA          = 0.0f;
    sof_tableSofValues.recommendedPeakPackDischargeCurrent_mA       = 0.0f;

    /* 初始化统计变量：已闭合（已连接）的电池串数量 */
    uint8_t nrClosedStrings = 0;
    /* 初始化全包最小充电电流和最小放电电流，设为浮点数最大值，便于后续寻找最小值 */
    float_t minCharge_mA    = FLT_MAX;
    float_t minDischarge_mA = FLT_MAX;

    /* 遍历系统中所有的电池串 (BS_NR_OF_STRINGS 为电池串总数) */
    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
        /* 定义局部变量，分别存储基于电压和基于温度计算出的电流限制 */
        SOF_CURRENT_LIMITS_s voltageBasedSof     = {0};
        SOF_CURRENT_LIMITS_s temperatureBasedSof = {0};

        /* 判断当前电池串是否已闭合（接触器闭合，接入电路） */
        if (BMS_IsStringClosed(s) == true) {
            /* --- 串已连接，计算该串的允许电流 --- */

            /* 1. 计算基于电压的SOF电流限制 */
            SOF_CalculateVoltageBasedCurrentLimit(
                (float_t)sof_tableMinimumMaximumValues.minimumCellVoltage_mV[s],      // 该串最低单体电压
                (float_t)sof_tableMinimumMaximumValues.maximumCellVoltage_mV[s],      // 该串最高单体电压
                &voltageBasedSof,                                                     // 输出：电压限制结果
                &sof_recommendedCurrent,                                              // 配置参数
                &sof_curveRecommendedOperatingCurrent);                               // 曲线参数

            /* 2. 计算基于温度的SOF电流限制 */
            SOF_CalculateTemperatureBasedCurrentLimit(
                (float_t)sof_tableMinimumMaximumValues.minimumTemperature_ddegC[s],  // 该串最低温度
                (float_t)sof_tableMinimumMaximumValues.maximumTemperature_ddegC[s],  // 该串最高温度
                &temperatureBasedSof,                                                 // 输出：温度限制结果
                &sof_recommendedCurrent,                                              // 配置参数
                &sof_curveRecommendedOperatingCurrent);                               // 曲线参数

            /* 3. 综合电压和温度的限制，取两者中的较小值作为该串的最终允许电流 */
            allowedCurrent = SOF_MinimumOfTwoSofValues(voltageBasedSof, temperatureBasedSof);

            /* 4. 将该串的计算结果写入全局SOF表中对应的位置 */
            sof_tableSofValues.recommendedContinuousChargeCurrent_mA[s] = allowedCurrent.continuousChargeCurrent_mA;
            sof_tableSofValues.recommendedContinuousDischargeCurrent_mA[s] =
                allowedCurrent.continuousDischargeCurrent_mA;
            sof_tableSofValues.recommendedPeakChargeCurrent_mA[s]    = allowedCurrent.peakChargeCurrent_mA;
            sof_tableSofValues.recommendedPeakDischargeCurrent_mA[s] = allowedCurrent.peakDischargeCurrent_mA;

            /* 5. 累加已闭合的电池串数量 */
            nrClosedStrings++;

            /* 6. 寻找所有串中最小的持续充电电流（木桶效应：最弱的串决定整个包的充电能力） */
            if (minCharge_mA > sof_tableSofValues.recommendedContinuousChargeCurrent_mA[s]) {
                minCharge_mA = sof_tableSofValues.recommendedContinuousChargeCurrent_mA[s];
            }
            /* 7. 寻找所有串中最小的持续放电电流（木桶效应：最弱的串决定整个包的放电能力） */
            if (minDischarge_mA > sof_tableSofValues.recommendedContinuousDischargeCurrent_mA[s]) {
                minDischarge_mA = sof_tableSofValues.recommendedContinuousDischargeCurrent_mA[s];
            }
        } else {
            /* --- 串未连接，将该串的允许电流全部置0 --- */
            sof_tableSofValues.recommendedContinuousChargeCurrent_mA[s]    = 0.0f;
            sof_tableSofValues.recommendedContinuousDischargeCurrent_mA[s] = 0.0f;
            sof_tableSofValues.recommendedPeakChargeCurrent_mA[s]          = 0.0f;
            sof_tableSofValues.recommendedPeakDischargeCurrent_mA[s]       = 0.0f;
        }
    }

    /* 限幅处理：即使计算出的单串最小电流超过了硬件允许的最大物理电流，也要被硬件上限截断 */
    if (minCharge_mA > (float_t)BS_MAXIMUM_STRING_CURRENT_mA) {
        minCharge_mA = (float_t)BS_MAXIMUM_STRING_CURRENT_mA;
    }
    if (minDischarge_mA > (float_t)BS_MAXIMUM_STRING_CURRENT_mA) {
        minDischarge_mA = (float_t)BS_MAXIMUM_STRING_CURRENT_mA;
    }

    /* 计算推荐的电池包级别的电流值：
     * 整个包的允许电流 = 单串最小允许电流 × 已闭合的串数 (假设各串并联，电流叠加) */
    sof_tableSofValues.recommendedContinuousPackChargeCurrent_mA    = (float_t)nrClosedStrings * minCharge_mA;
    sof_tableSofValues.recommendedContinuousPackDischargeCurrent_mA = (float_t)nrClosedStrings * minDischarge_mA;
    /* 注：此处峰值电流也使用了minCharge_mA/minDischarge_mA，说明在此逻辑中峰值和持续限制被拉齐了 */
    sof_tableSofValues.recommendedPeakPackChargeCurrent_mA          = (float_t)nrClosedStrings * minCharge_mA;
    sof_tableSofValues.recommendedPeakPackDischargeCurrent_mA       = (float_t)nrClosedStrings * minDischarge_mA;

    /* 安全拦截：检查当前 BMS 状态机是否正在向 ERROR 状态转换。
     * 如果是，说明系统发生了严重故障，必须立刻将允许的包级电流强制设置为 0，切断充放电功率 */
    if (BMS_IsTransitionToErrorStateActive() == true) {
        sof_tableSofValues.recommendedContinuousPackChargeCurrent_mA    = 0.0f;
        sof_tableSofValues.recommendedContinuousPackDischargeCurrent_mA = 0.0f;
        sof_tableSofValues.recommendedPeakPackChargeCurrent_mA          = 0.0f;
        sof_tableSofValues.recommendedPeakPackDischargeCurrent_mA       = 0.0f;
    }

    /* 将最终计算好的SOF数据写回数据库，供其他控制模块（如接触器控制、均衡控制等）使用 */
    DATA_WRITE_DATA(&sof_tableSofValues);
}


/*========== 外部化的静态函数实现 (单元测试) =======*/
#ifdef UNITY_UNIT_TEST
extern void TEST_SOF_CalculateCurves(const SOF_CONFIG_s *pConfigurationValues, SOF_CURVE_s *pCalculatedSofCurveValues) {
    SOF_CalculateCurves(pConfigurationValues, pCalculatedSofCurveValues);
}
extern void TEST_SOF_CalculateVoltageBasedCurrentLimit(
    int16_t minimumCellVoltage_mV,
    int16_t maximumCellVoltage_mV,
    SOF_CURRENT_LIMITS_s *pAllowedVoltageBasedCurrent,
    const SOF_CONFIG_s *pConfigLimitValues,
    SOF_CURVE_s *pCalculatedSofCurves) {
    SOF_CalculateVoltageBasedCurrentLimit(
        minimumCellVoltage_mV,
        maximumCellVoltage_mV,
        pAllowedVoltageBasedCurrent,
        pConfigLimitValues,
        pCalculatedSofCurves);
}
extern void TEST_SOF_CalculateTemperatureBasedCurrentLimit(
    int16_t minimumCellTemperature_ddegC,
    int16_t maximumCellTemperature_ddegC,
    SOF_CURRENT_LIMITS_s *pAllowedTemperatureBasedCurrent,
    const SOF_CONFIG_s *pConfigLimitValues,
    SOF_CURVE_s *pCalculatedSofCurves) {
    SOF_CalculateTemperatureBasedCurrentLimit(
        minimumCellTemperature_ddegC,
        maximumCellTemperature_ddegC,
        pAllowedTemperatureBasedCurrent,
        pConfigLimitValues,
        pCalculatedSofCurves);
}
extern SOF_CURRENT_LIMITS_s TEST_SOF_MinimumOfTwoSofValues(
    SOF_CURRENT_LIMITS_s voltageBasedLimits,
    SOF_CURRENT_LIMITS_s temperatureBasedLimits) {
    return SOF_MinimumOfTwoSofValues(voltageBasedLimits, temperatureBasedLimits);
}
#endif

