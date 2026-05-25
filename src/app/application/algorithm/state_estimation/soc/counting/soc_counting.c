/**
 *
 * @copyright &copy; 2010 - 2026, 弗劳恩霍夫应用研究促进协会
 * 保留所有权利。
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * 在满足以下条件的前提下，允许以源代码和二进制形式进行重新分发和使用，
 * 无论是否经过修改：
 *
 * 1. 源代码的重新分发必须保留上述版权声明、此条件列表以及下述免责声明。
 *
 * 2. 二进制形式的重新分发必须在随分发提供的文档和/或其他材料中
 * 复制上述版权声明、此条件列表以及下述免责声明。
 *
 * 3. 未经特定事先书面许可，版权持有者或其贡献者的姓名不得用于
 * 认可或推广基于本软件派生的产品。
 *
 * 本软件由版权持有者和贡献者“按原样”提供，不提供任何明示或暗示的保证，
 * 包括但不限于对适销性和特定用途适用性的暗示保证。在任何情况下，
 * 版权持有者或贡献者均不对任何直接、间接、偶然、特殊、惩罚性或后果性损害
 * （包括但不限于替代商品或服务的采购、使用、数据或利润的损失，或业务中断）
 * 承担责任，无论此类损害是基于何种责任理论，无论是合同责任、严格责任还是侵权
 * （包括疏忽或其他），即使已被告知可能发生此类损害，也是如此。
 *
 * 我们恳请您在硬件、软件、文档或广告材料中使用以下一个或多个短语来指代
 * foxBMS：
 *
 * - “本产品使用了 foxBMS&reg; 的部分内容”
 * - “本产品包含 foxBMS&reg; 的部分内容”
 * - “本产品派生自 foxBMS&reg;”
 *
 */

/**
 * @file    soc_counting.c
 * @author  foxBMS 团队
 * @date    2020-10-07 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup APPLICATION
 * @prefix  SOC
 *
 * @brief   负责计算 SOC 的 SOC 模块
 * @details 待办事项
 */

/*========== 包含文件 =======================================================*/
#include "general.h"                                  /* 包含通用系统配置和宏定义 */

#include "battery_cell_cfg.h"                         /* 包含电池单体配置参数，如容量等 */
#include "battery_system_cfg.h"                       /* 包含电池系统配置参数，如串数、电流方向等 */
#include "soc_counting_cfg.h"                         /* 包含 SOC 计数法模块的配置头文件 */

#include "bms.h"                                      /* 包含 BMS 全局定义和状态获取接口 */
#include "database.h"                                 /* 包含数据库接口，用于读写共享数据 */
#include "foxmath.h"                                  /* 包含数学工具库，如线性插值算法等 */
#include "fram.h"                                     /* 包含 FRAM(铁电存储器)驱动，用于非易失性存储 */
#include "state_estimation.h"                         /* 包含状态估计模块头文件 */

#include <math.h>                                     /* 包含标准数学库，如 fabs 绝对值函数 */
#include <stdbool.h>                                  /* 包含布尔类型支持 */
#include <stdint.h>                                   /* 包含标准整型定义 */

/*========== 宏和定义 =========================================================*/
/** 此结构体包含与 SOX 相关的所有变量 */
typedef struct {                                      /* 定义 SOC 模块的内部状态结构体 */
    bool socInitialized;                              /* 如果初始化已通过则为 true，否则为 false */
    bool sensorCcUsed[BS_NR_OF_STRINGS];              /* 布尔值，指示是否使用了来自电流传感器的库仑计数功能 */
    float_t ccScalingAverage[BS_NR_OF_STRINGS];       /* 平均 SOC 的电流传感器偏移缩放值 */
    float_t ccScalingMinimum[BS_NR_OF_STRINGS];       /* 最小 SOC 的电流传感器偏移缩放值 */
    float_t ccScalingMaximum[BS_NR_OF_STRINGS];       /* 最大 SOC 的电流传感器偏移缩放值 */
    float_t chargeThroughput_As[BS_NR_OF_STRINGS];    /* 充电吞吐量 */
    float_t dischargeThroughput_As[BS_NR_OF_STRINGS]; /* 放电吞吐量 */
    float_t previousCurrentCountingValue_As[BS_NR_OF_STRINGS]; /* 充电吞吐量(上次电流计数值) */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS];     /* 时间戳缓冲区，用于检查电流/库仑计数数据是否已更新 */
} SOC_STATE_s;                                        /* 结构体类型定义结束 */

