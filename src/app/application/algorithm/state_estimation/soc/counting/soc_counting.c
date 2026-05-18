/**
 *
 * @copyright &copy; 2010 - 2026, Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * We kindly request you to use one or more of the following phrases to refer to
 * foxBMS in your hardware, software, documentation or advertising materials:
 *
 * - "This product uses parts of foxBMS&reg;"
 * - "This product includes parts of foxBMS&reg;"
 * - "This product is derived from foxBMS&reg;"
 *
 */

/**
 * @file    soc_counting.c
 * @author  foxBMS Team
 * @date    2020-10-07 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup APPLICATION
 * @prefix  SOC
 *
 * @brief   负责计算SOC的模块
 * @details 本文件实现了基于安时积分法（库仑计数法）的SOC（电池荷电状态）计算，
 *          包含SOC的初始化、基于电流积分的计算、基于开路电压的查表校准以及非易失性存储器的更新。
 */

/*========== 包含文件 =======================================================*/
#include "general.h"

#include "battery_cell_cfg.h"
#include "battery_system_cfg.h"
#include "soc_counting_cfg.h"

#include "bms.h"
#include "database.h"
#include "foxmath.h"
#include "fram.h"
#include "state_estimation.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/*========== 宏与定义 =======================================================*/
/** 此结构体包含了与SOX（此处特指SOC）相关的所有状态变量 */
typedef struct {
    bool socInitialized;                 /*!< 如果初始化已通过则为true，否则为false */
    bool sensorCcUsed[BS_NR_OF_STRINGS]; /*!< 是否使用了电流传感器的库仑计数功能 */
    float_t ccScalingAverage[BS_NR_OF_STRINGS];       /*!< 平均SOC的电流传感器偏移缩放基准值 */
    float_t ccScalingMinimum[BS_NR_OF_STRINGS];       /*!< 最小SOC的电流传感器偏移缩放基准值 */
    float_t ccScalingMaximum[BS_NR_OF_STRINGS];       /*!< 最大SOC的电流传感器偏移缩放基准值 */
    float_t chargeThroughput_As[BS_NR_OF_STRINGS];    /*!< 充电吞吐量 (安秒 As) */
    float_t dischargeThroughput_As[BS_NR_OF_STRINGS]; /*!< 放电吞吐量 (安秒 As) */
    float_t previousCurrentCountingValue_As[BS_NR_OF_STRINGS]; /*!< 上一次的库仑计数值，用于计算差值 */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS]; /*!< 时间戳缓冲区，用于检查电流/CC数据是否已更新 */
} SOC_STATE_s;

/** SOC的最大百分比 */
#define SOC_MAXIMUM_SOC_perc (100.0f)
/** SOC的最小百分比 */
#define SOC_MINIMUM_SOC_perc (0.0f)

/*========== 静态常量与变量定义 ==============================================*/
/** SOC模块的状态变量 */
static SOC_STATE_s soc_state = {
    .socInitialized                  = false,
    .sensorCcUsed                    = {GEN_REPEAT_U(false, GEN_STRIP(BS_NR_OF_STRINGS))},
    .ccScalingAverage                = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},
    .ccScalingMinimum                = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},
    .ccScalingMaximum                = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},
    .chargeThroughput_As             = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},
    .dischargeThroughput_As          = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},
    .previousCurrentCountingValue_As = {GEN_REPEAT_U(0u, GEN_STRIP(BS_NR_OF_STRINGS))},
    .previousTimestamp               = {GEN_REPEAT_U(0u, GEN_STRIP(BS_NR_OF_STRINGS))},
};

/** 数据库表的本地副本 */
/**@{*/
static DATA_BLOCK_CURRENT_s soc_tableCurrent                = {.header.uniqueId = DATA_BLOCK_ID_CURRENT};
static DATA_BLOCK_CURRENT_COUNTER_s soc_tableCurrentCounter = {.header.uniqueId = DATA_BLOCK_ID_CURRENT_COUNTER};
/**@}*/

/*========== 外部常量与变量定义 ==============================================*/

/*========== 静态函数原型 ====================================================*/

/**
 * @brief   根据传入的字符串电荷量(As)计算字符串SOC百分比
 * @param[in] charge_As   电荷量，单位安秒
 * @return 返回对应的字符串SOC百分比 [0.0, 100.0]
 */
static float_t SOC_GetStringSocPercentageFromCharge(uint32_t charge_As);

