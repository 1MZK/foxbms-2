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
 * @file    soe_counting.c
 * @author  foxBMS 团队
 * @date    2020-10-07 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup APPLICATION
 * @prefix  SOE
 *
 * @brief   负责计算 SOE 的 SOE 模块
 * @details 待办
 */

/*========== 包含文件 =======================================================*/
#include "battery_cell_cfg.h"                         /* 包含电池单体配置参数 */
#include "battery_system_cfg.h"                       /* 包含电池系统配置参数 */
#include "soe_counting_cfg.h"                         /* 包含 SOE 计数模块的配置头文件 */

#include "bms.h"                                      /* 包含 BMS 全局定义和状态获取接口 */
#include "database.h"                                 /* 包含数据库接口，用于读写共享数据 */
#include "foxmath.h"                                  /* 包含数学工具库，如线性插值算法等 */
#include "fram.h"                                     /* 包含 FRAM 驱动，用于非易失性存储 */
#include "state_estimation.h"                         /* 包含状态估计模块头文件 */

#include <math.h>                                     /* 包含标准数学库 */
#include <stdbool.h>                                  /* 包含布尔类型支持 */
#include <stdint.h>                                   /* 包含标准整型定义 */

/*========== 宏和定义 =========================================================*/
/**
 * 此结构体包含与 SOE 相关的所有变量。
 */
typedef struct {
    bool soeInitialized;                        /*!< 如果初始化已通过则为 true，否则为 false */
    bool sensorEcUsed[BS_NR_OF_STRINGS];        /*!< 如果使用了电流传感器的能量计数功能则为 true */
    float_t ecScalingAverage[BS_NR_OF_STRINGS]; /*!< 平均 SOE 的电流传感器偏移缩放 */
    float_t ecScalingMinimum[BS_NR_OF_STRINGS]; /*!< 最小 SOE 的电流传感器偏移缩放 */
    float_t ecScalingMaximum[BS_NR_OF_STRINGS]; /*!< 最大 SOE 的电流传感器偏移缩放 */
    float_t chargeEnergyThroughput_Wh[BS_NR_OF_STRINGS];    /*!< 流入的能量(充电吞吐量) */
    float_t dischargeEnergyThroughput_Wh[BS_NR_OF_STRINGS]; /*!< 流出的能量(放电吞吐量) */
    float_t previousEnergyCount_Wh[BS_NR_OF_STRINGS];       /*!< 上一次的能量计数值 */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS]; /*!< 用于 SOE 估计的电流或能量计数值的上次使用时间戳 */
} SOE_STATE_s;

/** 最大和最小 SOE 的定义 */
#define MAXIMUM_SOE_PERC (100.0f)                     /* 定义最大 SOE 百分比为 100.0% */
#define MINIMUM_SOE_PERC (0.0f)                       /* 定义最小 SOE 百分比为 0.0% */

/*========== 静态常量和变量定义 =======================*/

/**
 * 包含 SOE 估计的状态
 */
static SOE_STATE_s soe_state = {                      /* 定义 SOE 模块的状态实例，静态局部变量，仅本文件可见 */
    .soeInitialized               = false,            /* 初始化标志位初始化为 false */
    .sensorEcUsed                 = {GEN_REPEAT_U(false, GEN_STRIP(BS_NR_OF_STRINGS))}, /* 能量计数使用标志数组全部初始化为 false */
    .ecScalingAverage             = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 平均 SOE 缩放基准数组全部初始化为 0.0f */
    .ecScalingMinimum             = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 最小 SOE 缩放基准数组全部初始化为 0.0f */
    .ecScalingMaximum             = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 最大 SOE 缩放基准数组全部初始化为 0.0f */
    .chargeEnergyThroughput_Wh    = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 充电能量吞吐量数组全部初始化为 0.0f */
    .dischargeEnergyThroughput_Wh = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 放电能量吞吐量数组全部初始化为 0.0f */
    .previousEnergyCount_Wh       = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 上次能量计数值数组全部初始化为 0.0f */
    .previousTimestamp            = {GEN_REPEAT_U(0u, GEN_STRIP(BS_NR_OF_STRINGS))},     /* 上次时间戳数组全部初始化为 0 */
};

/** 数据库表的本地副本 */
/**@{*/
static DATA_BLOCK_CURRENT_s soe_tableCurrent                 = {.header.uniqueId = DATA_BLOCK_ID_CURRENT};         /* 电流数据本地副本 */
static DATA_BLOCK_ENERGY_COUNTER_s soe_tableEnergyCounter    = {.header.uniqueId = DATA_BLOCK_ID_ENERGY_COUNTER}; /* 能量计数器数据本地副本 */
static DATA_BLOCK_SYSTEM_VOLTAGE_1_s soe_tableSystemVoltage1 = {.header.uniqueId = DATA_BLOCK_ID_SYSTEM_VOLTAGE_1}; /* 系统电压数据本地副本 */
/**@}*/