/** 最大 SOC 百分比 */
#define SOC_MAXIMUM_SOC_perc (100.0f)                 /* 定义 SOC 的最大百分比限制为 100.0% */
/** 最小 SOC 百分比 */
#define SOC_MINIMUM_SOC_perc (0.0f)                   /* 定义 SOC 的最小百分比限制为 0.0% */

/*========== 静态常量和变量定义 =======================*/
/** SOC 模块的状态变量 */
static SOC_STATE_s soc_state = {                      /* 定义 SOC 模块的状态实例，静态局部变量，仅本文件可见，并进行初始化 */
    .socInitialized                  = false,         /* 初始化标志位初始化为 false */
    .sensorCcUsed                    = {GEN_REPEAT_U(false, GEN_STRIP(BS_NR_OF_STRINGS))}, /* 库仑计数使用标志数组全部初始化为 false */
    .ccScalingAverage                = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 平均 SOC 缩放基准数组全部初始化为 0.0f */
    .ccScalingMinimum                = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 最小 SOC 缩放基准数组全部初始化为 0.0f */
    .ccScalingMaximum                = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 最大 SOC 缩放基准数组全部初始化为 0.0f */
    .chargeThroughput_As             = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 充电吞吐量数组全部初始化为 0.0f */
    .dischargeThroughput_As          = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 放电吞吐量数组全部初始化为 0.0f */
    .previousCurrentCountingValue_As = {GEN_REPEAT_U(0u, GEN_STRIP(BS_NR_OF_STRINGS))},    /* 上次电流计数值数组全部初始化为 0 */
    .previousTimestamp               = {GEN_REPEAT_U(0u, GEN_STRIP(BS_NR_OF_STRINGS))},    /* 上次时间戳数组全部初始化为 0 */
};

/** 数据库表的本地副本 */
/**@{*/
static DATA_BLOCK_CURRENT_s soc_tableCurrent                = {.header.uniqueId = DATA_BLOCK_ID_CURRENT};         /* 定义本地数据库表副本：电流数据，初始化唯一ID */
static DATA_BLOCK_CURRENT_COUNTER_s soc_tableCurrentCounter = {.header.uniqueId = DATA_BLOCK_ID_CURRENT_COUNTER}; /* 定义本地数据库表副本：电流计数器数据，初始化唯一ID */
/**@}*/

/*========== 外部常量和变量定义 =======================*/
/* 外部常量和变量定义（本文件无） */

/*========== 静态函数原型 =====================================*/

/**
 * @brief   根据传入的串电荷量(As)计算串 SOC 百分比
 * @param[in] charge_As   电荷量，单位 As
 * @return 返回对应的串 SOC 百分比 [0.0, 100.0]
 */
static float_t SOC_GetStringSocPercentageFromCharge(uint32_t charge_As); /* 静态函数声明：根据输入的电荷量计算对应的 SOC 百分比 */

/**
 * @brief   通过查找表（平均值、最小值和最大值）初始化数据库和 FRAM SOC 值。
 * @param[out] pTableSoc  指向带有 SOC 值的数据库条目的指针
 */
static void SOC_RecalibrateViaLookupTable(DATA_BLOCK_SOC_s *pTableSoc); /* 静态函数声明：通过查表法重新校准(初始化)数据库和 FRAM 中的 SOC 值 */

/**
 * @brief   将 SOC 值设置为 0.0 到 100.0 之间的参数。
 * @details 如果传递的值超出了允许的 SOC 范围，则将 SOE 值限制为 0.0 或 100.0。
 *          更新本地 fram 和 database 结构体，但*不*写入它们
 * @param[out]  pTableSoc  指向 SOC 数据库条目的指针
 * @param[in]   socMinimumValue_perc  要设置的最小 SOC 值
 * @param[in]   socMaximumValue_perc  要设置的最大 SOC 值
 * @param[in]   socAverageValue_perc  要设置的平均 SOC 值
 * @param[in]   stringNumber     寻址的串号
 */
static void SOC_SetValue(                                              /* 静态函数声明：设置指定串的 SOC 值，并处理限幅和缩放基准更新 */
    DATA_BLOCK_SOC_s *pTableSoc,                                       /* 参数：指向 SOC 数据库条目的指针 */
    float_t socMinimumValue_perc,                                      /* 参数：要设置的最小 SOC 值 */
    float_t socMaximumValue_perc,                                      /* 参数：要设置的最大 SOC 值 */
    float_t socAverageValue_perc,                                      /* 参数：要设置的平均 SOC 值 */
    uint8_t stringNumber);                                             /* 参数：目标电池串编号 */

/**
 * @brief   检查所有数据库 SOC 百分比值是否在 [0.0, 100.0] 范围内
 *          如果超出此范围，则将 SOC 值限制为极限值。
 * @param[in,out] pTableSoc  指向带有 SOC 值的数据库结构体的指针
 * @param[in] stringNumber   被检查的串
 */
