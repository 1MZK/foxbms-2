/**
 *
 * @copyright &copy; 2010 - 2026, Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 * 保留所有权利。
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * 允许在源代码和二进制形式下的重新分发和使用，无论是否经过修改，
 * 只要满足以下条件：
 *
 * 1. 源代码的重新分发必须保留上述版权声明、此条件列表和以下免责声明。
 *
 * 2. 二进制形式的重新分发必须在随分发提供的文档和/或其他材料中
 * 复制上述版权声明、此条件列表和以下免责声明。
 *
 * 3. 未经特定事先书面许可，版权持有者的名称及其贡献者的名称
 * 不得用于认可或推广源自本软件的产品。
 *
 * 本软件由版权持有者和贡献者“按原样”提供，不提供任何明示或暗示的保证，
 * 包括但不限于对适销性和特定用途适用性的暗示保证。
 * 在任何情况下，版权持有者或贡献者均不对任何直接、间接、偶然、特殊、惩罚性或
 * 后果性损害（包括但不限于替代商品或服务的采购、使用、数据或利润的损失，
 * 或业务中断）负责，无论是基于何种责任理论，无论是合同责任、严格责任还是侵权
 * （包括疏忽或其他），即使已被告知可能发生此类损害，也是如此。
 *
 * 我们恳请您在您的硬件、软件、文档或广告材料中使用以下一个或多个短语来指代
 * foxBMS：
 *
 * - "本产品使用了 foxBMS&reg; 的部分内容"
 * - "本产品包含了 foxBMS&reg; 的部分内容"
 * - "本产品衍生自 foxBMS&reg;"
 *
 */

/**
 * @file    battery_system_cfg.h
 * @author  foxBMS 团队
 * @date    2019-12-10 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup BATTERY_SYSTEM_CONFIGURATION
 * @prefix  BS
 *
 * @brief   电池系统配置（例如，电池模块数量、电池电芯数量、温度传感器数量）
 * @details 此文件包含电池系统的基本宏定义，以便在软件的其他部分
 *          派生出所需的输入参数。这些宏都依赖于硬件。
 */

#ifndef FOXBMS__BATTERY_SYSTEM_CFG_H_ /* 如果未定义宏 FOXBMS__BATTERY_SYSTEM_CFG_H_ */
#define FOXBMS__BATTERY_SYSTEM_CFG_H_ /* 则定义宏 FOXBMS__BATTERY_SYSTEM_CFG_H_，防止头文件重复包含 */

/*========== 包含文件 =======================================================*/
#include "general.h"          /* 包含通用配置头文件 */

#include "bms-slave_cfg.h"    /* 包含BMS从控配置头文件 */

#include "fassert.h"          /* 包含断言模块头文件 */

#include <stdbool.h>          /* 包含布尔类型支持 */
#include <stdint.h>           /* 包含标准整数类型支持 */

/* 如果应用程序*不是*为单元测试构建的，即，目标构建
   使用这些设置 */
#ifndef UNITY_UNIT_TEST       /* 如果未定义 UNITY_UNIT_TEST 宏（非单元测试环境） */

/*========== 宏和定义 =========================================================*/

/** 带有预充电的字符串的符号标识符 */
typedef enum {                /* 定义枚举类型，标识字符串是否带有预充电 */
    BS_STRING_WITH_PRECHARGE, /* 枚举值：带有预充电的字符串 */
    BS_STRING_WITHOUT_PRECHARGE, /* 枚举值：不带预充电的字符串 */
} BS_STRING_PRECHARGE_PRESENT_e; /* 枚举类型名 */

/** 字符串的符号标识符。 */
typedef enum {                /* 定义枚举类型，标识字符串ID */
    BS_STRING0    = 0u,       /* 字符串0，值为0 */
    BS_STRING1    = 1u,       /* 字符串1，值为1 */
    BS_STRING2    = 2u,       /* 字符串2，值为2 */
    BS_STRING_MAX = 3u,       /* 字符串最大数量，值为3 */
} BS_STRING_ID_e;             /* 枚举类型名 */

/** 定义放电电流被视为正还是负 */
#define BS_POSITIVE_DISCHARGE_CURRENT (true) /* 宏定义：放电电流为正，设为true */

#if BS_POSITIVE_DISCHARGE_CURRENT == true    /* 如果放电电流定义为正 */
#define BS_CURRENT_DIRECTION_FLOAT (1.0f)    /* 则电流方向浮点系数为1.0 */
#else                                        /* 否则 */
#define BS_CURRENT_DIRECTION_FLOAT (-1.0f)   /* 电流方向浮点系数为-1.0 */
#endif                                       /* 结束 #if */