/*========== 外部常量和变量定义 =======================*/
/* 外部常量和变量定义（本文件无） */

/*========== 静态函数原型 =====================================*/

/**
 * @brief   根据传入的 SOE 百分比计算串能量
 *
 * @param[in] stringSoe_perc   串 SOE 百分比 [0.0, 100.0]
 *
 * @return 返回对应的串能量
 */
static uint32_t SOE_GetStringEnergyFromSoePercentage(float_t stringSoe_perc);

/**
 * @brief   根据传入的串能量计算串 SOE 百分比(Wh)
 *
 * @param[in] energy_Wh   串能量，单位 Wh
 *
 * @return 返回对应的串 SOE 百分比 [0.0, 100.0]
 */
static float_t SOE_GetStringSoePercentageFromEnergy(uint32_t energy_Wh);

/**
 * @brief   通过查找表（平均值、最小值和最大值）初始化数据库和 FRAM SOE 值。
 * @param[out] pSoeValues   指向 SOE 数据库条目的指针
 */
static void SOE_RecalibrateViaLookupTable(DATA_BLOCK_SOE_s *pSoeValues);

/**
 * @brief   用于 SOE 初始化的查找表
 *
 * @param[in] voltage_mV  电池单体的电压
 *
 * @return  SOE 值的百分比 [0.0 - 100.0]
 */
static float_t SOE_GetFromVoltage(int16_t voltage_mV);

/**
 * @brief   将 SOE 值设置为 0.0 到 100.0 之间的参数。
 * @details 如果传递的值超出了允许的 SOE 范围，则将 SOE 值限制为 0.0 或 100.0。
 *          更新本地 fram 和 database 结构体，但*不*写入它们
 * @param[out]  pSoeValues  指向 SOE 数据库条目的指针
 * @param[in]   soeMinimumValue_perc  要设置的最小 SOE 值
 * @param[in]   soeMaximumValue_perc  要设置的最大 SOE 值
 * @param[in]   soeAverageValue_perc  要设置的平均 SOE 值
 * @param[in]   stringNumber     寻址的串
 */
static void SOE_SetValue(
    DATA_BLOCK_SOE_s *pSoeValues,
    float_t soeMinimumValue_perc,
    float_t soeMaximumValue_perc,
    float_t soeAverageValue_perc,
    uint8_t stringNumber);

/**
 * @brief   检查所有数据库 SOE 百分比值是否在 [0.0, 100.0] 范围内
 *          如果超出此范围，则将 SOE 值限制为极限值。
 *
 * @param[in,out] pTableSoe  指向带有 SOE 值的数据库结构体的指针
 * @param[in] stringNumber   被检查的串
 */
static void SOE_CheckDatabaseSoePercentageLimits(DATA_BLOCK_SOE_s *pTableSoe, uint8_t stringNumber);

/*========== 静态函数实现 ================================*/
static float_t SOE_GetStringSoePercentageFromEnergy(uint32_t energy_Wh) { /* 函数定义：将能量转化为SOE百分比 */
    float_t stringSoe_perc        = 0.0f;                              /* 初始化返回变量为 0.0% */
    const float_t stringEnergy_Wh = (float_t)energy_Wh;                /* 将输入的无符号整型能量值转换为浮点型 */
    if (stringEnergy_Wh >= SOE_STRING_ENERGY_Wh) {                     /* 如果当前能量大于或等于串的满电总能量 */
        stringSoe_perc = MAXIMUM_SOE_PERC;                             /* 则 SOE 百分比设为最大值 100.0% */
    } else {                                                           /* 否则 */
        stringSoe_perc = UNIT_CONVERSION_FACTOR_100_FLOAT * (stringEnergy_Wh / SOE_STRING_ENERGY_Wh); /* 计算百分比：(当前能量 / 总能量) * 100 */
    }
    return stringSoe_perc;                                             /* 返回计算出的 SOE 百分比 */
}

static uint32_t SOE_GetStringEnergyFromSoePercentage(float_t stringSoe_perc) { /* 函数定义：将SOE百分比转化为能量 */
    float_t energy_Wh = 0.0f;                                          /* 初始化能量变量为 0.0 */
    if (stringSoe_perc >= MAXIMUM_SOE_PERC) {                          /* 如果 SOE 百分比大于或等于 100.0% */
        energy_Wh = SOE_STRING_ENERGY_Wh;                              /* 则能量等于串的总能量 */
    } else if (stringSoe_perc <= MINIMUM_SOE_PERC) {                   /* 如果 SOE 百分比小于或等于 0.0% */
        energy_Wh = MINIMUM_SOE_PERC;                                  /* 则能量等于 0.0 */
    } else {                                                           /* 如果在 0~100% 之间 */
        energy_Wh = SOE_STRING_ENERGY_Wh * (stringSoe_perc / UNIT_CONVERSION_FACTOR_100_FLOAT); /* 计算能量：总能量 * (当前百分比 / 100) */
    }
    return (uint32_t)energy_Wh;                                        /* 将浮点型能量强制转换为无符号整型并返回 */
}