static void SOC_CheckDatabaseSocPercentageLimits(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber); /* 静态函数声明：检查并限制 SOC 百分比在[0.0, 100.0]范围内 */

/**
 * @brief   在非易失性存储器中设置与 SOC 相关的值
 * @param[in] pTableSoc      指向带有 SOC 值的数据库结构体的指针
 * @param[in] stringNumber   寻址的串号
 */
static void SOC_UpdateNvmValues(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber); /* 静态函数声明：将 SOC 相关值更新到非易失性内存(FRAM)缓存中 */

/*========== 静态函数实现 ================================*/
static float_t SOC_GetStringSocPercentageFromCharge(uint32_t charge_As) { /* 函数定义：将电荷量转换为 SOC 百分比 */
    const float_t charge_mAs = (float_t)charge_As * UNIT_CONVERSION_FACTOR_1000_FLOAT; /* 将电荷量从安培秒转换为毫安培秒，乘以 1000.0f */
    return UNIT_CONVERSION_FACTOR_100_FLOAT * (charge_mAs / SOC_STRING_CAPACITY_mAs); /* 计算百分比：(当前电荷量 / 串总容量) * 100，并返回 */
}

static void SOC_RecalibrateViaLookupTable(DATA_BLOCK_SOC_s *pTableSoc) { /* 函数定义：通过查表法重新校准 SOC */
    FAS_ASSERT(pTableSoc != NULL_PTR);                                 /* 断言检查：确保指针不为空 */
    DATA_BLOCK_MIN_MAX_s tableMinMaxCellVoltages = {.header.uniqueId = DATA_BLOCK_ID_MIN_MAX}; /* 定义局部变量存储最大最小电压，并初始化唯一ID */
    DATA_READ_DATA(&tableMinMaxCellVoltages);                          /* 从数据库读取最新的单体电压极值数据 */

    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {                 /* 遍历电池系统中所有的串 */
        SOC_SetValue(                                                  /* 调用 SOC_SetValue 函数设置当前串的 SOC 值 */
            pTableSoc,                                                 /* 传入 SOC 数据库指针 */
            SE_GetStateOfChargeFromVoltage(tableMinMaxCellVoltages.minimumCellVoltage_mV[s]), /* 根据最低单体电压查表求最小 SOC */
            SE_GetStateOfChargeFromVoltage(tableMinMaxCellVoltages.maximumCellVoltage_mV[s]), /* 根据最高单体电压查表求最大 SOC */
            SE_GetStateOfChargeFromVoltage(tableMinMaxCellVoltages.averageCellVoltage_mV[s]), /* 根据平均单体电压查表求平均 SOC */
            s);                                                        /* 传入当前串编号 */
    }
    FRAM_WriteData(FRAM_BLOCK_ID_SOC);                                 /* 将校准后的 SOC 数据写入 FRAM 非易失性存储器 */
}