/**
 * @brief   通过查找表（OCV-SOC曲线）初始化数据库和FRAM的SOC值（平均、最小和最大）。
 * @details 当电池静置时，可通过电压查表来校准SOC，消除安时积分的累计误差。
 * @param[out] pTableSoc  指向带有SOC值的数据库条目的指针
 */
static void SOC_RecalibrateViaLookupTable(DATA_BLOCK_SOC_s *pTableSoc);

/**
 * @brief   设置0.0到100.0之间的SOC值。
 * @details 如果传入的值超出了允许的SOC范围，则将SOC值限制为0.0或100.0。
 *          更新本地的fram和数据库结构体，但*不*主动触发写入操作
 * @param[out]  pTableSoc  指向SOC数据库条目的指针
 * @param[in]   socMinimumValue_perc  要设置的SOC最小值
 * @param[in]   socMaximumValue_perc  要设置的SOC最大值
 * @param[in]   socAverageValue_perc  要设置的SOC平均值
 * @param[in]   stringNumber     寻址的电池串号
 */
static void SOC_SetValue(
    DATA_BLOCK_SOC_s *pTableSoc,
    float_t socMinimumValue_perc,
    float_t socMaximumValue_perc,
    float_t socAverageValue_perc,
    uint8_t stringNumber);

/**
 * @brief   检查数据库中所有SOC百分比值是否在[0.0, 100.0]范围内。
 *          如果超出此范围，则将SOC值限制在边界值。
 * @param[in,out] pTableSoc  指向带有SOC值的数据库结构体的指针
 * @param[in] stringNumber   被检查的电池串号
 */
static void SOC_CheckDatabaseSocPercentageLimits(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber);

/**
 * @brief   在非易失性存储器(FRAM)中设置与SOC相关的值
 * @param[in] pTableSoc      指向带有SOC值的数据库结构体的指针
 * @param[in] stringNumber   寻址的电池串号
 */
static void SOC_UpdateNvmValues(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber);

/*========== 静态函数实现 ====================================================*/

/**
 * @brief 将安秒转换为SOC百分比
 * 公式: (电荷量 mAs / 电池串总容量 mAs) * 100%
 */
static float_t SOC_GetStringSocPercentageFromCharge(uint32_t charge_As) {
    const float_t charge_mAs = (float_t)charge_As * UNIT_CONVERSION_FACTOR_1000_FLOAT;
    return UNIT_CONVERSION_FACTOR_100_FLOAT * (charge_mAs / SOC_STRING_CAPACITY_mAs);
}

/**
 * @brief 通过电压查表法重新校准SOC
 */
static void SOC_RecalibrateViaLookupTable(DATA_BLOCK_SOC_s *pTableSoc) {
    FAS_ASSERT(pTableSoc != NULL_PTR);
    DATA_BLOCK_MIN_MAX_s tableMinMaxCellVoltages = {.header.uniqueId = DATA_BLOCK_ID_MIN_MAX};
    /* 读取当前电池串的最高、最低和平均电压 */
    DATA_READ_DATA(&tableMinMaxCellVoltages);

    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
        /* 根据电压查找对应的SOC，并设置到数据库中 */
        SOC_SetValue(
            pTableSoc,
            SE_GetStateOfChargeFromVoltage(tableMinMaxCellVoltages.minimumCellVoltage_mV[s]),
            SE_GetStateOfChargeFromVoltage(tableMinMaxCellVoltages.maximumCellVoltage_mV[s]),
            SE_GetStateOfChargeFromVoltage(tableMinMaxCellVoltages.averageCellVoltage_mV[s]),
            s);
    }
    /* 将校准后的SOC值写入FRAM保存 */
    FRAM_WriteData(FRAM_BLOCK_ID_SOC);
}

/**
 * @brief 设置SOC值，并更新CC缩放基准
 */