static void SOE_RecalibrateViaLookupTable(DATA_BLOCK_SOE_s *pSoeValues) { /* 函数定义：通过查表法重新校准 SOE */
    FAS_ASSERT(pSoeValues != NULL_PTR);                                /* 断言检查：确保指针不为空 */
    DATA_BLOCK_MIN_MAX_s tableMinimumMaximumAverage = {.header.uniqueId = DATA_BLOCK_ID_MIN_MAX}; /* 定义局部变量存储最大最小平均电压，并初始化唯一ID */

    DATA_READ_DATA(&tableMinimumMaximumAverage);                       /* 从数据库读取最新的单体电压极值数据 */

    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {                 /* 遍历电池系统中所有的串 */
        SOE_SetValue(                                                  /* 调用 SOE_SetValue 函数设置当前串的 SOE 值 */
            pSoeValues,                                                /* 传入 SOE 数据库指针 */
            SOE_GetFromVoltage(tableMinimumMaximumAverage.minimumCellVoltage_mV[s]), /* 根据最低单体电压查表求最小 SOE */
            SOE_GetFromVoltage(tableMinimumMaximumAverage.maximumCellVoltage_mV[s]), /* 根据最高单体电压查表求最大 SOE */
            SOE_GetFromVoltage(tableMinimumMaximumAverage.averageCellVoltage_mV[s]), /* 根据平均单体电压查表求平均 SOE */
            s);                                                        /* 传入当前串编号 */
    }
    FRAM_WriteData(FRAM_BLOCK_ID_SOE);                                 /* 将校准后的 SOE 数据写入 FRAM 非易失性存储器 */
}

static float_t SOE_GetFromVoltage(int16_t voltage_mV) {               /* 函数定义：根据单体电压通过查表和线性插值计算 SOE */
    float_t soe_perc = 50.0f;                                          /* 初始化返回值为 50.0%，作为异常情况下的安全默认值 */

    /* 用于插值查找表值的变量 */
    uint16_t between_high = 0;                                         /* 用于插值的查找表索引边界：指向高电压(低 SOE)的索引 */
    uint16_t between_low  = 0;                                         /* 用于插值的查找表索引边界：指向低电压(高 SOE)的索引 */

    /* 单体电压以降序插入查找表中 -> 从 1 开始，因为我们不想外推。 */
    for (uint16_t i = 1u; i < bc_stateOfEnergyLookupTableLength; i++) { /* 遍历查找表，从 1 开始避免外推。查找表电压为降序排列 */
        if (voltage_mV < bc_stateOfEnergyLookupTable[i].voltage_mV) {  /* 如果输入电压小于当前查找表项的电压 */
            between_low  = i + 1u;                                     /* 输入电压必定在比当前索引更小的位置(更低电压)，记录低端索引 */
            between_high = i;                                          /* 当前索引作为高电压边界，记录高端索引 */
        }
    }

    /* 在查找表值之间进行插值，但不要对查找表进行外推！ */
    if (!(((between_high == 0u) && (between_low == 0u)) ||       /* 单体电压 > 最大查找表电压 */ /* 如果不是(电压超出表上限) */
          (between_low >= bc_stateOfEnergyLookupTableLength))) { /* 单体电压 < 最小查找表电压 */ /* 且不是(电压低于表下限) */
        soe_perc = MATH_LinearInterpolation(                           /* 在查找表区间内，调用数学库的线性插值函数计算 SOE */
            (float_t)bc_stateOfEnergyLookupTable[between_low].voltage_mV, /* 区间低端电压(X1) */
            bc_stateOfEnergyLookupTable[between_low].value,             /* 区间低端 SOE(Y1) */
            (float_t)bc_stateOfEnergyLookupTable[between_high].voltage_mV, /* 区间高端电压(X2) */
            bc_stateOfEnergyLookupTable[between_high].value,            /* 区间高端 SOE(Y2) */
            (float_t)voltage_mV);                                       /* 输入电压 */
    } else if ((between_low >= bc_stateOfEnergyLookupTableLength)) {   /* 如果电压低于查找表中的最小电压 */
        /* 查找表 SOE 值按降序排列：单体电压 < 最小查找表电压 */
        soe_perc = MINIMUM_SOE_PERC;                                   /* 说明电池已彻底没电，返回 0% */
    } else {                                                           /* 如果电压高于查找表中的最大电压 */
        /* 单体电压 > 最大查找表电压 */
        soe_perc = MAXIMUM_SOE_PERC;                                   /* 说明电池已完全充满，返回 100% */
    }
    return soe_perc;                                                   /* 返回计算出的 SOE 百分比 */
}

