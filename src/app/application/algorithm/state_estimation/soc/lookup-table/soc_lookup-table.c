/**
 *
 * @copyright &copy; 2010 - 2026, Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 * All rights reserved.
 * ... (版权及许可证声明省略) ...
 */

/**
 * @file    soc_lookup-table.c
 * @author  foxBMS Team
 * @date    2025-06-02 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup APPLICATION
 * @prefix  SOC
 *
 * @brief   SOC module responsible for calculation of state-of-charge (SOC)
 * @details 该模块负责通过查表法（Lookup Table）和线性插值计算电池的荷电状态(SOC)
 */

/*========== Includes =======================================================*/
#include "general.h"                     /* 通用系统配置及宏定义 */

#include "soc_lookup-table_cfg.h"        /* SOC查表法模块的配置头文件 */

#include "bms.h"                         /* BMS全局定义，如串数等 */
#include "database.h"                    /* 数据库接口，用于读写共享数据 */
#include "foxmath.h"                     /* 数学工具库，如插值算法等 */
#include "fram.h"                        /* FRAM(铁电存储器)驱动，用于非易失性存储 */
#include "state_estimation.h"            /* 状态估计模块头文件 */

#include <math.h>                        /* 标准数学库 */
#include <stdbool.h>                     /* 布尔类型支持 */
#include <stdint.h>                      /* 标准整型定义 */

/*========== Macros and Definitions =========================================*/
/**
 * @brief SOC模块的内部状态结构体
 * 保存SOC计算过程中的中间变量、传感器状态和吞吐量统计等
 */
typedef struct {
    bool socInitialized;                 /*!< SOC模块是否已初始化的标志位 */
    bool sensorCcUsed[BS_NR_OF_STRINGS]; /*!< 当前串是否使用了电流传感器的库仑计数功能 */
    float_t ccScalingAverage[BS_NR_OF_STRINGS];       /*!< 电流传感器平均SOC的偏移缩放系数 */
    float_t ccScalingMinimum[BS_NR_OF_STRINGS];       /*!< 电流传感器最小SOC的偏移缩放系数 */
    float_t ccScalingMaximum[BS_NR_OF_STRINGS];       /*!< 电流传感器最大SOC的偏移缩放系数 */
    float_t chargeThroughput_As[BS_NR_OF_STRINGS];    /*!< 累计充电吞吐量(安培秒) */
    float_t dischargeThroughput_As[BS_NR_OF_STRINGS]; /*!< 累计放电吞吐量(安培秒) */
    float_t previousCurrentCountingValue_As[BS_NR_OF_STRINGS]; /*!< 上一次电流计数的值，用于计算差分增量 */
    uint32_t previousTimestamp; /*!< 上一次更新的时间戳，用于判断电压数据是否已刷新 */
} SOC_STATE_s;

/** 本地数据库表的副本 */
/**@{*/
static DATA_BLOCK_MIN_MAX_s soc_tableMinMax = {.header.uniqueId = DATA_BLOCK_ID_MIN_MAX};
/**@}*/

/** SOC的最大百分比限制 */
#define SOC_MAXIMUM_SOC_perc (100.0f)
/** SOC的最小百分比限制 */
#define SOC_MINIMUM_SOC_perc (0.0f)

/*========== Static Constant and Variable Definitions =======================*/
/**
 * @brief SOC模块的状态实例，静态局部变量，仅在本文本可见
 * 初始化所有成员变量，使用了GEN_REPEAT_U宏来批量初始化数组
 */