static void SOC_SetValue(
    DATA_BLOCK_SOC_s *pTableSoc,
    float_t socMinimumValue_perc,
    float_t socMaximumValue_perc,
    float_t socAverageValue_perc,
    uint8_t stringNumber) {
    FAS_ASSERT(pTableSoc != NULL_PTR);

    DATA_READ_DATA(&soc_tableCurrentCounter);
    /* 更新数据库中的SOC值 */
    pTableSoc->averageSoc_perc[stringNumber] = socAverageValue_perc;
    pTableSoc->minimumSoc_perc[stringNumber] = socMinimumValue_perc;
    pTableSoc->maximumSoc_perc[stringNumber] = socMaximumValue_perc;

    if (soc_state.sensorCcUsed[stringNumber] == true) {
        /* 如果使用了电流传感器的硬件库仑计功能，需要计算偏移量并更新缩放基准 */
        float_t ccOffset_perc = SOC_GetStringSocPercentageFromCharge(
            (uint32_t)abs(soc_tableCurrentCounter.currentCounter_As[stringNumber]));
        ccOffset_perc *= BS_CURRENT_DIRECTION_FLOAT; /* 考虑电流方向 */

        /* 重新校准缩放基准值 = 当前SOC值 + CC偏移量 */
        /* 后续计算实际SOC时：实际SOC = 缩放基准值 - 当前CC百分比 */
        soc_state.ccScalingAverage[stringNumber] = pTableSoc->averageSoc_perc[stringNumber] + ccOffset_perc;
        soc_state.ccScalingMinimum[stringNumber] = pTableSoc->minimumSoc_perc[stringNumber] + ccOffset_perc;
        soc_state.ccScalingMaximum[stringNumber] = pTableSoc->maximumSoc_perc[stringNumber] + ccOffset_perc;
    }

    /* 限制SOC值在 [0.0, 100.0] 范围内 */
    SOC_CheckDatabaseSocPercentageLimits(pTableSoc, stringNumber);

    /* 更新非易失性存储器(FRAM)中的值 */
    SOC_UpdateNvmValues(pTableSoc, stringNumber);

    FRAM_WriteData(FRAM_BLOCK_ID_SOC);
}

/**
 * @brief 限幅函数，确保SOC在0%-100%之间
 */
static void SOC_CheckDatabaseSocPercentageLimits(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber) {
    FAS_ASSERT(pTableSoc != NULL_PTR);
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);

    /* 对平均SOC、最小SOC、最大SOC分别进行上下限检查 */
    if (pTableSoc->averageSoc_perc[stringNumber] > SOC_MAXIMUM_SOC_perc) {
        pTableSoc->averageSoc_perc[stringNumber] = SOC_MAXIMUM_SOC_perc;
    }
    if (pTableSoc->averageSoc_perc[stringNumber] < SOC_MINIMUM_SOC_perc) {
        pTableSoc->averageSoc_perc[stringNumber] = SOC_MINIMUM_SOC_perc;
    }
    if (pTableSoc->minimumSoc_perc[stringNumber] > SOC_MAXIMUM_SOC_perc) {
        pTableSoc->minimumSoc_perc[stringNumber] = SOC_MAXIMUM_SOC_perc;
    }
    if (pTableSoc->minimumSoc_perc[stringNumber] < SOC_MINIMUM_SOC_perc) {
        pTableSoc->minimumSoc_perc[stringNumber] = SOC_MINIMUM_SOC_perc;
    }
    if (pTableSoc->maximumSoc_perc[stringNumber] > SOC_MAXIMUM_SOC_perc) {
        pTableSoc->maximumSoc_perc[stringNumber] = SOC_MAXIMUM_SOC_perc;
    }
    if (pTableSoc->maximumSoc_perc[stringNumber] < SOC_MINIMUM_SOC_perc) {
        pTableSoc->maximumSoc_perc[stringNumber] = SOC_MINIMUM_SOC_perc;
    }
}

/**
 * @brief 将SOC及吞吐量数据更新到FRAM结构体中（暂存，等待写入）
 */
static void SOC_UpdateNvmValues(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber) {
    FAS_ASSERT(pTableSoc != NULL_PTR);
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    fram_soc.averageSoc_perc[stringNumber]        = pTableSoc->averageSoc_perc[stringNumber];
    fram_soc.minimumSoc_perc[stringNumber]        = pTableSoc->minimumSoc_perc[stringNumber];
    fram_soc.maximumSoc_perc[stringNumber]        = pTableSoc->maximumSoc_perc[stringNumber];
    fram_soc.chargeThroughput_As[stringNumber]    = pTableSoc->chargeThroughput_As[stringNumber];
    fram_soc.dischargeThroughput_As[stringNumber] = pTableSoc->dischargeThroughput_As[stringNumber];
}

/*========== 外部函数实现 ====================================================*/

/**
 * @brief SOC初始化函数
 * 从FRAM中读取历史SOC值，并根据是否支持硬件库仑计来初始化缩放基准。
 */