/**
 * @brief   电池包中并联字符串的数量
 * @details 详情请参见
 *          <a href="../../../../introduction/naming-conventions.html" target="_blank">命名约定</a>。
 *          实现细节：并联字符串的数量不能超过 #GEN_REPEAT_MAXIMUM_REPETITIONS，
 *          除非调整重复宏的实现。
 * @ptype   uint
 */
#define BS_NR_OF_STRINGS (1u) /* 宏定义：电池包中并联字符串的数量为1 */

/* 安全检查：由于实现原因，BS_NR_OF_STRINGS 不能大于 GEN_REPEAT_MAXIMUM_REPETITIONS */
#if (BS_NR_OF_STRINGS > GEN_REPEAT_MAXIMUM_REPETITIONS) /* 如果字符串数量大于最大重复次数 */
#error "Too large number of strings, please check implementation of GEN_REPEAT_U()." /* 编译错误：字符串数量过多，请检查 GEN_REPEAT_U() 的实现 */
#endif /* 结束 #if */

/**
 * @brief   一个字符串中的模块数量
 * @details 详情请参见
 *          <a href="../../../../introduction/naming-conventions.html" target="_blank">命名约定</a>。
 * @ptype   uint
 */
#define BS_NR_OF_MODULES_PER_STRING (1u) /* 宏定义：每个字符串中的电池模块数量为1 */

/**
 * @brief   每个模块中的电芯数量
 * @details 每个模块中的电芯数量，其中并联的电芯被
 *          计为一个电芯块。默认值为 18u。
 *          详情请参见
 *          <a href="../../../../introduction/naming-conventions.html" target="_blank">命名约定</a>。
 * @ptype   uint
 */
#define BS_NR_OF_CELL_BLOCKS_PER_MODULE (18u) /* 宏定义：每个模块中的电芯块数量为18 */

/**
 * @brief   一个电芯块中并联连接的电池电芯数量
 * @details 详情请参见
 *          <a href="../../../../introduction/naming-conventions.html" target="_blank">命名约定</a>。
 * @ptype   uint
 */
#define BS_NR_OF_PARALLEL_CELLS_PER_CELL_BLOCK (1u) /* 宏定义：每个电芯块中并联电芯数量为1 */

/**
 * @brief   每个电池模块上的温度传感器数量
 * @ptype   int
 */
#define BS_NR_OF_TEMP_SENSORS_PER_MODULE (8u) /* 宏定义：每个模块的温度传感器数量为8 */

#if (SLV_USE_MUX_FOR_TEMP == false) && (BS_NR_OF_TEMP_SENSORS_PER_MODULE > SLV_NR_OF_GPIOS_PER_MODULE) /* 如果未使用多路复用器且温度传感器数量大于GPIO数量 */
#error "Number of temperature inputs cannot be higher than number of GPIOs" /* 编译错误：温度输入数量不能高于GPIO数量 */
#endif /* 结束 #if */

/** 一个字符串中的电池电芯数量 */
#define BS_NR_OF_CELL_BLOCKS_PER_STRING (BS_NR_OF_MODULES_PER_STRING * BS_NR_OF_CELL_BLOCKS_PER_MODULE) /* 宏定义：每个字符串的电芯块数量 = 模块数 * 每个模块电芯块数 */
/** 电池系统中的电池电芯总数 */
#define BS_NR_OF_CELL_BLOCKS (BS_NR_OF_CELL_BLOCKS_PER_STRING * BS_NR_OF_STRINGS) /* 宏定义：系统总电芯块数 = 每串电芯块数 * 字符串数 */
/** 一个字符串中的温度传感器数量 */
#define BS_NR_OF_TEMP_SENSORS_PER_STRING (BS_NR_OF_MODULES_PER_STRING * BS_NR_OF_TEMP_SENSORS_PER_MODULE) /* 宏定义：每个字符串的温度传感器数量 = 模块数 * 每个模块传感器数 */
/** 电池系统中温度传感器的总数 */
#define BS_NR_OF_TEMP_SENSORS (BS_NR_OF_TEMP_SENSORS_PER_STRING * BS_NR_OF_STRINGS) /* 宏定义：系统总温度传感器数 = 每串传感器数 * 字符串数 */

/** 延迟时间（毫秒），超过此时间则认为电流测量不再响应。 */
#define BS_CURRENT_MEASUREMENT_RESPONSE_TIMEOUT_ms (200u) /* 宏定义：电流测量响应超时时间200ms */

/** 延迟时间（毫秒），超过此时间则认为库仑计数不再响应。 */
#define BS_COULOMB_COUNTING_MEASUREMENT_RESPONSE_TIMEOUT_ms (2000u) /* 宏定义：库仑计数测量响应超时时间2000ms */

/** 延迟时间（毫秒），超过此时间则认为能量计数不再响应。 */
#define BS_ENERGY_COUNTING_MEASUREMENT_RESPONSE_TIMEOUT_ms (2000u) /* 宏定义：能量计数测量响应超时时间2000ms */