static SOC_STATE_s soc_state = {
    .socInitialized                  = false,                                     /* 初始化标记设为假 */
    .sensorCcUsed                    = {GEN_REPEAT_U(false, GEN_STRIP(BS_NR_OF_STRINGS))}, /* 库仑计数使用标志初始化为假 */
    .ccScalingAverage                = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 缩放系数初始化为0.0 */
    .ccScalingMinimum                = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 缩放系数初始化为0.0 */
    .ccScalingMaximum                = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 缩放系数初始化为0.0 */
    .chargeThroughput_As             = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 充电吞吐量初始化为0.0 */
    .dischargeThroughput_As          = {GEN_REPEAT_U(0.0f, GEN_STRIP(BS_NR_OF_STRINGS))},  /* 放电吞吐量初始化为0.0 */
    .previousCurrentCountingValue_As = {GEN_REPEAT_U(0u, GEN_STRIP(BS_NR_OF_STRINGS))},    /* 上次电流值初始化为0 */
    .previousTimestamp               = 0u,                                        /* 时间戳初始化为0 */
};

/*========== Extern Constant and Variable Definitions =======================*/
/* 外部常量和变量定义（本文件无） */

/*========== Static Function Prototypes =====================================*/
/**
 * @brief   检查数据库中的所有SOC百分比值是否在 [0.0, 100.0] 范围内
 *          如果超出此范围，则将SOC值限制在边界值。
 * @param[in,out] pTableSoc  指向包含SOC值的数据库结构体的指针
 * @param[in] stringNumber   需要检查的电池串编号
 */
static void SOC_CheckDatabaseSocPercentageLimits(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber);

/**
 * @brief   将SOC相关的值更新到非易失性存储器(FRAM)中，防止掉电丢失
 * @param[in] pTableSoc      指向包含SOC值的数据库结构体的指针
 * @param[in] stringNumber   需要更新的电池串编号
 */
static void SOC_UpdateNvmValues(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber);

/*========== Static Function Implementations ================================*/
/**
 * @brief   检查并限幅SOC百分比，确保其不超过0%~100%
 */
static void SOC_CheckDatabaseSocPercentageLimits(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber) {
    FAS_ASSERT(pTableSoc != NULL_PTR);               /* 断言：指针不为空 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);     /* 断言：串号在有效范围内 */

    /* 限制平均SOC：超过100%则置为100%，低于0%则置为0% */
    if (pTableSoc->averageSoc_perc[stringNumber] > SOC_MAXIMUM_SOC_perc) {
        pTableSoc->averageSoc_perc[stringNumber] = SOC_MAXIMUM_SOC_perc;
    }
    if (pTableSoc->averageSoc_perc[stringNumber] < SOC_MINIMUM_SOC_perc) {
        pTableSoc->averageSoc_perc[stringNumber] = SOC_MINIMUM_SOC_perc;
    }

    /* 限制最小SOC：超过100%则置为100%，低于0%则置为0% */
    if (pTableSoc->minimumSoc_perc[stringNumber] > SOC_MAXIMUM_SOC_perc) {
        pTableSoc->minimumSoc_perc[stringNumber] = SOC_MAXIMUM_SOC_perc;
    }
    if (pTableSoc->minimumSoc_perc[stringNumber] < SOC_MINIMUM_SOC_perc) {
        pTableSoc->minimumSoc_perc[stringNumber] = SOC_MINIMUM_SOC_perc;
    }

    /* 限制最大SOC：超过100%则置为100%，低于0%则置为0% */
    if (pTableSoc->maximumSoc_perc[stringNumber] > SOC_MAXIMUM_SOC_perc) {
        pTableSoc->maximumSoc_perc[stringNumber] = SOC_MAXIMUM_SOC_perc;
    }
    if (pTableSoc->maximumSoc_perc[stringNumber] < SOC_MINIMUM_SOC_perc) {
        pTableSoc->maximumSoc_perc[stringNumber] = SOC_MINIMUM_SOC_perc;
    }
}

/**
 * @brief   将当前计算出的SOC及吞吐量数据备份到FRAM缓存中
 */