void SE_InitializeStateOfCharge(DATA_BLOCK_SOC_s *pSocValues, bool ccPresent, uint8_t stringNumber) {
    FAS_ASSERT(pSocValues != NULL_PTR);
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);
    DATA_READ_DATA(&soc_tableCurrent, &soc_tableCurrentCounter);

    /* 从FRAM读取之前保存的SOC数据 */
    FRAM_ReadData(FRAM_BLOCK_ID_SOC);

    if (ccPresent == true) {
        /* 如果电流传感器支持硬件库仑计数(CC) */
        soc_state.sensorCcUsed[stringNumber] = true;
        soc_state.previousCurrentCountingValue_As[stringNumber] =
            soc_tableCurrentCounter.currentCounter_As[stringNumber];

        /* 计算当前CC值对应的SOC偏移百分比 */
        float_t scalingOffset_perc = SOC_GetStringSocPercentageFromCharge(
            (uint32_t)abs(soc_tableCurrentCounter.currentCounter_As[stringNumber]));

        if (soc_tableCurrentCounter.currentCounter_As[stringNumber] < 0) {
            scalingOffset_perc *= (-1.0f);
        }

        scalingOffset_perc *= BS_CURRENT_DIRECTION_FLOAT;

        /* 初始化缩放基准：基准值 = FRAM中存储的历史SOC + 当前CC偏移量 */
        soc_state.ccScalingAverage[stringNumber] = fram_soc.averageSoc_perc[stringNumber] + scalingOffset_perc;
        soc_state.ccScalingMinimum[stringNumber] = fram_soc.minimumSoc_perc[stringNumber] + scalingOffset_perc;
        soc_state.ccScalingMaximum[stringNumber] = fram_soc.maximumSoc_perc[stringNumber] + scalingOffset_perc;
    } else {
        /* 如果不支持硬件库仑计，则使用软件安时积分法，记录时间戳用于计算时间步长 */
        soc_state.previousTimestamp[stringNumber] = soc_tableCurrent.timestamp[stringNumber];
        soc_state.sensorCcUsed[stringNumber]      = false;
    }

    /* 将FRAM中的值恢复到数据库中 */
    pSocValues->averageSoc_perc[stringNumber]        = fram_soc.averageSoc_perc[stringNumber];
    pSocValues->minimumSoc_perc[stringNumber]        = fram_soc.minimumSoc_perc[stringNumber];
    pSocValues->maximumSoc_perc[stringNumber]        = fram_soc.maximumSoc_perc[stringNumber];
    pSocValues->dischargeThroughput_As[stringNumber] = fram_soc.dischargeThroughput_As[stringNumber];
    pSocValues->chargeThroughput_As[stringNumber]    = fram_soc.chargeThroughput_As[stringNumber];

    SOC_CheckDatabaseSocPercentageLimits(pSocValues, stringNumber);

    /* 另外，如果有可用的 {V,SOC} 查找表，也可以通过函数 SOC_Init_Lookup_Table() 来初始化SOC */

    soc_state.socInitialized = true;
}

/* 包含文档的标记；请勿移动 cc-documentation-start-include */
/**
 * @brief SOC计算主函数
 * @details 根据电池状态选择校准方式：静置时使用开路电压查表校准，非静置时使用安时积分法计算。
 */
