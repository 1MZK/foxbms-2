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
 * @file    fram_cfg.h
 * @author  foxBMS 团队
 * @date    2020-03-05 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup DRIVERS
 * @prefix  FRAM
 *
 * @brief   FRAM 模块配置的头文件
 * @details 待办
 */

#ifndef FOXBMS__FRAM_CFG_H_                                           /* 防止重复包含宏：开始 */
#define FOXBMS__FRAM_CFG_H_

/*========== 包含文件 =======================================================*/

#include "battery_system_cfg.h"                                        /* 包含电池系统配置，如 BS_NR_OF_STRINGS */

#include "fstd_types.h"                                                /* 包含标准类型定义，如 STD_RETURN_TYPE_e */

#include <math.h>                                                      /* 包含标准数学库 */
#include <stdbool.h>                                                   /* 包含布尔类型支持 */
#include <stdint.h>                                                    /* 包含标准整型定义 */

/*========== 宏和定义 =========================================================*/

/* 每个条目的头部由 4 字节 SPI 头 + 8 字节 CRC 组成 */
#define FRAM_CRC_HEADER_SIZE (sizeof(uint64_t))                        /* 定义 FRAM CRC 头部大小为 64 位(8 字节)的无符号整型大小 */

/**
 * @brief   使用内存布局的项目的 ID
 * @details 此枚举可用于区分不同项目。虽然同一项目的旧版本
 *          应该是可以升级的，但如果发生冲突，另一个项目的条目
 *          应该直接被丢弃。
 *
 *          为了保持 ID 不变，重要的是不要更改定义的宏，并且每个宏
 *          必须有不同的值。
 */
typedef uint16_t FRAM_PROJECT_ID;                                      /* 将 FRAM_PROJECT_ID 定义为 uint16_t 类型，用于标识项目 */

/** 这是标准的主开发分支 */
#define FRAM_PROJECT_ID_FOXBMS_BASELINE ((FRAM_PROJECT_ID)0u)         /* 定义 foxBMS 基线项目的 ID 为 0 */

/** FRAM 块识别号 (访问返回类型) */
typedef enum {
    FRAM_ACCESS_OK,                                                    /* 与 FRAM 的事务成功 */
    FRAM_ACCESS_SPI_BUSY,                                              /* SPI 忙，无法进行 FRAM 事务 */
    FRAM_ACCESS_CRC_BUSY,                                              /* CRC 硬件忙，无法进行 FRAM 事务 */
    FRAM_ACCESS_CRC_ERROR,                                             /* 读取的 CRC 与根据读取数据计算的 CRC 不匹配 */
} FRAM_RETURN_TYPE_e;                                                  /* 定义 FRAM 访问返回类型的枚举 */

/** 数据库通道(数据块)的配置结构体 */
typedef struct {
    void *blockptr;                                                    /* 指向数据块的指针 */
    uint32_t datalength;                                               /* 数据块的长度 */
    uint32_t address;                                                  /* 数据块在 FRAM 中的地址 */
} FRAM_BASE_HEADER_s;                                                  /* 定义 FRAM 基础头结构体 */

/** FRAM 块识别号 (数据块 ID) */
typedef enum {
    FRAM_BLOCK_ID_VERSION,                                             /* 版本信息块 ID */
    FRAM_BLOCK_ID_SOC,                                                 /* SOC(荷电状态)数据块 ID */
    FRAM_BLOCK_ID_SBC_INIT_STATE,                                      /* SBC(系统基础芯片)初始化状态块 ID */
    FRAM_BLOCK_ID_DEEP_DISCHARGE_FLAG,                                 /* 深度放电标志块 ID */
    FRAM_BLOCK_ID_SOE,                                                 /* SOE(能量状态)数据块 ID */
    FRAM_BLOCK_ID_SYS_MON_RECORD,                                      /* 系统监控记录块 ID */
    FRAM_BLOCK_ID_INSULATION_FLAG,                                     /* 绝缘接地故障标志块 ID */
    FRAM_BLOCK_MAX,                                                    /**< 不要更改，必须是最后一个条目 */ /* 用于循环遍历的最大值，必须放在最后 */
} FRAM_BLOCK_ID_e;                                                     /* 定义 FRAM 数据块 ID 的枚举 */