static void SOE_SetValue(                                              /* 函数定义：设置指定串的 SOE 值，并更新缩放基准和存储 */
    DATA_BLOCK_SOE_s *pSoeValues,                                      /* 参数：指向 SOE 数据库条目的指针 */
    float_t soeMinimumValue_perc,                                      /* 参数：要设置的最小 SOE 值 */
    float_t soeMaximumValue_perc,                                      /* 参数：要设置的最大 SOE 值 */
    float_t soeAverageValue_perc,                                      /* 参数：要设置的平均 SOE 值 */
    uint8_t stringNumber) {                                            /* 参数：目标电池串编号 */
    FAS_ASSERT(pSoeValues != NULL_PTR);                                /* 断言检查：确保指针不为空 */

    /* 更新 FRAM 值 */
    fram_soe.averageSoe_perc[stringNumber] = soeAverageValue_perc;     /* 将传入的平均 SOE 值写入 FRAM 缓存对应位置 */
    fram_soe.minimumSoe_perc[stringNumber] = soeMinimumValue_perc;     /* 将传入的最小 SOE 值写入 FRAM 缓存对应位置 */
    fram_soe.maximumSoe_perc[stringNumber] = soeMaximumValue_perc;     /* 将传入的最大 SOE 值写入 FRAM 缓存对应位置 */

    /* 更新数据库值 */
    pSoeValues->averageSoe_perc[stringNumber] = soeAverageValue_perc;  /* 将传入的平均 SOE 值写入数据库对应位置 */
    pSoeValues->minimumSoe_perc[stringNumber] = soeMinimumValue_perc;  /* 将传入的最小 SOE 值写入数据库对应位置 */
    pSoeValues->maximumSoe_perc[stringNumber] = soeMaximumValue_perc;  /* 将传入的最大 SOE 值写入数据库对应位置 */

    pSoeValues->maximumSoe_Wh[stringNumber] = SOE_GetStringEnergyFromSoePercentage(soeMaximumValue_perc); /* 将最大 SOE 百分比转换为能量并写入数据库 */
    pSoeValues->averageSoe_Wh[stringNumber] = SOE_GetStringEnergyFromSoePercentage(soeAverageValue_perc); /* 将平均 SOE 百分比转换为能量并写入数据库 */
    pSoeValues->minimumSoe_Wh[stringNumber] = SOE_GetStringEnergyFromSoePercentage(soeMinimumValue_perc); /* 将最小 SOE 百分比转换为能量并写入数据库 */

    /* 根据 EC 计数值和当前 SOE 计算缩放值 */
    if (soe_state.sensorEcUsed[stringNumber] == true) {                /* 检查该串是否使用了传感器的能量计数功能 */
        DATA_READ_DATA(&soe_tableEnergyCounter);                       /* 从数据库读取能量计数器值 */

        float_t ecOffset =                                             /* 计算当前能量计数值对应的 SOE 百分比偏移量 */
            SOE_GetStringSoePercentageFromEnergy((uint32_t)abs(soe_tableEnergyCounter.energyCounter_Wh[stringNumber])); /* 取能量计数器绝对值并转换为无符号整型，再求百分比 */

        if (soe_tableEnergyCounter.energyCounter_Wh[stringNumber] < 0) { /* 如果能量计数值为负数 */
            ecOffset *= (-1.0f);                                       /* 则将偏移量取反，保持符号一致 */
        }

        ecOffset *= BS_CURRENT_DIRECTION_FLOAT;                        /* 根据系统电流方向定义调整偏移量符号，对计算出的百分比增量取反 */

        soe_state.ecScalingAverage[stringNumber] = fram_soe.averageSoe_perc[stringNumber] + ecOffset; /* 更新平均 SOE 的能量计数缩放基准：FRAM 中的 SOE + 偏移量 */
        soe_state.ecScalingMinimum[stringNumber] = fram_soe.minimumSoe_perc[stringNumber] + ecOffset; /* 更新最小 SOE 的能量计数缩放基准：FRAM 中的 SOE + 偏移量 */
        soe_state.ecScalingMaximum[stringNumber] = fram_soe.maximumSoe_perc[stringNumber] + ecOffset; /* 更新最大 SOE 的能量计数缩放基准：FRAM 中的 SOE + 偏移量 */
    }
}