static void SOC_UpdateNvmValues(DATA_BLOCK_SOC_s *pTableSoc, uint8_t stringNumber) {
    FAS_ASSERT(pTableSoc != NULL_PTR);               /* 断言：指针不为空 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);     /* 断言：串号在有效范围内 */
    
    /* 将pTableSoc中的值拷贝到全局的fram_soc结构体中，等待写入FRAM硬件 */
    fram_soc.averageSoc_perc[stringNumber]        = pTableSoc->averageSoc_perc[stringNumber];
    fram_soc.minimumSoc_perc[stringNumber]        = pTableSoc->minimumSoc_perc[stringNumber];
    fram_soc.maximumSoc_perc[stringNumber]        = pTableSoc->maximumSoc_perc[stringNumber];
    fram_soc.chargeThroughput_As[stringNumber]    = pTableSoc->chargeThroughput_As[stringNumber];
    fram_soc.dischargeThroughput_As[stringNumber] = pTableSoc->dischargeThroughput_As[stringNumber];
}

/*========== Extern Function Implementations ================================*/
/**
 * @brief   初始化SOC计算模块
 */
extern void SE_InitializeStateOfCharge(DATA_BLOCK_SOC_s *pSocValues, bool ccPresent, uint8_t stringNumber) {
    FAS_ASSERT(pSocValues != NULL_PTR);                              /* 断言：指针不为空 */
    FAS_ASSERT((ccPresent == true) || (ccPresent == false));         /* 断言：布尔值合法性检查 */
    FAS_ASSERT(stringNumber < BS_NR_OF_STRINGS);                    /* 断言：串号合法性 */
    
    soc_state.socInitialized = true; /* 将初始化标志位置为true，表示SOC模块已准备好进行计算 */
}

/**
 * @brief   周期性调用以计算电池的SOC状态
 * @details 通过读取最新电压，利用OCV查表计算最大/最小/平均SOC
 */
extern void SE_CalculateStateOfCharge(DATA_BLOCK_SOC_s *pSocValues) {
    FAS_ASSERT(pSocValues != NULL_PTR); /* 断言：指针不为空 */
    
    bool continueFunction = true;       /* 控制流程是否继续的标志位 */

    /* 检查SOC是否已初始化 */
    if (soc_state.socInitialized == false) {
        /* 如果未初始化，则退出函数。在此处初始化真的有必要吗？(原作者留下的疑问) */
        /* continueFunction = false; */ 
    }

    /* 如果允许继续执行 */
    if (continueFunction == true) {
        /* 从数据库读取最新的最小/最大/平均电压值 */
        DATA_READ_DATA(&soc_tableMinMax);
        
        /* 遍历所有电池串 */
        for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
            /* 检查时间戳是否更新，确保只有在底层数据刷新时才重新计算，避免无意义的重复计算 */
            if (soc_state.previousTimestamp != soc_tableMinMax.header.timestamp) {
                /* 更新时间戳为最新值 */
                soc_state.previousTimestamp = soc_tableMinMax.header.timestamp;

                /* 根据最高单体电压查表计算该串的最大SOC */
                pSocValues->maximumSoc_perc[s] =
                    SE_GetStateOfChargeFromVoltage(soc_tableMinMax.maximumCellVoltage_mV[s]);
                /* 根据最低单体电压查表计算该串的最小SOC */
                pSocValues->minimumSoc_perc[s] =
                    SE_GetStateOfChargeFromVoltage(soc_tableMinMax.minimumCellVoltage_mV[s]);
                /* 根据平均单体电压查表计算该串的平均SOC */
                pSocValues->averageSoc_perc[s] =
                    SE_GetStateOfChargeFromVoltage(soc_tableMinMax.averageCellVoltage_mV[s]);

                /* 将计算出的SOC值限幅在 [0.0, 100.0] 范围内 */
                SOC_CheckDatabaseSocPercentageLimits(pSocValues, s);

                /* 更新FRAM中的非易失性SOC值，防止掉电丢失 */
                SOC_UpdateNvmValues(pSocValues, s);
            }
        }
        /* 将FRAM缓存中的数据实际写入到FRAM硬件中 */
        FRAM_WriteData(FRAM_BLOCK_ID_SOC);
    }
}