static void SOC_SetValue(                                              /* 函数定义：设置指定串的 SOC 值，并更新缩放基准和存储 */
    DATA_BLOCK_SOC_s *pTableSoc,                                       /* 参数：指向 SOC 数据库条目的指针 */
    float_t socMinimumValue_perc,                                      /* 参数：要设置的最小 SOC 值 */
    float_t socMaximumValue_perc,                                      /* 参数：要设置的最大 SOC 值 */
    float_t socAverageValue_perc,                                      /* 参数：要设置的平均 SOC 值 */
    uint8_t stringNumber) {                                            /* 参数：目标电池串编号 */
    FAS_ASSERT(pTableSoc != NULL_PTR);                                 /* 断言检查：确保指针不为空 */

    DATA_READ_DATA(&soc_tableCurrentCounter);                          /* 从数据库读取电流计数器值，用于后续计算库仑计偏移 */

    /* 设置数据库值 */
    pTableSoc->averageSoc_perc[stringNumber] = socAverageValue_perc;  /* 将传入的平均 SOC 值写入数据库对应位置 */
    pTableSoc->minimumSoc_perc[stringNumber] = socMinimumValue_perc;  /* 将传入的最小 SOC 值写入数据库对应位置 */
    pTableSoc->maximumSoc_perc[stringNumber] = socMaximumValue_perc;  /* 将传入的最大 SOC 值写入数据库对应位置 */

    if (soc_state.sensorCcUsed[stringNumber] == true) {               /* 检查该串是否使用了电流传感器的库仑计数功能 */
        /* 在调用 SOC_SetValue 之前已读取电流传感器数据库条目 */
        float_t ccOffset_perc = SOC_GetStringSocPercentageFromCharge( /* 计算当前库仑计数值对应的 SOC 百分比偏移量 */
            (uint32_t)abs(soc_tableCurrentCounter.currentCounter_As[stringNumber])); /* 取电流计数器绝对值并转换为无符号整型 */
        ccOffset_perc *= BS_CURRENT_DIRECTION_FLOAT;                  /* 根据系统电流方向定义(充电为正或负)调整偏移量符号 */

        /* 重新校准缩放值 */
        soc_state.ccScalingAverage[stringNumber] = pTableSoc->averageSoc_perc[stringNumber] + ccOffset_perc; /* 更新平均 SOC 的库仑计缩放基准：当前 SOC + 偏移量 */
        soc_state.ccScalingMinimum[stringNumber] = pTableSoc->minimumSoc_perc[stringNumber] + ccOffset_perc; /* 更新最小 SOC 的库仑计缩放基准：当前 SOC + 偏移量 */
        soc_state.ccScalingMaximum[stringNumber] = pTableSoc->maximumSoc_perc[stringNumber] + ccOffset_perc; /* 更新最大 SOC 的库仑计缩放基准：当前 SOC + 偏移量 */
    }

    /* 将 SOC 值限制在 [0.0, 100.0] */
    SOC_CheckDatabaseSocPercentageLimits(pTableSoc, stringNumber);    /* 调用函数检查并将 SOC 值限幅在 0% 到 100% 之间 */

    /* 更新非易失性存储器中的值 */
    SOC_UpdateNvmValues(pTableSoc, stringNumber);                     /* 调用函数将 SOC 值更新到 FRAM 缓存中 */

    FRAM_WriteData(FRAM_BLOCK_ID_SOC);                                /* 将更新后的 FRAM 缓存数据实际写入 FRAM 硬件 */
}

static void SOC_CheckDatabaseSocPercentageLimits(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber) { /* 函数定义：检查并限制数据库中 SOC 百分比在合法范围内 */
    FAS_ASSERT(pTableSoc != NULL_PTR);                                 /* 断言检查：确保指针不为空 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);                      /* 断言检查：确保串号在有效范围内 */

    if (pTableSoc->averageSoc_perc[stringNumber] > SOC_MAXIMUM_SOC_perc) { /* 如果平均 SOC 大于 100.0% */
        pTableSoc->averageSoc_perc[stringNumber] = SOC_MAXIMUM_SOC_perc; /* 则将其限制为 100.0% */
    }
    if (pTableSoc->averageSoc_perc[stringNumber] < SOC_MINIMUM_SOC_perc) { /* 如果平均 SOC 小于 0.0% */
        pTableSoc->averageSoc_perc[stringNumber] = SOC_MINIMUM_SOC_perc; /* 则将其限制为 0.0% */
    }
    if (pTableSoc->minimumSoc_perc[stringNumber] > SOC_MAXIMUM_SOC_perc) { /* 如果最小 SOC 大于 100.0% */
        pTableSoc->minimumSoc_perc[stringNumber] = SOC_MAXIMUM_SOC_perc; /* 则将其限制为 100.0% */
    }
    if (pTableSoc->minimumSoc_perc[stringNumber] < SOC_MINIMUM_SOC_perc) { /* 如果最小 SOC 小于 0.0% */
        pTableSoc->minimumSoc_perc[stringNumber] = SOC_MINIMUM_SOC_perc; /* 则将其限制为 0.0% */
    }
    if (pTableSoc->maximumSoc_perc[stringNumber] > SOC_MAXIMUM_SOC_perc) { /* 如果最大 SOC 大于 100.0% */
        pTableSoc->maximumSoc_perc[stringNumber] = SOC_MAXIMUM_SOC_perc; /* 则将其限制为 100.0% */
    }
    if (pTableSoc->maximumSoc_perc[stringNumber] < SOC_MINIMUM_SOC_perc) { /* 如果最大 SOC 小于 0.0% */
        pTableSoc->maximumSoc_perc[stringNumber] = SOC_MINIMUM_SOC_perc; /* 则将其限制为 0.0% */
    }
}

