
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
 * @file    battery_cell_cfg.h
 * @author  foxBMS Team
 * @date    2017-03-14 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup BATTERY_CELL_CONFIGURATION
 * @prefix  BC
 *
 * @brief   电池单体配置（如最小和最大单体电压）
 * @details 本文件包含电池单体的基本宏定义，用于为软件的其他部分提供所需的输入参数。
 *          这些宏均依赖于具体的硬件特性。
 */

#ifndef FOXBMS__BATTERY_CELL_CFG_H_  //条件编译开始指令
#define FOXBMS__BATTERY_CELL_CFG_H_  //定义宏

/*========== 包含文件 =======================================================*/

#include <math.h>
#include <stdint.h>

/*========== 宏与定义 =======================================================*/

/**
 * @brief   放电时的最高温度限制。
 * @details 当违反最大安全限制(MSL)时，将请求错误状态并断开接触器。
 *          当违反推荐安全限制(RSL)或最大运行限制(MOL)时，将设置相应的标志位。
 * @ptype   int
 * @unit    0.1 °C (deci °C)
 */
/**@{*/
#define BC_TEMPERATURE_MAX_DISCHARGE_MSL_ddegC (550)    //电池放电时的最大安全限制温度
#define BC_TEMPERATURE_MAX_DISCHARGE_RSL_ddegC (500)    //电池放电时的推荐安全限制温度
#define BC_TEMPERATURE_MAX_DISCHARGE_MOL_ddegC (450)    //电池放电时的最大运行限制温度
/**@}*/

/**
 * @brief   放电时的最低温度限制。
 * @details 当违反最大安全限制(MSL)时，将请求错误状态并断开接触器。
 *          当违反推荐安全限制(RSL)或最大运行限制(MOL)时，将设置相应的标志位。
 * @ptype   int
 * @unit    0.1 °C (deci °C)
 */
/**@{*/
#define BC_TEMPERATURE_MIN_DISCHARGE_MSL_ddegC (-200)    //电池放电时的最小安全限制温度
#define BC_TEMPERATURE_MIN_DISCHARGE_RSL_ddegC (-150)    //电池放电时的最小推荐安全限制温度
#define BC_TEMPERATURE_MIN_DISCHARGE_MOL_ddegC (-100)    //电池放电时的最小运行限制温度
/**@}*/

/**
 * @brief   充电时的最高温度限制。
 * @details 当违反最大安全限制(MSL)时，将请求错误状态并断开接触器。
 *          当违反推荐安全限制(RSL)或最大运行限制(MOL)时，将设置相应的标志位。
 * @ptype   int
 * @unit    0.1 °C (deci °C)
 */
/**@{*/
#define BC_TEMPERATURE_MAX_CHARGE_MSL_ddegC (450)    //电池充电时的最大安全限制温度
#define BC_TEMPERATURE_MAX_CHARGE_RSL_ddegC (400)    //电池充电时的推荐安全限制温度
#define BC_TEMPERATURE_MAX_CHARGE_MOL_ddegC (350)    //电池充电时的最大运行限制温度
/**@}*/

/**
 * @brief   充电时的最低温度限制。
 * @details 当违反最大安全限制(MSL)时，将请求错误状态并断开接触器。
 *          当违反推荐安全限制(RSL)或最大运行限制(MOL)时，将设置相应的标志位。
 * @ptype   int
 * @unit    0.1 °C (deci °C)
 */
/**@{*/
#define BC_TEMPERATURE_MIN_CHARGE_MSL_ddegC (-200)    //电池充电时的最小安全限制温度
#define BC_TEMPERATURE_MIN_CHARGE_RSL_ddegC (-150)    //电池充电时的最小推荐安全限制温度
#define BC_TEMPERATURE_MIN_CHARGE_MOL_ddegC (-100)    //电池充电时的最小运行限制温度
/**@}*/

/**
 * @brief   最高单体电压限制。
 * @details 当违反最大安全限制(MSL)时，将请求错误状态并断开接触器。
 *          当违反推荐安全限制(RSL)或最大运行限制(MOL)时，将设置相应的标志位。
 * @ptype   int
 * @unit    mV
 */
/**@{*/
#define BC_VOLTAGE_MAX_MSL_mV (2800)  //电池单体电压的最大安全限制
#define BC_VOLTAGE_MAX_RSL_mV (2750)  //电池单体电压的推荐安全限制
#define BC_VOLTAGE_MAX_MOL_mV (2720)  //电池单体电压的最大运行限制
/**@}*/

/**
 * @brief   数据手册中的标称单体电压
 * @ptype   int
 * @unit    mV
 */