/**
 * @brief   根据单体电压通过查表和线性插值计算SOC
 * @param   voltage_mV 当前单体电压，单位毫伏
 * @return  估算出的SOC百分比
 */
extern float_t SE_GetStateOfChargeFromVoltage(int16_t voltage_mV) {
    float_t soc_perc = 0.50f; /* 默认返回0.5(50%)，作为异常情况下的安全返回值 */

    /* 用于插值的查找表索引边界 */
    uint16_t between_high = 0; /* 指向高电压(低SOC)的索引 */
    uint16_t between_low  = 0; /* 指向低电压(高SOC)的索引 */

    /* 
     * 在查找表(LUT)中寻找输入电压所在区间。
     * 注意：查找表中的电压是降序排列的（即索引0电压最高，SOC最高），
     * 因此从 i=1 开始遍历，避免向外 extrapolate(外推)。
     */
    for (uint16_t i = 1u; i < bc_stateOfChargeLookupTableLength; i++) {
        /* 如果输入电压小于当前LUT项的电压 */
        if (voltage_mV < bc_stateOfChargeLookupTable[i].voltage_mV) {
            between_low  = i + 1u; /* 输入电压必定在比当前索引更小的位置(更低电压) */
            between_high = i;      /* 当前索引作为高电压边界 */
        }
    }

    /* 在LUT值之间进行线性插值，但不进行外推！ */
    if (!(((between_high == 0u) && (between_low == 0u)) ||       /* 情况1：cell电压 > LUT中的最大电压 */
          (between_low >= bc_stateOfChargeLookupTableLength))) { /* 情况2：cell电压 < LUT中的最小电压 */
        
        /* 正常区间内，调用数学库的线性插值函数计算SOC */
        soc_perc = MATH_LinearInterpolation(
            (float_t)bc_stateOfChargeLookupTable[between_low].voltage_mV,   /* 区间低端电压(X1) */
            bc_stateOfChargeLookupTable[between_low].value,                 /* 区间低端SOC(Y1) */
            (float_t)bc_stateOfChargeLookupTable[between_high].voltage_mV,  /* 区间高端电压(X2) */
            bc_stateOfChargeLookupTable[between_high].value,                /* 区间高端SOC(Y2) */
            (float_t)voltage_mV);                                           /* 输入电压 */
            
    } else if ((between_low >= bc_stateOfChargeLookupTableLength)) {
        /* LUT中的SOC值是降序排列的：如果cell电压 < LUT中的最小电压，说明电池已彻底没电 */
        soc_perc = SOC_MINIMUM_SOC_perc; /* 返回 0% */
    } else {
        /* 如果cell电压 > LUT中的最大电压，说明电池已完全充满 */
        soc_perc = SOC_MAXIMUM_SOC_perc; /* 返回 100% */
    }
    
    return soc_perc; /* 返回计算出的SOC百分比 */
}

/*========== Externalized Static Function Implementations (Unit Test) =======*/
/* 单元测试专用代码：在UNITY_UNIT_TEST宏定义下，将static函数导出供测试框架调用 */
#ifdef UNITY_UNIT_TEST
extern bool TEST_SE_GetSocStateInitialized(void) {
    return soc_state.socInitialized;
}
extern void TEST_SOC_CheckDatabaseSocPercentageLimits(DATA_BLOCK_SOC_s *TableSoc, uint8_t stringNumber) {
    SOC_CheckDatabaseSocPercentageLimits(TableSoc, stringNumber);
}
extern void TEST_SOC_UpdateNvmValues(DATA_BLOCK_SOC_s *TableSoc, uint8_t stringNumber) {
    SOC_UpdateNvmValues(TableSoc, stringNumber);
}
#endif