FAS_STATIC_ASSERT(((uint32_t)FRAM_BLOCK_MAX < (uint32_t)UINT8_MAX), "Looping over 'FRAM_BLOCK_MAX' assumes 'uint8_t'."); /* 静态断言：确保 FRAM_BLOCK_MAX 小于 255，因为遍历时使用 uint8_t */

/**
 * @brief   存储 FRAM 内存布局的版本
 * @details 此结构体存储 FRAM 是用哪个内存布局版本写入的。
 *          这允许 BMS 识别不兼容的内存布局。
 */
typedef struct {
    FRAM_PROJECT_ID project;                                           /*!< 项目的标识符，不打算在不同项目之间迁移 */
    uint8_t major;                                                      /*!< 主发布版本 */
    uint8_t minor;                                                      /*!< 次发布版本 */
    uint8_t patch;                                                      /*!< 补丁版本 */
} FRAM_VERSION_s;                                                      /* 定义 FRAM 版本信息结构体 */

/** SBC 驱动的 FRAM 条目的结构体 */
typedef struct {
    uint8_t phase;                                                      /* 阶段 */
    STD_RETURN_TYPE_e finState;                                         /* 最终状态 */
} FRAM_SBC_INIT_s;                                                     /* 定义 FRAM 中 SBC 初始化状态的结构体 */

/**
 * 荷电状态 (SOC)。由于 SOC 依赖于电压，因此使用三个不同的
 * 值：最小值、最大值和平均值。SOC 定义为 0.0f 和 100.0f 之间的
 * float_t 数字 (0% 和 100%)
 */
typedef struct {
    float_t minimumSoc_perc[BS_NR_OF_STRINGS];                         /*!< 最小 SOC */
    float_t maximumSoc_perc[BS_NR_OF_STRINGS];                         /*!< 最大 SOC */
    float_t averageSoc_perc[BS_NR_OF_STRINGS];                         /*!< 平均 SOC */
    float_t chargeThroughput_As[BS_NR_OF_STRINGS];                     /*!<  充电吞吐量(安培秒) */
    float_t dischargeThroughput_As[BS_NR_OF_STRINGS];                  /*!< 放电吞吐量(安培秒) */
} FRAM_SOC_s;                                                          /* 定义存储在 FRAM 中的 SOC 数据结构体 */

/**
 * 能量状态 (SOE)。由于 SOE 依赖于电压，因此使用三个不同的
 * 值：最小值、最大值和平均值。SOE 定义为 0.0f 和 100.0f 之间的
 * float_t 数字 (0% 和 100%)
 */
typedef struct {
    float_t minimumSoe_perc[BS_NR_OF_STRINGS];                         /*!< 最小 SOE */
    float_t maximumSoe_perc[BS_NR_OF_STRINGS];                         /*!< 最大 SOE */
    float_t averageSoe_perc[BS_NR_OF_STRINGS];                         /*!< 平均 SOE */
    float_t chargeEnergyThroughput_Wh[BS_NR_OF_STRINGS];               /*!< 流入的能量(充电吞吐量，瓦时) */
    float_t dischargeEnergyThroughput_Wh[BS_NR_OF_STRINGS];            /*!< 流出的能量(放电吞吐量，瓦时) */
} FRAM_SOE_s;                                                          /* 定义存储在 FRAM 中的 SOE 数据结构体 */

/** 用于指示是否检测到某串发生深度放电的标志 */
typedef struct {
    bool deepDischargeFlag[BS_NR_OF_STRINGS];                          /*!< false (0): 无错误，true (1): 检测到深度放电 */
} FRAM_DEEP_DISCHARGE_FLAG_s;                                          /* 定义存储在 FRAM 中的深度放电标志结构体 */