static void SOC_UpdateNvmValues(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber) { /* 函数定义：将数据库中的 SOC 值和吞吐量拷贝到 FRAM 缓存结构体中 */
    FAS_ASSERT(pTableSoc != NULL_PTR);                                 /* 断言检查：确保指针不为空 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);                      /* 断言检查：确保串号在有效范围内 */
    fram_soc.averageSoc_perc[stringNumber]        = pTableSoc->averageSoc_perc[stringNumber]; /* 将平均 SOC 从数据库拷贝到 FRAM 缓存 */
    fram_soc.minimumSoc_perc[stringNumber]        = pTableSoc->minimumSoc_perc[stringNumber]; /* 将最小 SOC 从数据库拷贝到 FRAM 缓存 */
    fram_soc.maximumSoc_perc[stringNumber]        = pTableSoc->maximumSoc_perc[stringNumber]; /* 将最大 SOC 从数据库拷贝到 FRAM 缓存 */
    fram_soc.chargeThroughput_As[stringNumber]    = pTableSoc->chargeThroughput_As[stringNumber]; /* 将充电吞吐量从数据库拷贝到 FRAM 缓存 */
    fram_soc.dischargeThroughput_As[stringNumber] = pTableSoc->dischargeThroughput_As[stringNumber]; /* 将放电吞吐量从数据库拷贝到 FRAM 缓存 */
}

/*========== 外部函数实现 ================================*/

void SE_InitializeStateOfCharge(DATA_BLOCK_SOC_s *pSocValues, bool ccPresent, uint8_t stringNumber) { /* 函数定义：初始化状态估计中的 SOC 模块 */
    FAS_ASSERT(pSocValues != NULL_PTR);                               /* 断言检查：确保指针不为空 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);                      /* 断言检查：确保串号在有效范围内 */
    DATA_READ_DATA(&soc_tableCurrent, &soc_tableCurrentCounter);      /* 从数据库读取电流数据和电流计数器数据 */

    FRAM_ReadData(FRAM_BLOCK_ID_SOC);                                 /* 从 FRAM 硬件中读取之前保存的 SOC 数据到 fram_soc 缓存 */

    if (ccPresent == true) {                                          /* 如果当前串存在库仑计数(电流积分)功能 */
        soc_state.sensorCcUsed[stringNumber] = true;                  /* 标记该串使用了库仑计数功能 */
        soc_state.previousCurrentCountingValue_As[stringNumber] =     /* 保存当前库仑计数值作为下次计算的基准 */
            soc_tableCurrentCounter.currentCounter_As[stringNumber];  /* 赋值：当前库仑计数值 */

        float_t scalingOffset_perc = SOC_GetStringSocPercentageFromCharge( /* 计算当前库仑计数值对应的 SOC 百分比偏移量 */
            (uint32_t)abs(soc_tableCurrentCounter.currentCounter_As[stringNumber])); /* 取库仑计数值的绝对值进行转换 */

        if (soc_tableCurrentCounter.currentCounter_As[stringNumber] < 0) { /* 如果库仑计数值为负数 */
            scalingOffset_perc *= (-1.0f);                            /* 则将偏移量取反，保持符号一致 */
        }

        scalingOffset_perc *= BS_CURRENT_DIRECTION_FLOAT;             /* 根据系统电流方向定义调整偏移量符号 */

        soc_state.ccScalingAverage[stringNumber] = fram_soc.averageSoc_perc[stringNumber] + scalingOffset_perc; /* 初始化平均 SOC 的库仑计缩放基准：FRAM 中的 SOC + 偏移量 */
        soc_state.ccScalingMinimum[stringNumber] = fram_soc.minimumSoc_perc[stringNumber] + scalingOffset_perc; /* 初始化最小 SOC 的库仑计缩放基准：FRAM 中的 SOC + 偏移量 */
        soc_state.ccScalingMaximum[stringNumber] = fram_soc.maximumSoc_perc[stringNumber] + scalingOffset_perc; /* 初始化最大 SOC 的库仑计缩放基准：FRAM 中的 SOC + 偏移量 */
    } else {                                                          /* 如果当前串没有库仑计数功能，则使用电流直接积分法 */
        soc_state.previousTimestamp[stringNumber] = soc_tableCurrent.timestamp[stringNumber]; /* 记录当前电流测量的时间戳，作为下次计算的时间基准 */
        soc_state.sensorCcUsed[stringNumber]      = false;            /* 标记该串未使用库仑计数功能 */
    }

    pSocValues->averageSoc_perc[stringNumber]        = fram_soc.averageSoc_perc[stringNumber]; /* 用 FRAM 中存储的平均 SOC 初始化数据库值 */
    pSocValues->minimumSoc_perc[stringNumber]        = fram_soc.minimumSoc_perc[stringNumber]; /* 用 FRAM 中存储的最小 SOC 初始化数据库值 */
    pSocValues->maximumSoc_perc[stringNumber]        = fram_soc.maximumSoc_perc[stringNumber]; /* 用 FRAM 中存储的最大 SOC 初始化数据库值 */
    pSocValues->dischargeThroughput_As[stringNumber] = fram_soc.dischargeThroughput_As[stringNumber]; /* 用 FRAM 中存储的放电吞吐量初始化数据库值 */
    pSocValues->chargeThroughput_As[stringNumber]    = fram_soc.chargeThroughput_As[stringNumber]; /* 用 FRAM 中存储的充电吞吐量初始化数据库值 */

    SOC_CheckDatabaseSocPercentageLimits(pSocValues, stringNumber);   /* 检查并限幅初始化后的 SOC 值，确保在 [0, 100] 范围内 */

    /* 或者，如果可用，可以使用 {V,SOC} 查找表初始化 SOC */
    /* 使用函数 SOC_Init_Lookup_Table() */

    soc_state.socInitialized = true;                                  /* 将 SOC 模块初始化标志位置为 true，表示初始化完成 */
}