#define BC_VOLTAGE_NOMINAL_mV (2500)    //数据手册中的标称单体电压

/**
 * @brief   最低单体电压限制。
 * @details 当违反最大安全限制(MSL)时，将请求错误状态并断开接触器。
 *          当违反推荐安全限制(RSL)或最大运行限制(MOL)时，将设置相应的标志位。
 * @ptype   int
 * @unit    mV
 */
/**@{*/
#define BC_VOLTAGE_MIN_MSL_mV (1500)    //数据手册中的最低单体电压
#define BC_VOLTAGE_MIN_RSL_mV (1550)    //电池单体电压的最小推荐安全限制
#define BC_VOLTAGE_MIN_MOL_mV (1580)    //电池单体电压的最小运行限制
/**@}*/

/**
 * @brief   深度放电单体电压限制。
 * @details 如果违反了此电压限制，则说明单体电池已发生故障。
 *          BMS将不允许闭合接触器，直到更换该单体电池。
 *          通过发送相应的CAN调试消息来确认单体电池已更换。
 * @ptype   int
 * @unit    mV
 */
#define BC_VOLTAGE_DEEP_DISCHARGE_mV (BC_VOLTAGE_MIN_MSL_mV)    //深度放电单体电压限制，设置为与最低单体电压的最大安全限制相同

/**
 * @brief   最大放电电流限制。
 * @details 当违反最大安全限制(MSL)时，将请求错误状态并断开接触器。
 *          当违反推荐安全限制(RSL)或最大运行限制(MOL)时，将设置相应的标志位。
 * @ptype   int
 * @unit    mA
 */
/**@{*/
#define BC_CURRENT_MAX_DISCHARGE_MSL_mA (180000u)   /*!< 电池放电时的最大安全限制电流 */
#define BC_CURRENT_MAX_DISCHARGE_RSL_mA (175000u)   /*!< 电池放电时的推荐安全限制电流 */
#define BC_CURRENT_MAX_DISCHARGE_MOL_mA (170000u)   /*!< 电池放电时的最大运行限制电流 */
/**@}*/

/**
 * @brief   最大充电电流限制。
 * @details 当违反最大安全限制(MSL)时，将请求错误状态并断开接触器。
 *          当违反推荐安全限制(RSL)或最大运行限制(MOL)时，将设置相应的标志位。
 * @ptype   int
 * @unit    mA
 */
/**@{*/
#define BC_CURRENT_MAX_CHARGE_MSL_mA (180000u)   /*!< 电池充电时的最大安全限制电流 */
#define BC_CURRENT_MAX_CHARGE_RSL_mA (175000u)   /*!< 电池充电时的推荐安全限制电流 */
#define BC_CURRENT_MAX_CHARGE_MOL_mA (170000u)   /*!< 电池充电时的最大运行限制电流 */
/**@}*/

/**
 * @brief   用于SOC计算的电池容量
 * @ptype   int
 * @unit    mAh
 */
#define BC_CAPACITY_mAh (3500u) //电池容量

/**
 * @brief   电池能量
 * @ptype   float
 * @unit    Wh
 */
#define BC_ENERGY_Wh (10.0f)    //电池能量，计算方法：BC_ENERGY_Wh = (BC_VOLTAGE_NOMINAL_mV / 1000) * (BC_CAPACITY_mAh / 1000);

#if BC_VOLTAGE_MIN_MSL_mV < BC_VOLTAGE_DEEP_DISCHARGE_mV
#error "配置错误！ - 欠压最大安全限制不能低于深度放电限制"
#endif

/** 查找表结构体 */
typedef struct {
    const int16_t voltage_mV; /*!< 单体电压，单位：mV */
    const float_t value;      /*!< 对应的值，可以是SOC/SOE(%)或容量/能量 */
} BC_LUT_s;

/*========== 外部常量与变量声明 ==============================================*/
extern uint16_t bc_stateOfChargeLookupTableLength;   /*!< SOC查找表长度 */
extern const BC_LUT_s bc_stateOfChargeLookupTable[]; /*!< SOC查找表 */

extern uint16_t bc_stateOfEnergyLookupTableLength;   /*!< SOE查找表长度 */
extern const BC_LUT_s bc_stateOfEnergyLookupTable[]; /*!< SOE查找表 */

/*========== 外部函数原型 ===================================================*/

/*========== 外部化的静态函数原型（单元测试） ================================*/
#ifdef UNITY_UNIT_TEST
#endif

#endif /* FOXBMS__BATTERY_CELL_CFG_H_ */