/**
 * @brief   主接触器的最大分断电流。
 * @details 当高于接触器最大分断电流的电流流过时，如果试图中断电流，
 *          主接触器的触点可能会熔焊。
 *
 *          因此，如果浮动电流高于此值，接触器将不会断开。
 *          保险丝应触发以中断高于此值的电流。
 */
#define BS_MAIN_CONTACTORS_MAXIMUM_BREAK_CURRENT_mA (3500) /* 宏定义：主接触器最大分断电流3500mA */

/**
 * @brief   最大保险丝触发持续时间
 * @details 如果电流超过 #BS_MAIN_CONTACTORS_MAXIMUM_BREAK_CURRENT_mA，
 *          BMS状态机将等待此时间直到保险丝触发，以便电流被保险丝
 *          而不是接触器中断。超过此时间后，BMS仍会尝试断开接触器。
 */
#define BS_MAIN_FUSE_MAXIMUM_TRIGGER_DURATION_ms (3000u) /* 宏定义：主保险丝最大触发持续时间3000ms */

/**
 * @brief   在 SOA 模块中用于检查字符串过流的字符串电流最大限值（mA）
 * @details  当违反最大安全限值 (MSL) 时，将请求错误状态并断开接触器。
 */
#define BS_MAXIMUM_STRING_CURRENT_mA (2400u) /* 宏定义：最大字符串电流2400mA */

/**
 * @brief   在 SOA 模块中用于检查电池包过流的电池包电流最大限值（mA）
 * @details 当违反最大安全限值 (MSL) 时，将请求错误状态并断开接触器。
 */
#define BS_MAXIMUM_PACK_CURRENT_mA (2400u * BS_NR_OF_STRINGS) /* 宏定义：最大电池包电流 = 2400mA * 字符串数 */

/**
 * @brief   定义是否应忽略联锁反馈
 * @details True：联锁反馈将被忽略
 *          False：联锁反馈将被评估
 */
#define BS_IGNORE_INTERLOCK_FEEDBACK (false) /* 宏定义：不忽略联锁反馈 */

/**
 * @brief   定义是否应评估 CAN 时序
 * @details - 如果设置为 false，foxBMS 不检查 CAN 时序。
 *          - 如果设置为 true，foxBMS 检查 CAN 时序。有效请求必须
 *            每100ms到来一次，在95-105ms的窗口内。
 */
#define BS_CHECK_CAN_TIMING (true) /* 宏定义：检查CAN时序 */

/**
 * @brief   定义均衡是否可用
 * @details - 如果设置为 true，则完全停用均衡。
 *          - 如果设置为 false，foxBMS 会检查何时必须进行均衡并
 *            相应地激活它。
 */
#define BS_BALANCING_DEFAULT_INACTIVE (true) /* 宏定义：均衡默认处于非活动状态 */

/** 字符串接触器之外的接触器数量（例如，预充电接触器）。*/
#define BS_NR_OF_CONTACTORS_OUTSIDE_STRINGS (1u) /* 宏定义：字符串外部的接触器数量为1（如预充电接触器） */

/** 系统中接触器的总数：
 *  - 每个字符串两个接触器（字符串正极和字符串负极）
 *  - 每个字符串一个可选的预充电接触器 */
#define BS_NR_OF_CONTACTORS ((2u * BS_NR_OF_STRINGS) + BS_NR_OF_CONTACTORS_OUTSIDE_STRINGS) /* 宏定义：总接触器数 = (2 * 字符串数) + 外部接触器数 */

/**
 * @brief   用于确定电池静置状态的电流阈值。如果绝对
 *          电流低于此限值，则电池处于静置状态。
 */
#define BS_REST_CURRENT_mA (200) /* 宏定义：电池静置电流阈值200mA */

/**
 * @brief   电池系统静置前的等待时间，单位为10ms。例如，
 *          仅当电池系统静置时才开始均衡。
 */
#define BS_RELAXATION_PERIOD_10ms (60000u) /* 宏定义：电池弛豫周期60000个10ms（即600秒） */

/**
 * @brief   0mA电流的电流传感器阈值（mA），因为传感器存在抖动。
 */
#define BS_CS_THRESHOLD_NO_CURRENT_mA (200u) /* 宏定义：电流传感器无电流判定阈值200mA */

/**
 * @brief   保险丝上的最大电压降。
 * @details 如果测量的电池电压与保险丝后电压之间的电压差
 *          大于此值，则可以断定保险丝已熔断。电压差可以通过
 *          最大电流和电阻来估算。
 *          对于 Cooper Bussmann 1000A 保险丝，电压降可估算为：
 *          I_max = 1000A，P_loss = 206W
 *          -> 1000A时的电压降大约为206mV
 *          -> 由于测量不精确，选择500mV
 */