/* 文档的包含标记；请勿移动 cc-documentation-start-include */
void SE_CalculateStateOfCharge(DATA_BLOCK_SOC_s *pSocValues) {        /* 函数定义：周期性调用以计算电池的 SOC 状态 */
    
    FAS_ASSERT(pSocValues != NULL_PTR);                               /* 断言检查：确保指针不为空 */
    bool continueFunction = true;                                     /* 定义并初始化继续执行函数的标志位 */

    if (soc_state.socInitialized == false) {                          /* 检查 SOC 模块是否已初始化 */
        /* 如果 SOC 尚未初始化则退出 */
        continueFunction = false;                                     /* 如果未初始化，则将标志位置为 false，退出后续计算 */
    }

    if (continueFunction == true) {                                   /* 如果允许继续执行 */
        if (BMS_GetBatterySystemState() == BMS_AT_REST) {             /* 检查电池系统当前是否处于静置状态 */
            /* 通过查找表重新校准 SOC */
            SOC_RecalibrateViaLookupTable(pSocValues);                /* 如果是静置状态，利用开路电压查表法重新校准 SOC，消除累积误差 */
        } else {                                                      /* 如果电池系统处于工作状态(正在充放电) */
            /* 读取电流传感器条目以进行库仑/电流计数或 CC 重新校准 */
            DATA_READ_DATA(&soc_tableCurrent, &soc_tableCurrentCounter); /* 从数据库读取最新的电流值和库仑计数值 */
            for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {         /* 遍历所有电池串 */

                /* ==================================================================================== */
                /* ================== 分支一：安时积分法 (软件侧电流积分) ================== */
                /* ==================================================================================== */
                if (soc_state.sensorCcUsed[s] == false) {             /* 判断该串是否未使用传感器库仑计数(按时积分法) */
                    /* 检查电流测量是否已更新 */
                    if (soc_state.previousTimestamp[s] != soc_tableCurrent.timestamp[s]) { /* 检查电流测量值的时间戳是否更新，避免重复计算 */
                        float_t timeStep_s =                           /* 计算时间步长(单位：秒) */
                            ((float_t)(soc_tableCurrent.timestamp[s] - soc_state.previousTimestamp[s])) / 1000.0f; /* (当前时间戳 - 上次时间戳) / 1000.0，假设时间戳单位为 ms */

                        if (timeStep_s > 0.0f) {                      /* 确保时间步长有效(大于 0) */
                            /* 充电方向电流为负意味着 SOC 增加 --> BAT 命名，而非 ROB */

                            /* 按时积分计算 SOC 变化量(百分比) */
                            /* ((mA) * 1s) / 1As) * 100%  公式：电流 * 时间 / 容量 * 100% */
                            float_t deltaSOC_perc =(((float_t)soc_tableCurrent.current_mA[s] * timeStep_s) / SOC_STRING_CAPACITY_mAs) *100.0f;  

                            float_t charge_As = fabs((float_t)soc_tableCurrent.current_mA[s] * timeStep_s / 1000.0f); /* 计算本周期内的电荷量绝对值(单位：安培秒)，用于统计吞吐量 */

                            deltaSOC_perc *= BS_CURRENT_DIRECTION_FLOAT; /* 根据系统电流方向定义修正 SOC 变化量的符号，确保充电 SOC 增加，放电 SOC 减少 */

                            pSocValues->averageSoc_perc[s] = pSocValues->averageSoc_perc[s] - deltaSOC_perc; /* 累加平均 SOC：原值减去变化量(因方向定义可能为负，故用减号) */
                            pSocValues->minimumSoc_perc[s] = pSocValues->minimumSoc_perc[s] - deltaSOC_perc; /* 累加最小 SOC：原值减去变化量 */
                            pSocValues->maximumSoc_perc[s] = pSocValues->maximumSoc_perc[s] - deltaSOC_perc; /* 累加最大 SOC：原值减去变化量 */
                            if (BMS_GetCurrentFlowDirection(soc_tableCurrent.current_mA[s]) == BMS_CHARGING) { /* 判断当前电流方向是否为充电 */
                                pSocValues->chargeThroughput_As[s] = pSocValues->chargeThroughput_As[s] + charge_As; /* 如果是充电，累加充电吞吐量 */
                            } else {                                  /* 如果不是充电(放电或静置) */
                                /* 当 BMS_DISCHARGING 和 BMS_AT_REST 时，将电荷加到 dischargeThroughput */
                                pSocValues->dischargeThroughput_As[s] = pSocValues->dischargeThroughput_As[s] +
                                                                        charge_As; /* 如果是放电或静置，累加放电吞吐量 */
                            }
                            /* 将 SOC 计算限制为 0% 和 100% */
                            SOC_CheckDatabaseSocPercentageLimits(pSocValues, s); /* 检查并将计算后的 SOC 值限幅在 [0%, 100%] 范围内 */

                            /* 更新非易失性存储器中的值 */
                            SOC_UpdateNvmValues(pSocValues, s);       /* 将更新后的 SOC 和吞吐量写入 FRAM 缓存 */
                        }
                        soc_state.previousTimestamp[s] = soc_tableCurrent.timestamp[s]; /* 更新时间戳，为下一次计算做准备 */
                    } /* 检查电流测量是否已更新结束 */
                    /* 更新变量以供下次检查 */
                } else {                                              /* 如果该串使用了传感器库仑计数功能 */
                    /* ==================================================================================== */
                    /* ================== 分支二：库仑计数法 (传感器库仑计数) ================== */
                    /* ==================================================================================== */
                    /* 检查库仑计数测量是否已更新 */
                    if (soc_state.previousTimestamp[s] != soc_tableCurrentCounter.timestamp[s]) { /* 检查库仑计数器的时间戳是否更新 */
                        /* 计算基于库仑计数值的 SOC 百分比 */
                        float_t deltaSoc_perc = ((float_t)soc_tableCurrentCounter.currentCounter_As[s] / SOC_STRING_CAPACITY_As) * 100.0f; /* (当前库仑计数值 / 串总容量 As) * 100 */

                        /* 更新 SOC 百分比 计算与上次相比的电荷量差值绝对值，用于吞吐量统计 */
                        float_t chargeDifference_As = fabs((float_t)soc_tableCurrentCounter.currentCounter_As[s] -soc_state.previousCurrentCountingValue_As[s]); /* 当前库仑计数值 - 上次库仑计数值 */

                        deltaSoc_perc *= BS_CURRENT_DIRECTION_FLOAT;   /* 根据系统电流方向定义修正 SOC 的符号 */

                        pSocValues->averageSoc_perc[s] = soc_state.ccScalingAverage[s] - deltaSoc_perc; /* 计算平均 SOC：缩放基准值 - 当前库仑计 SOC */
                        pSocValues->minimumSoc_perc[s] = soc_state.ccScalingMinimum[s] - deltaSoc_perc; /* 计算最小 SOC：缩放基准值 - 当前库仑计 SOC */
                        pSocValues->maximumSoc_perc[s] = soc_state.ccScalingMaximum[s] - deltaSoc_perc; /* 计算最大 SOC：缩放基准值 - 当前库仑计 SOC */
                        if (BMS_GetCurrentFlowDirection(soc_tableCurrent.current_mA[s]) == BMS_CHARGING) { /* 判断当前电流方向是否为充电 */
                            pSocValues->chargeThroughput_As[s] = pSocValues->chargeThroughput_As[s] +chargeDifference_As; /* 如果是充电，累加充电吞吐量差值 */
                        } else {                                      /* 如果不是充电(放电或静置) */
                            /* 当 BMS_DISCHARGING 和 BMS_AT_REST 时，将电荷加到 dischargeThroughput */
                            pSocValues->dischargeThroughput_As[s] = pSocValues->dischargeThroughput_As[s] +chargeDifference_As; /* 如果是放电或静置，累加放电吞吐量差值 */
                        }

                        /* 将 SOC 值限制在 [0.0, 100.0] */
                        SOC_CheckDatabaseSocPercentageLimits(pSocValues, s); /* 检查并将计算后的 SOC 值限幅在 [0%, 100%] 范围内 */

                        /* 更新非易失性存储器中的值 */
                        SOC_UpdateNvmValues(pSocValues, s);           /* 将更新后的 SOC 和吞吐量写入 FRAM 缓存 */
                        soc_state.previousCurrentCountingValue_As[s] = soc_tableCurrentCounter.currentCounter_As[s]; /* 更新上一次库仑计数值，为下次计算差值做准备 */
                        soc_state.previousTimestamp[s]               = soc_tableCurrentCounter.timestamp[s]; /* 更新时间戳，为下次判断是否更新做准备 */
                    } /* 检查库仑计数测量是否已更新结束 */
                }
            }
            /* 更新数据库和 FRAM 值 */
            FRAM_WriteData(FRAM_BLOCK_ID_SOC);                        /* 遍历完所有串后，将 FRAM 缓存中的数据实际写入 FRAM 硬件 */
        }
    }
}