/** 用于指示是否检测到绝缘接地故障的标志 */
typedef struct {
    bool groundErrorDetected;                                          /*!< false (0): 无错误，true (1): 检测到接地故障 */
} FRAM_INSULATION_FLAG_s;                                              /* 定义存储在 FRAM 中的绝缘故障标志结构体 */

/**
 * @brief 存储每个任务最后一次时序违规的结构体
 */
typedef struct {
    /** 只要记录了任何时序问题就会设置的便利标志 */
    bool anyTimingIssueOccurred;                                       /* 任何时序问题发生的标志 */
    /** 发生最后一次时序违规时记录的持续时间 */
    uint32_t taskEngineViolatingDuration;                              /* 引擎任务违规持续时间 */
    /** 进入违规执行任务时记录的时间戳 */
    uint32_t taskEngineEnterTimestamp;                                 /* 引擎任务进入时间戳 */
    /** 发生最后一次时序违规时记录的持续时间 */
    uint32_t task1msViolatingDuration;                                 /* 1毫秒任务违规持续时间 */
    /** 进入违规执行任务时记录的时间戳 */
    uint32_t task1msEnterTimestamp;                                    /* 1毫秒任务进入时间戳 */
    /** 发生最后一次时序违规时记录的持续时间 */
    uint32_t task10msViolatingDuration;                                /* 10毫秒任务违规持续时间 */
    /** 进入违规执行任务时记录的时间戳 */
    uint32_t task10msEnterTimestamp;                                   /* 10毫秒任务进入时间戳 */
    /** 发生最后一次时序违规时记录的持续时间 */
    uint32_t task100msViolatingDuration;                               /* 100毫秒任务违规持续时间 */
    /** 进入违规执行任务时记录的时间戳 */
    uint32_t task100msEnterTimestamp;                                  /* 100毫秒任务进入时间戳 */
    /** 发生最后一次时序违规时记录的持续时间 */
    uint32_t task100msAlgorithmViolatingDuration;                      /* 100毫秒算法任务违规持续时间 */
    /** 进入违规执行任务时记录的时间戳 */
    uint32_t task100msAlgorithmEnterTimestamp;                         /* 100毫秒算法任务进入时间戳 */
} FRAM_SYS_MON_RECORD_s;                                              /* 定义存储在 FRAM 中的系统监控记录结构体 */

/*========== 外部常量和变量声明 ======================*/

extern FRAM_BASE_HEADER_s fram_databaseHeader[FRAM_BLOCK_MAX];         /* 声明外部变量：FRAM 数据库头部数组，用于管理各数据块在FRAM中的地址和长度 */

/**
 * 要存储在 FRAM 中的变量
 */
/**@{*/
extern FRAM_VERSION_s fram_version;                                    /* 声明外部变量：FRAM 版本信息实例 */
extern FRAM_SOC_s fram_soc;                                            /* 声明外部变量：FRAM 中的 SOC 数据实例 */
extern FRAM_SOE_s fram_soe;                                            /* 声明外部变量：FRAM 中的 SOE 数据实例 */
extern FRAM_SBC_INIT_s fram_sbcInit;                                   /* 声明外部变量：FRAM 中的 SBC 初始化状态实例 */
extern FRAM_DEEP_DISCHARGE_FLAG_s fram_deepDischargeFlags;             /* 声明外部变量：FRAM 中的深度放电标志实例 */
extern FRAM_SYS_MON_RECORD_s fram_sysMonViolationRecord;               /* 声明外部变量：FRAM 中的系统监控违规记录实例 */
extern FRAM_INSULATION_FLAG_s fram_insulationFlags;                    /* 声明外部变量：FRAM 中的绝缘故障标志实例 */
/**@}*/

/*========== 外部函数原型 =====================================*/
/* 外部函数原型（本文件无） */

/*========== 外部化的静态函数原型 (单元测试) ===========*/
#ifdef UNITY_UNIT_TEST                                                  /* 如果定义了 UNITY_UNIT_TEST 宏，用于单元测试 */
#endif                                                                  /* 本文件无导出的测试函数原型 */

#endif /* FOXBMS__FRAM_CFG_H_ */                                       /* 防止重复包含宏：结束 */