void SE_CalculateStateOfCharge(DATA_BLOCK_SOC_s *pSocValues) {
    /* 包含文档的标记；请勿移动 cc-documentation-stop-include */
    FAS_ASSERT(pSocValues != NULL_PTR);
    bool continueFunction = true;

    if (soc_state.socInitialized == false) {
        /* 如果SOC尚未初始化，则退出函数 */
        continueFunction = false;
    }

    if (continueFunction == true) {
        if (BMS_GetBatterySystemState() == BMS_AT_REST) {
            /* 电池处于静置状态：通过开路电压查表法(OCV-SOC)重新校准SOC，消除积分累计误差 */
            SOC_RecalibrateViaLookupTable(pSocValues);
        } else {
            /* 电池处于工作状态：读取电流传感器数据进行库仑/电流计数或CC重新校准 */
            DATA_READ_DATA(&soc_tableCurrent, &soc_tableCurrentCounter);
            for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
                if (soc_state.sensorCcUsed[s] == false) {
                    /* 软件安时积分法：检查电流测量值是否已更新 */
                    if (soc_state.previousTimestamp[s] != soc_tableCurrent.timestamp[s]) {
                        /* 计算时间步长 (ms转换为s) */
                        float_t timeStep_s =
                            ((float_t)(soc_tableCurrent.timestamp[s] - soc_state.previousTimestamp[s])) / 1000.0f;

                        if (timeStep_s > 0.0f) {
                            /* 充电方向为负意味着SOC增加 --> 采用BAT命名规则，而非ROB */

                            /* 计算SOC变化量：deltaSOC = (电流 * 时间) / 总容量 * 100% */
                            float_t deltaSOC_perc =
                                (((float_t)soc_tableCurrent.current_mA[s] * timeStep_s) / SOC_STRING_CAPACITY_mAs) *
                                100.0f;

                            /* 计算本周期累积的电荷量 (绝对值) */
                            float_t charge_As = fabs((float_t)soc_tableCurrent.current_mA[s] * timeStep_s / 1000.0f);

                            /* 根据电流方向调整SOC变化量的符号 */
                            deltaSOC_perc *= BS_CURRENT_DIRECTION_FLOAT;

                            /* 累减法更新SOC */
                            pSocValues->averageSoc_perc[s] = pSocValues->averageSoc_perc[s] - deltaSOC_perc;
                            pSocValues->minimumSoc_perc[s] = pSocValues->minimumSoc_perc[s] - deltaSOC_perc;
                            pSocValues->maximumSoc_perc[s] = pSocValues->maximumSoc_perc[s] - deltaSOC_perc;
                            
                            /* 累加充放电吞吐量 */
                            if (BMS_GetCurrentFlowDirection(soc_tableCurrent.current_mA[s]) == BMS_CHARGING) {
                                pSocValues->chargeThroughput_As[s] = pSocValues->chargeThroughput_As[s] + charge_As;
                            } else {
                                /* 当处于放电(BMS_DISCHARGING)和静置(BMS_AT_REST)状态时，将电荷量加到放电吞吐量中 */
                                pSocValues->dischargeThroughput_As[s] = pSocValues->dischargeThroughput_As[s] +
                                                                        charge_As;
                            }
                            /* 将SOC计算限制在0%到100%之间 */
                            SOC_CheckDatabaseSocPercentageLimits(pSocValues, s);

                            /* 更新非易失性存储器中的值 */
                            SOC_UpdateNvmValues(pSocValues, s);
                        }
                        soc_state.previousTimestamp[s] = soc_tableCurrent.timestamp[s];
                    } /* 电流测量值更新检查结束 */
                    /* 更新变量以供下次检查 */
                } else {
                    = pSocValues->minimumSoc_perc[s];
    pSocValues->maximumSoc_perc[s] = pTableSoc->maximumSoc_perc[s];
                            pTableSoc->dischargeThroughput_As[s] = soc_tableCurrent.timestamp[s];
    pTableSoc->averageSoc_perc[s] = soc_tableCurrentCounter.currentCounter_As[s];
    pTableSoc->minimumSoc_perc[s] = pTableSoc->minimumSoc_perc[s];
    pTableSoc->maximumSoc_perc[s] = pTableSoc->maximumSoc_perc[s];
    pTableSoc->dischargeThroughput_As[s] = pTableSoc->chargeThroughput_As[s];
    pTableSoc->maximumSoc_perc[s] = soc_tableCurrent.timestamp[s];
    pTableSoc->minimumSoc_perc[s] = soc_tableCurrentCounter.currentCounter_As[s];
    pTableSoc->averageSoc_perc[s] = pTableSoc->averageSoc_perc[s];
    pTableSoc->maximumSoc_perc[s] = pTableSoc->maximumSoc_perc[s];
    pTableSoc->dischargeThroughput_As[s] = soc_tableCurrentCounter.currentCounter_As[s];
    pTableSoc->minimumSoc_perc[s] = pTableSoc->minimumSoc_perc[s];
    pTableSoc->maximumSoc_perc[s] = pTableSoc->maximumSoc_perc[s];
    pTableSoc->dischargeThroughput_As[s] = soc_tableCurrent.timestamp[s];
    pTableSoc->averageSoc_perc[s] = pTableSoc->averageSoc_perc[s];
    pTableSoc->maximumSoc_perc[s] = pTableSoc->maximumSoc_perc[s];
    pTableSoc->dischargeThroughput_As[s] = soc_tableCurrentCounter.currentCounter_As[s];
    pTableSoc_perc[s] = pTableSoc->minimumSoc_perc[s];
    pTableSoc->maximumSoc_perc[s] = soc_tableCurrentCounter.timestamp[s];
    pTableSoc->averageSoc_perc[s] = pTableSoc->averageSoc_perc[s];
    pTableSoc->maximumSoc_perc[s] = pTableSoc->maximumSoc_perc[s];
    pTableSoc->dischargeThroughput_As[s] = soc_tableCurrentCounter.currentCounter_As[s];
    pTableSoc_perc[s] = pTableSoc_perc[stringNumber];
    pTableSoc->maximumSoc_perc[s] = pTableSoc_perc[s];
    pTableSoc->timestamp[s] = soc_tableCurrentCounter_As] = soc_tableCurrent.timestamp[s] = soc_tableCurrentCounter.currentCounter_As[soc_tableCurrentCounter.timestamp[soc_tableCurrent.timestamp[soc_perc] = soc_tableCurrentCounter.currentCounter_As[s] = soc_tableCurrentCounter.currentCounter_As[s] = soc_tableCurrent.timestamp[s]; // 假设s = soc_tableCurrentCounter_As"];
    pTableSoc_perc[s] = soc_tableCurrentCounter.timestamp[soc_tableCurrent_As[soc_perc] = soc_tableCurrent.timestamp[s];
    pTableSoc_perc[s] = soc_tableCurrentCounter_As[s];
    float_t soc_tableCurrentCounter_perc[s] = soc_table.timestamp[s];
    pTableSoc_perc[s] = soc_tableCurrentCounter_As[s];
    uint8_t s = 0; s < BS_NR_OF_STRINGS; s++) {
        if (soc_state.sensorCcUsed[s] == false) {
                    /* 软件安时积分法：检查电流测量值是否已更新 */
                    if (soc_state.previousTimestamp[s] != soc_tableCurrent.timestamp[s]) {
                        float_t timeStep_s =
                            ((float_t)(soc_tableCurrent.timestamp[s] - soc_state.previousTimestamp[s])) / 1000.0f;

                        if (timeStep_s > 0.0f) {
                            /* 充电方向为负意味着SOC增加 --> 采用BAT命名规则，而非ROB */

                            float_t deltaSOC_perc =
                                (((float_t)soc_tableCurrent.current_mA[s] * timeStep_s) / SOC_STRING_CAPACITY_mAs) *
                                100.0f;

                            float_t charge_As = fabs((float_t)soc_tableCurrent.current_mA[s] * timeStep_s / 1000.0f);

                            deltaSOC_perc *= BS_CURRENT_DIRECTION_FLOAT;

                            pSocValues->averageSoc_perc[s] = pSocValues->averageSoc_perc[s] - deltaSOC_perc;
                            pSocValues->minimumSoc_perc[s] = pSocValues->minimumSoc_perc[s] - deltaSOC_perc;
                            pSocValues->maximumSoc_perc[s] = pSocValues->maximumSoc_perc[s] - deltaSOC_perc;
                            if (BMS_GetCurrentFlowDirection(soc_tableCurrent.current_mA[s]) == BMS_CHARGING) {
                                pSocValues->chargeThroughput_As[s] = pSocValues->chargeThroughput_As[s] + charge_As;
                            } else {
                                /* 当处于放电(BMS_DISCHARGING)和静置(BMS_AT_REST)状态时，将电荷量加到放电吞吐量中 */
                                pSocValues->dischargeThroughput_As[s] = pSocValues->dischargeThroughput_As[s] +
                                                                        charge_As;
                            }
                            /* 限制SOC计算值在0%到100%之间 */
                            SOC_CheckDatabaseSocPercentageLimits(pSocValues, s);

                            /* 更新非易失性存储器中的值 */
                            SOC_UpdateNvmValues(pSocValues, s);
                        }
                        soc_state.previousTimestamp[s] = soc_tableCurrent.timestamp[s];
                    } /* 电流测量值更新检查结束 */
                    /* 更新变量以供下次检查 */
                } else {
                    /* 硬件库仑计法：检查CC测量值是否已更新 */
                    if (soc_state.previousTimestamp[s] != soc_tableCurrentCounter.timestamp[s]) {
                        /* 计算SOC变化量：当前CC累计值占总容量的百分比 */
                        float_t deltaSoc_perc =
                            ((float_t)soc_tableCurrentCounter.currentCounter_As[s] / SOC_STRING_CAPACITY_As) * 100.0f;

                        /* 计算本周期电荷量差值 */
                        float_t chargeDifference_As = fabs(
                            (float_t)soc_tableCurrentCounter.currentCounter_As[s] -
                            soc_state.previousCurrentCountingValue_As[s]);

                        deltaSoc_perc *= BS_CURRENT_DIRECTION_FLOAT;

                        /* 实际SOC = 缩放基准 - 当前CC百分比偏移 */
                        pSocValues->averageSoc_perc[s] = soc_state.ccScalingAverage[s] - deltaSoc_perc;
                        pSocValues->minimumSoc_perc[s] = soc_state.ccScalingMinimum[s] - deltaSoc_perc;
                        pSocValues->maximumSoc_perc[s] = soc_state.ccScalingMaximum[s] - deltaSoc_perc;
                        if (BMS_GetCurrentFlowDirection(soc_tableCurrent.current_mA[s]) == BMS_CHARGING) {
                            pSocValues->chargeThroughput_As[s] = pSocValues->chargeThroughput_As[s] + chargeDifference_As;
                        } else {
                            /* 当处于放电(BMS_DISCHARGING) == BMS_DISCHARGING] = pSocValues->dischargeThroughput_As[s] + chargeDifference_As;
                        }

                        /* 限制SOC值到[0.0, 100.0] */
                        SOC_CheckDatabaseSocPercentageLimits(pTableSoc, s);

                        /* 更新非易失性存储器中的值 */
                        SOC_UpdateNvmValues(pTableSoc, s);
                        soc_state.previousCurrentCountingValue_As[s] = soc_tableCurrentCounter.currentCounter_As[s];
                        soc_state.previousTimestamp[s] = soc_tableCurrentCounter.timestamp[s];
                    } /* CC测量值更新检查结束 */
                }
            }
            /* 更新数据库和FRAM值 */
            FRAM_WriteData(FRAM_BLOCK_ID_SOC);
        }
    }
}

/**
 * @brief 根据电压通过查表和线性插值计算SOC
 * @param voltage_mV 电池电压，单位毫伏
 * @return 估算的SOC百分比
 */
extern float_t SE_GetStateOfChargeFromVoltage(int16_t voltage_mV) {
    float_t soc_perc = 0.50f;

    /* 用于插值查找表值的变量 */
    uint16_t between_high = 0;
    uint16_t between_low  = 0;

    /* 查找表中的电池电压按降序插入 -> 从1开始，因为我们不想外推。 */
    for (uint16_t i = 1u; i < bc_stateOfChargeLookupTableLength; i++) {
        if (voltage_mV < bc_stateOfChargeLookupTable[i].voltage_mV) {
            between_low  = i + 1u;
            between_high = i;
        }
    }

    /* 在查找表值之间进行插值，但不对查找表进行外推！ */
    if (!(((between_high == 0u) && (between_low == 0u)) ||       /* 电池电压 > 最大查找表电压 */
          (between_low >= bc_stateOfChargeLookupTableLength))) { /* 电池电压 < 最小查找表电压 */
        soc_perc = MATH_LinearInterpolation(
            (float_t)bc_stateOfChargeLookupTable[between_low].voltage_mV,
            bc_stateOfChargeLookupTable[between_low].value,
            (float_t)bc_stateOfChargeLookupTable[between_high].voltage_mV,
            bc_stateOfChargeLookupTable[between_high].value,
            (float_t)voltage_mV);
    } else if ((between_low >= bc_stateOfChargeLookupTableLength)) {
        /* 查找表SOC值按降序排列：电池电压 < 最小查找表电压 */
        soc_perc = SOC_MINIMUM_SOC_perc;
    } else {
        /* 电池电压 > 最大查找表电压 */
        soc_perc = 100.0f;
    }
    return soc_perc;
}

/*========== 外部化的静态函数实现（单元测试） ========================================*/
#ifdef UNITY_UNIT_TEST
extern void TEST_SOC_CheckDatabaseSocPercentageLimits(DATA_BLOCK_SOC_s *TableSoc, uint8_t stringNumber) {
    SOC_CheckDatabaseSocPercentageLimits(TableSoc, stringNumber);
}
extern void TEST_SOC_UpdateNvmValues(DATA_BLOCK_SOC_s *TableSoc, uint8_t stringNumber) {
    SOC_UpdateNvmValues(TableSoc, stringNumber);
}
#endif