static void SOE_CheckDatabaseSoePercentageLimits(DATA_BLOCK_SOE_s *pTableSoe, uint8_t stringNumber) { /* 函数定义：检查并限制数据库中 SOE 百分比在合法范围内 */
    FAS_ASSERT(pTableSoe != NULL_PTR);                                 /* 断言检查：确保指针不为空 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);                       /* 断言检查：确保串号在有效范围内 */

    if (pTableSoe->averageSoe_perc[stringNumber] > MAXIMUM_SOE_PERC) { /* 如果平均 SOE 大于 100.0% */
        pTableSoe->averageSoe_perc[stringNumber] = MAXIMUM_SOE_PERC;   /* 则将其限制为 100.0% */
    }
    if (pTableSoe->averageSoe_perc[stringNumber] < MINIMUM_SOE_PERC) { /* 如果平均 SOE 小于 0.0% */
        pTableSoe->averageSoe_perc[stringNumber] = MINIMUM_SOE_PERC;   /* 则将其限制为 0.0% */
    }
    if (pTableSoe->minimumSoe_perc[stringNumber] > MAXIMUM_SOE_PERC) { /* 如果最小 SOE 大于 100.0% */
        pTableSoe->minimumSoe_perc[stringNumber] = MAXIMUM_SOE_PERC;   /* 则将其限制为 100.0% */
    }
    if (pTableSoe->minimumSoe_perc[stringNumber] < MINIMUM_SOE_PERC) { /* 如果最小 SOE 小于 0.0% */
        pTableSoe->minimumSoe_perc[stringNumber] = MINIMUM_SOE_PERC;   /* 则将其限制为 0.0% */
    }
    if (pTableSoe->maximumSoe_perc[stringNumber] > MAXIMUM_SOE_PERC) { /* 如果最大 SOE 大于 100.0% */
        pTableSoe->maximumSoe_perc[stringNumber] = MAXIMUM_SOE_PERC;   /* 则将其限制为 100.0% */
    }
    if (pTableSoe->maximumSoe_perc[stringNumber] < MINIMUM_SOE_PERC) { /* 如果最大 SOE 小于 0.0% */
        pTableSoe->maximumSoe_perc[stringNumber] = MINIMUM_SOE_PERC;   /* 则将其限制为 0.0% */
    }
}

/*========== 外部函数实现 ================================*/

extern void SE_InitializeStateOfEnergy(DATA_BLOCK_SOE_s *pSoeValues, bool ec_present, uint8_t stringNumber) { /* 函数定义：初始化状态估计中的 SOE 模块 */
    FAS_ASSERT(pSoeValues != NULL_PTR);                                /* 断言检查：确保指针不为空 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);                       /* 断言检查：确保串号在有效范围内 */
    FRAM_ReadData(FRAM_BLOCK_ID_SOE);                                  /* 从 FRAM 硬件中读取之前保存的 SOE 数据到 fram_soe 缓存 */

    pSoeValues->averageSoe_perc[stringNumber]              = fram_soe.averageSoe_perc[stringNumber]; /* 用 FRAM 中存储的平均 SOE 初始化数据库值 */
    pSoeValues->minimumSoe_perc[stringNumber]              = fram_soe.minimumSoe_perc[stringNumber]; /* 用 FRAM 中存储的最小 SOE 初始化数据库值 */
    pSoeValues->maximumSoe_perc[stringNumber]              = fram_soe.maximumSoe_perc[stringNumber]; /* 用 FRAM 中存储的最大 SOE 初始化数据库值 */
    pSoeValues->chargeEnergyThroughput_Wh[stringNumber]    = fram_soe.chargeEnergyThroughput_Wh[stringNumber]; /* 用 FRAM 中存储的充电能量吞吐量初始化数据库值 */
    pSoeValues->dischargeEnergyThroughput_Wh[stringNumber] = fram_soe.dischargeEnergyThroughput_Wh[stringNumber]; /* 用 FRAM 中存储的放电能量吞吐量初始化数据库值 */

    /* 限制 SOE 值在 [0.0f, 100.0f] 范围内 */
    SOE_CheckDatabaseSoePercentageLimits(pSoeValues, stringNumber);    /* 检查并限幅初始化后的 SOE 值，确保在 [0, 100] 范围内 */

    /* 计算串能量 */
    pSoeValues->maximumSoe_Wh[stringNumber] =                          /* 将最大 SOE 百分比转换为能量 */
        SOE_GetStringEnergyFromSoePercentage(pSoeValues->maximumSoe_perc[stringNumber]); /* 调用转换函数并赋值 */
    pSoeValues->minimumSoe_Wh[stringNumber] =                          /* 将最小 SOE 百分比转换为能量 */
        SOE_GetStringEnergyFromSoePercentage(pSoeValues->minimumSoe_perc[stringNumber]); /* 调用转换函数并赋值 */
    pSoeValues->averageSoe_Wh[stringNumber] =                          /* 将平均 SOE 百分比转换为能量 */
        SOE_GetStringEnergyFromSoePercentage(pSoeValues->averageSoe_perc[stringNumber]); /* 调用转换函数并赋值 */

    if (ec_present == true) {                                          /* 如果当前串存在能量计数功能 */
        DATA_READ_DATA(&soe_tableEnergyCounter);                      /* 从数据库读取能量计数器数据 */
        soe_state.sensorEcUsed[stringNumber] = true;                   /* 标记该串使用了能量计数功能 */

        /* 设置缩放值 */
        float_t ecOffset =                                             /* 计算当前能量计数值对应的 SOE 百分比偏移量 */
            SOE_GetStringSoePercentageFromEnergy((uint32_t)abs(soe_tableEnergyCounter.energyCounter_Wh[stringNumber])); /* 取能量计数值的绝对值进行转换 */

        if (soe_tableEnergyCounter.energyCounter_Wh[stringNumber] < 0) { /* 如果能量计数值为负数 */
            ecOffset *= (-1.0f);                                       /* 则将偏移量取反，保持符号一致 */
        }

        ecOffset *= BS_CURRENT_DIRECTION_FLOAT;                        /* 根据系统电流方向定义调整偏移量符号，对计算出的百分比增量取反 */

        soe_state.ecScalingMinimum[stringNumber] = fram_soe.minimumSoe_perc[stringNumber] + ecOffset; /* 初始化最小 SOE 的能量计数缩放基准：FRAM 中的 SOE + 偏移量 */
        soe_state.ecScalingMaximum[stringNumber] = fram_soe.maximumSoe_perc[stringNumber] + ecOffset; /* 初始化最大 SOE 的能量计数缩放基准：FRAM 中的 SOE + 偏移量 */
        soe_state.ecScalingAverage[stringNumber] = fram_soe.averageSoe_perc[stringNumber] + ecOffset; /* 初始化平均 SOE 的能量计数缩放基准：FRAM 中的 SOE + 偏移量 */
    }
    soe_state.soeInitialized = true;                                   /* 将 SOE 模块初始化标志位置为 true，表示初始化完成 */
}