extern float_t SE_GetStateOfChargeFromVoltage(int16_t voltage_mV) {   /* 函数定义：根据单体电压通过查表和线性插值计算 SOC */
    float_t soc_perc = 0.50f;                                         /* 初始化返回值为 0.5(50%)，作为异常情况下的安全默认值 */

    /* 用于插值查找表值的变量 */
    uint16_t between_high = 0;                                        /* 用于插值的查找表索引边界：指向高电压(低 SOC)的索引 */
    uint16_t between_low  = 0;                                        /* 用于插值的查找表索引边界：指向低电压(高 SOC)的索引 */

    /* 单体电压以降序插入查找表中 -> 从 1 开始，因为我们不想外推。 */
    for (uint16_t i = 1u; i < bc_stateOfChargeLookupTableLength; i++) { /* 遍历查找表，从 1 开始避免外推。查找表电压为降序排列 */
        if (voltage_mV < bc_stateOfChargeLookupTable[i].voltage_mV) { /* 如果输入电压小于当前查找表项的电压 */
            between_low  = i + 1u;                                    /* 输入电压必定在比当前索引更小的位置(更低电压)，记录低端索引 */
            between_high = i;                                         /* 当前索引作为高电压边界，记录高端索引 */
        }
    }

    /* 在查找表值之间进行插值，但不要对查找表进行外推！ */
    if (!(((between_high == 0u) && (between_low == 0u)) ||       /* 单体电压 > 最大查找表电压 */ /* 如果不是(电压超出表上限) */
          (between_low >= bc_stateOfChargeLookupTableLength))) { /* 单体电压 < 最小查找表电压 */ /* 且不是(电压低于表下限) */
        soc_perc = MATH_LinearInterpolation(                          /* 在查找表区间内，调用数学库的线性插值函数计算 SOC */
            (float_t)bc_stateOfChargeLookupTable[between_low].voltage_mV, /* 区间低端电压(X1) */
            bc_stateOfChargeLookupTable[between_low].value,            /* 区间低端 SOC(Y1) */
            (float_t)bc_stateOfChargeLookupTable[between_high].voltage_mV, /* 区间高端电压(X2) */
            bc_stateOfChargeLookupTable[between_high].value,           /* 区间高端 SOC(Y2) */
            (float_t)voltage_mV);                                      /* 输入电压 */
    } else if ((between_low >= bc_stateOfChargeLookupTableLength)) { /* 如果电压低于查找表中的最小电压 */
        /* 查找表 SOE 值按降序排列：单体电压 < 最小查找表电压 */
        soc_perc = SOC_MINIMUM_SOC_perc;                              /* 说明电池已彻底没电，返回 0% */
    } else {                                                          /* 如果电压高于查找表中的最大电压 */
        /* 单体电压 > 最大查找表电压 */
        soc_perc = 100.0f;                                            /* 说明电池已完全充满，返回 100% */
    }
    return soc_perc;                                                  /* 返回计算出的 SOC 百分比 */
}

/*========== 外部化的静态函数实现 (单元测试) =======*/
#ifdef UNITY_UNIT_TEST                                                /* 如果定义了 UNITY_UNIT_TEST 宏，用于单元测试 */
extern void TEST_SOC_CheckDatabaseSocPercentageLimits(DATA_BLOCK_SOC_s *TableSoc, uint8_t stringNumber) { /* 导出静态函数供测试框架调用：检查 SOC 限幅 */
    SOC_CheckDatabaseSocPercentageLimits(TableSoc, stringNumber);     /* 调用实际的静态函数 */
}
extern void TEST_SOC_UpdateNvmValues(DATA_BLOCK_SOC_s *TableSoc, uint8_t stringNumber) { /* 导出静态函数供测试框架调用：更新 NVM 值 */
    SOC_UpdateNvmValues(TableSoc, stringNumber);                      /* 调用实际的静态函数 */
}
#endif