#define BS_MAX_VOLTAGE_DROP_OVER_FUSE_mV (500) /* 宏定义：保险丝最大电压降500mV */

/**
 * @brief   待办事项
 * @details 如果应检查正常路径中的保险丝，则设置为 true。这只能在
 *          使用一个专用高压测量来直接监测保险丝后的电压时才能做到。
 *          那么 V_bat 和 V_fuse 之间的电压差表明保险丝已熔断。
 *
 *            V_bat  +------+     V_fuse       预充电/主正接触器
 *          -----+---| 保险丝 |-----+------------/   -----------------
 *                   +------+
 */
#define BS_CHECK_FUSE_PLACED_IN_NORMAL_PATH (true) /* 宏定义：检查放置在正常路径中的保险丝 */

/**
 * @brief   待办事项
 * @details 如果应检查充电路径中的保险丝，则设置为 true。这只能在
 *          使用一个专用高压测量来直接监测保险丝后的电压时才能做到。
 *          那么 V_bat 和 V_fuse 之间的电压差表明保险丝已熔断。
 *
 *            V_bat  +------+     V_fuse       预充电/充电正极接触器
 *          -----+---| 保险丝 |-----+------------/   -----------------
 *                   +------+
 */
#define BS_CHECK_FUSE_PLACED_IN_CHARGE_PATH (false) /* 宏定义：不检查放置在充电路径中的保险丝 */

/**
 * \defgroup    开路检测配置
 *  @details    如果执行开路检测，根据 AFE（模拟前端）的实现，
 *              电芯电压和温度将不会更新，因此旧值可能会通过 CAN 总线
 *              传输。检查时间取决于模块配置和外部电容。请谨慎激活
 *              开路检测！详情请参阅 AFE 实现。
 * @{
 */
/** 在待机期间启用开路检测 */
#define BS_STANDBY_PERIODIC_OPEN_WIRE_CHECK (false) /* 宏定义：待机状态下禁用周期性开路检测 */

/** STANDBY 状态下的周期性开路检测时间（毫秒） */
#define BS_STANDBY_OPEN_WIRE_PERIOD_ms (600000) /* 宏定义：待机状态开路检测周期600000ms（10分钟） */

/** 正常模式下的开路检测（设置为 true 或 false） */
#define BS_NORMAL_PERIODIC_OPEN_WIRE_CHECK (false) /* 宏定义：正常状态下禁用周期性开路检测 */

/** NORMAL 状态下的周期性开路检测时间（毫秒） */
#define BS_NORMAL_OPEN_WIRE_PERIOD_ms (600000) /* 宏定义：正常状态开路检测周期600000ms（10分钟） */

/** 充电模式下的开路检测（设置为 true 或 false） */
#define BS_CHARGE_PERIODIC_OPEN_WIRE_CHECK (false) /* 宏定义：充电状态下禁用周期性开路检测 */

/** CHARGE 状态下的周期性开路检测时间（毫秒） */
#define BS_CHARGE_OPEN_WIRE_PERIOD_ms (600000) /* 宏定义：充电状态开路检测周期600000ms（10分钟） */

/** ERROR 状态下的周期性开路检测时间（毫秒） */
#define BS_ERROR_OPEN_WIRE_PERIOD_ms (30000) /* 宏定义：错误状态开路检测周期30000ms（30秒） */
/**@}*/

FAS_STATIC_ASSERT((BS_NR_OF_STRINGS <= (uint8_t)UINT8_MAX), "This code assumes BS_NR_OF_STRINGS fits into uint8_t"); /* 静态断言：确保字符串数量可以存入uint8_t */

/*========== 外部常量和变量声明 ===============================================*/
/** 定义字符串是否可用于预充电 */
extern BS_STRING_PRECHARGE_PRESENT_e bs_stringsWithPrecharge[BS_NR_OF_STRINGS]; /* 外部全局变量声明：表示各字符串是否带预充电的数组 */

/*========== 外部函数原型 =====================================================*/

/*========== 外部化的静态函数原型（单元测试）==================================*/
#else /* 否则（定义了 UNITY_UNIT_TEST，即单元测试环境） */
/* 在运行单元测试套件的情况下，配置应完全
   从单元测试特定的配置文件中读取。
   单元测试配置文件最终如何使用，在
   单元测试配置目录中进行了描述 */
#include "battery_system_cfg_unit_test.h" /* 包含单元测试专用的电池系统配置头文件 */
#endif /* 结束 #ifndef UNITY_UNIT_TEST */
#ifdef UNITY_UNIT_TEST /* 如果定义了 UNITY_UNIT_TEST 宏 */
#endif /* 结束 #ifdef UNITY_UNIT_TEST */

#endif /* FOXBMS__BATTERY_SYSTEM_CFG_H_ */ /* 结束头文件保护宏 */