void SE_CalculateStateOfEnergy(DATA_BLOCK_SOE_s *pSoeValues) {         /* 函数定义：周期性调用以计算电池的 SOE 状态 */
    FAS_ASSERT(pSoeValues != NULL_PTR);                                /* 断言检查：确保指针不为空 */
    bool continueFunction = true;                                      /* 定义并初始化继续执行函数的标志位 */
    if (soe_state.soeInitialized == false) {                           /* 检查 SOE 模块是否已初始化 */
        /* 如果 SOE 尚未初始化则退出 */
        continueFunction = false;                                      /* 如果未初始化，则将标志位置为 false，退出后续计算 */
    }

    if (continueFunction == true) {                                    /* 如果允许继续执行 */
        /* 使用能量计数/积分 */
        DATA_READ_DATA(&soe_tableCurrent, &soe_tableSystemVoltage1, &soe_tableEnergyCounter); /* 从数据库读取最新的电流值、系统电压值和能量计数值 */

        if (BMS_GetBatterySystemState() == BMS_AT_REST) {              /* 检查电池系统当前是否处于静置状态 */
            /* 通过查找表重新校准 SOE */
            SOE_RecalibrateViaLookupTable(pSoeValues);                 /* 如果是静置状态，利用开路电压查表法重新校准 SOE，消除累积误差 */
        } else {                                                       /* 如果电池系统处于工作状态(正在充放电) */
            for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {          /* 遍历所有电池串 */

                /* ==================================================================================== */
                /* =================== 分支一：瓦时积分法 (软件侧电压电流积分) =================== */
                /* ==================================================================================== */
                if (soe_state.sensorEcUsed[s] == false) {               /* 判断该串是否未使用传感器硬件能量计数(即使用软件瓦时积分法) */
                    /* 未激活能量计数 -> 手动积分能量 */
                    uint32_t timestamp          = soe_tableCurrent.timestamp[s]; /* 获取当前电流测量的时间戳 */
                    uint32_t previous_timestamp = soe_tableCurrent.previousTimestamp[s]; /* 获取上一次电流测量的时间戳 */

                    /* 检查电流测量是否已更新 */
                    if (soe_state.previousTimestamp[s] != timestamp) {  /* 检查时间戳是否更新，避免重复计算 */
                        float_t time_step_s = (((float_t)timestamp - (float_t)previous_timestamp)) / 1000.0f; /* 【瓦时积分-步长】计算时间步长，将毫秒转换为秒 */
                        if (time_step_s > 0.0f) {                       /* 确保时间步长有效(大于 0) */
                            /* 充电方向电流为负意味着 SOE 增加 --> BAT 命名，而非 ROB */

                            float_t deltaSOE_Wh =                      /* 【瓦时积分-核心】计算能量变化量(瓦时) */
                                ((((float_t)soe_tableCurrent.current_mA[s] / 1000.0f) *             /* 电流 mA 转换为 A */
                                  ((float_t)soe_tableSystemVoltage1.highVoltage_mV[s] / 1000.0f)) / /* 电压 mV 转换为 V */
                                 time_step_s) /                                                     /* 单位：s (功率 = 电压 * 电流，单位 W) */
                                3600.0f;                              /* 将 Ws(瓦秒=焦耳) 转换为 Wh(瓦时)，除以 3600 */

                            deltaSOE_Wh *= BS_CURRENT_DIRECTION_FLOAT; /* 根据系统电流方向定义修正 SOE 变化量的符号 */

                            pSoeValues->averageSoe_Wh[s] -= (uint32_t)deltaSOE_Wh; /* 【瓦时积分-累加】累加平均 SOE 能量：原值减去变化量(强制转换为整型) */
                            pSoeValues->minimumSoe_Wh[s] -= (uint32_t)deltaSOE_Wh; /* 【瓦时积分-累加】累加最小 SOE 能量：原值减去变化量 */
                            pSoeValues->maximumSoe_Wh[s] -= (uint32_t)deltaSOE_Wh; /* 【瓦时积分-累加】累加最大 SOE 能量：原值减去变化量 */

                            if (BMS_GetCurrentFlowDirection(soe_tableCurrent.current_mA[s]) == BMS_CHARGING) { /* 判断当前电流方向是否为充电 */
                                pSoeValues->chargeEnergyThroughput_Wh[s] = pSoeValues->chargeEnergyThroughput_Wh[s] +
                                                                           fabs(deltaSOE_Wh); /* 如果是充电，累加充电能量吞吐量绝对值 */
                            } else {                                   /* 如果不是充电(放电或静置) */
                                /* 当 BMS_DISCHARGING 和 BMS_AT_REST 时，将电荷加到 dischargeThroughput*/
                                pSoeValues->dischargeEnergyThroughput_Wh[s] =
                                    pSoeValues->dischargeEnergyThroughput_Wh[s] + fabs(deltaSOE_Wh); /* 如果是放电或静置，累加放电能量吞吐量绝对值 */
                            }

                            pSoeValues->averageSoe_perc[s] =                              /* 将累加后的平均 SOE 能量转换为百分比 */
                                SOE_GetStringSoePercentageFromEnergy(pSoeValues->averageSoe_Wh[s]); /* 调用转换函数并赋值 */
                            pSoeValues->minimumSoe_perc[s] =                              /* 将累加后的最小 SOE 能量转换为百分比 */
                                SOE_GetStringSoePercentageFromEnergy(pSoeValues->minimumSoe_Wh[s]); /* 调用转换函数并赋值 */
                            pSoeValues->maximumSoe_perc[s] =                              /* 将累加后的最大 SOE 能量转换为百分比 */
                                SOE_GetStringSoePercentageFromEnergy(pSoeValues->maximumSoe_Wh[s]); /* 调用转换函数并赋值 */

                            /* 更新时间戳 SOE 状态变量以供下次迭代 */
                            soe_state.previousTimestamp[s] = timestamp; /* 更新时间戳，为下一次计算做准备 */
                        }
                    } /* 检查电流测量是否已更新结束 */

                /* ==================================================================================== */
                /* ============== 分支二：能量计算法 (硬件传感器能量计直接读取) ============== */
                /* ==================================================================================== */
                } else {                                               /* 如果该串使用了传感器硬件能量计数功能 */
                    /* 检查能量计数测量是否已更新 */
                    if (soe_state.previousTimestamp[s] != soe_tableEnergyCounter.timestamp[s]) { /* 检查能量计数器的时间戳是否更新 */
                        /* 使用电流传感器 EC 值计算 SOE 值 */
                        float_t deltaSoe_perc =                        /* 【能量计算-核心】计算基于能量计数值的 SOE 百分比 */
                            (((float_t)soe_tableEnergyCounter.energyCounter_Wh[s] / SOE_STRING_ENERGY_Wh) *
                             UNIT_CONVERSION_FACTOR_100_FLOAT);        /* 核心公式：(当前能量计数值 Wh / 串总能量 Wh) * 100% */

                        float_t energyDifference_Wh = fabs(             /* 【能量计算-差值】计算与上次相比的能量差值绝对值，用于吞吐量统计 */
                            (float_t)soe_tableEnergyCounter.energyCounter_Wh[s] - soe_state.previousEnergyCount_Wh[s]); /* 当前能量计数值 - 上次能量计数值 */

                        deltaSoe_perc *= BS_CURRENT_DIRECTION_FLOAT;   /* 根据系统电流方向定义修正 SOE 的符号 */

                        /* 应用 EC 缩放偏移以获取实际的串能量 */
                        pSoeValues->averageSoe_perc[s] = soe_state.ecScalingAverage[s] - deltaSoe_perc; /* 【能量计算-赋值】计算平均 SOE：缩放基准值 - 当前能量计 SOE */
                        pSoeValues->minimumSoe_perc[s] = soe_state.ecScalingMinimum[s] - deltaSoe_perc; /* 【能量计算-赋值】计算最小 SOE：缩放基准值 - 当前能量计 SOE */
                        pSoeValues->maximumSoe_perc[s] = soe_state.ecScalingMaximum[s] - deltaSoe_perc; /* 【能量计算-赋值】计算最大 SOE：缩放基准值 - 当前能量计 SOE */
                        if (BMS_GetCurrentFlowDirection(soe_tableCurrent.current_mA[s]) == BMS_CHARGING) { /* 判断当前电流方向是否为充电 */
                            pSoeValues->chargeEnergyThroughput_Wh[s] = pSoeValues->chargeEnergyThroughput_Wh[s] +
                                                                       energyDifference_Wh; /* 如果是充电，累加充电能量吞吐量差值 */
                        } else {                                       /* 如果不是充电(放电或静置) */
                            /* 当 BMS_DISCHARGING 和 BMS_AT_REST 时，将电荷加到 dischargeThroughput*/
                            pSoeValues->dischargeEnergyThroughput_Wh[s] = pSoeValues->dischargeEnergyThroughput_Wh[s] +
                                                                          energyDifference_Wh; /* 如果是放电或静置，累加放电能量吞吐量差值 */
                        }
                        /* 将 SOE 值限制在 [0.0, 100.0] */
                        SOE_CheckDatabaseSoePercentageLimits(pSoeValues, s); /* 检查并将计算后的 SOE 值限幅在 [0%, 100%] 范围内 */

                        /* 计算新的 Wh 值 */
                        pSoeValues->maximumSoe_Wh[s] =                  /* 将最大 SOE 百分比转换为能量 */
                            SOE_GetStringEnergyFromSoePercentage(pSoeValues->maximumSoe_perc[s]); /* 调用转换函数并赋值 */
                        pSoeValues->averageSoe_Wh[s] =                  /* 将平均 SOE 百分比转换为能量 */
                            SOE_GetStringEnergyFromSoePercentage(pSoeValues->averageSoe_perc[s]); /* 调用转换函数并赋值 */
                        pSoeValues->minimumSoe_Wh[s] =                  /* 将最小 SOE 百分比转换为能量 */
                            SOE_GetStringEnergyFromSoePercentage(pSoeValues->minimumSoe_perc[s]); /* 调用转换函数并赋值 */

                        /* 更新时间戳以供下次迭代 */
                        soe_state.previousEnergyCount_Wh[s] = soe_tableEnergyCounter.energyCounter_Wh[s]; /* 更新上一次能量计数值，为下次计算差值做准备 */
                        soe_state.previousTimestamp[s]      = soe_tableEnergyCounter.timestamp[s]; /* 更新时间戳，为下次判断是否更新做准备 */
                    }
                }

                fram_soe.averageSoe_perc[s]              = pSoeValues->averageSoe_perc[s]; /* 将数据库中的平均 SOE 拷贝到 FRAM 缓存 */
                fram_soe.minimumSoe_perc[s]              = pSoeValues->minimumSoe_perc[s]; /* 将数据库中的最小 SOE 拷贝到 FRAM 缓存 */
                fram_soe.maximumSoe_perc[s]              = pSoeValues->maximumSoe_perc[s]; /* 将数据库中的最大 SOE 拷贝到 FRAM 缓存 */
                fram_soe.chargeEnergyThroughput_Wh[s]    = pSoeValues->chargeEnergyThroughput_Wh[s]; /* 将数据库中的充电能量吞吐量拷贝到 FRAM 缓存 */
                fram_soe.dischargeEnergyThroughput_Wh[s] = pSoeValues->dischargeEnergyThroughput_Wh[s]; /* 将数据库中的放电能量吞吐量拷贝到 FRAM 缓存 */
            }

            /* 更新数据库和 FRAM 值 */
            FRAM_WriteData(FRAM_BLOCK_ID_SOE);                         /* 遍历完所有串后，将 FRAM 缓存中的数据实际写入 FRAM 硬件 */
        }
    }
}

/*========== 外部化的静态函数实现 (单元测试) =======*/
#ifdef UNITY_UNIT_TEST                                                 /* 如果定义了 UNITY_UNIT_TEST 宏，用于单元测试 */
#endif                                                                 /* 本文件无导出的测试函数 */

