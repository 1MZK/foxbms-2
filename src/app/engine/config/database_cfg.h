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
 * @file    database_cfg.h
 * @author  foxBMS Team
 * @date    2015-08-18 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup ENGINE_CONFIGURATION
 * @prefix  DATA
 *
 * @brief   数据库配置头文件
 * @details 提供数据库配置的接口
 */

#ifndef FOXBMS__DATABASE_CFG_H_
#define FOXBMS__DATABASE_CFG_H_

/*========== 包含文件 =======================================================*/

#include "battery_system_cfg.h"
#include "bms-slave_cfg.h"

#include "mcu.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/*========== 宏与定义 =======================================================*/
/** 数据库通道(数据块)的配置结构体 */
typedef struct {
    void *pDatabaseEntry; /*!< 指向数据库条目的指针 */
    uint32_t dataLength;  /*!< 条目的长度 */
} DATA_BASE_s;

/** 数据块标识号 (ID) 枚举 */
typedef enum {
    DATA_BLOCK_ID_ADC_VOLTAGE,                      /*!< ADC电压 */
    DATA_BLOCK_ID_AEROSOL_SENSOR,                   /*!< 气溶胶传感器 */
    DATA_BLOCK_ID_ALL_GPIO_VOLTAGES_BASE,           /*!< 所有GPIO电压基准 */
    DATA_BLOCK_ID_ALL_GPIO_VOLTAGES_REDUNDANCY0,    /*!< 所有GPIO电压冗余0 */
    DATA_BLOCK_ID_BALANCING_CONTROL,                /*!< 均衡控制 */
    DATA_BLOCK_ID_BALANCING_FEEDBACK_BASE,          /*!< 均衡反馈基准 */
    DATA_BLOCK_ID_BALANCING_FEEDBACK_REDUNDANCY0,   /*!< 均衡反馈冗余0 */
    DATA_BLOCK_ID_CELL_TEMPERATURE,                 /*!< 单体温度 */
    DATA_BLOCK_ID_CELL_TEMPERATURE_BASE,            /*!< 单体温度基准 */
    DATA_BLOCK_ID_CELL_TEMPERATURE_REDUNDANCY0,     /*!< 单体温度冗余0 */
    DATA_BLOCK_ID_CELL_VOLTAGE,                     /*!< 单体电压 */
    DATA_BLOCK_ID_CELL_VOLTAGE_BASE,                /*!< 单体电压基准 */
    DATA_BLOCK_ID_CELL_VOLTAGE_REDUNDANCY0,         /*!< 单体电压冗余0 */
    DATA_BLOCK_ID_CONTACTOR_FEEDBACK,               /*!< 接触器反馈 */
    DATA_BLOCK_ID_CURRENT,                          /*!< 电流 */
    DATA_BLOCK_ID_CURRENT_SENSOR_TEMPERATURE,       /*!< 电流传感器温度 */
    DATA_BLOCK_ID_POWER,                            /*!< 功率 */
    DATA_BLOCK_ID_CURRENT_COUNTER,                  /*!< 电流计数(库仑计) */
    DATA_BLOCK_ID_ENERGY_COUNTER,                   /*!< 能量计数 */
    DATA_BLOCK_ID_SYSTEM_VOLTAGE_1,                 /*!< 系统电压U1 */
    DATA_BLOCK_ID_SYSTEM_VOLTAGE_2,                 /*!< 系统电压U2 */
    DATA_BLOCK_ID_SYSTEM_VOLTAGE_3,                 /*!< 系统电压U3 */
    DATA_BLOCK_ID_DUMMY_FOR_SELF_TEST,              /*!< 自检用的虚拟块 */
    DATA_BLOCK_ID_ERROR_STATE,                      /*!< 错误状态 */
    DATA_BLOCK_ID_HTSEN,                            /*!< 温湿度传感器 */
    DATA_BLOCK_ID_INSULATION,                       /*!< 绝缘监测 */
    DATA_BLOCK_ID_INTERLOCK_FEEDBACK,               /*!< 互锁反馈 */
    DATA_BLOCK_ID_MIN_MAX,                          /*!< 最小/最大值 */
    DATA_BLOCK_ID_MOL_FLAG,                         /*!< MOL(最大运行限制)标志 */
    DATA_BLOCK_ID_MOVING_AVERAGE,                   /*!< 滑动平均 */
    DATA_BLOCK_ID_MSL_FLAG,                         /*!< MSL(最大安全限制)标志 */
    DATA_BLOCK_ID_OPEN_WIRE_BASE,                   /*!< 开路基准 */
    DATA_BLOCK_ID_OPEN_WIRE_REDUNDANCY0,            /*!< 开路冗余0 */
    DATA_BLOCK_ID_PACK_VALUES,                      /*!< 电池包数值 */
    DATA_BLOCK_ID_RSL_FLAG,                         /*!< RSL(推荐安全限制)标志 */
    DATA_BLOCK_ID_SLAVE_CONTROL,                    /*!< 从控控制 */
    DATA_BLOCK_ID_SOC,                              /*!< SOC(荷电状态) */
    DATA_BLOCK_ID_SOE,                              /*!< SOE(能量状态) */
    DATA_BLOCK_ID_SOF,                              /*!< SOF(功能状态) */
    DATA_BLOCK_ID_SOH,                              /*!< SOH(健康状态) */
    DATA_BLOCK_ID_STATE_REQUEST,                    /*!< 状态请求 */
    DATA_BLOCK_ID_SYSTEM_STATE,                     /*!< 系统状态 */
    DATA_BLOCK_ID_PHY,                              /*!< PHY(物理层) */
    DATA_BLOCK_ID_MAX,                              /**< 请勿更改，必须是最后一个条目 */
} DATA_BLOCK_ID_e;

FAS_STATIC_ASSERT(
    (int32_t)DATA_BLOCK_ID_MAX < (int32_t)UINT8_MAX,
    "数据库条目的最大数量超过了UINT8_MAX；请调整DATA_Initialize和DATA_IterateOverDatabaseEntries中的长度检查");

/** 数据块头结构体 */
typedef struct {
    DATA_BLOCK_ID_e uniqueId;   /*!< 数据库条目的唯一标识ID */
    uint32_t timestamp;         /*!< 上次数据库更新的时间戳 */
    uint32_t previousTimestamp; /*!< 上上次数据库更新的时间戳 */
} DATA_BLOCK_HEADER_s;

/** 单体电压数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                  /*!< 数据块头 */
    uint8_t state;                               /*!< 保留供将来使用 */
    int32_t stringVoltage_mV[BS_NR_OF_STRINGS];  /*!< 每个串的累积单体电压，单位：mV */
    bool invalidStringVoltage[BS_NR_OF_STRINGS]; /*!< false -> 有效, true -> 无效 */
    int16_t cellVoltage_mV[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING]
                          [BS_NR_OF_CELL_BLOCKS_PER_MODULE];                 /*!< 单体电压，单位：mV */
    bool invalidCellVoltage[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING]
                           [BS_NR_OF_CELL_BLOCKS_PER_MODULE];                 /*!< false -> 有效, true -> 无效 */
    uint16_t nrValidCellVoltages[BS_NR_OF_STRINGS];                           /*!< 有效电压的数量 */
    uint32_t moduleVoltage_mV[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING]; /*!< 每个模块的累积单体电压，单位：mV */
    bool invalidModuleVoltage[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING]; /*!< false -> 有效, true -> 无效 */
} DATA_BLOCK_CELL_VOLTAGE_s;

/** 单体温度数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header; /*!< 数据块头 */
    uint8_t state;              /*!< 保留供将来使用 */
    int16_t cellTemperature_ddegC[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING]
                                 [BS_NR_OF_TEMP_SENSORS_PER_MODULE];     /*!< 单体温度，单位：0.1 °C */
    bool invalidCellTemperature[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING]
                               [BS_NR_OF_TEMP_SENSORS_PER_MODULE]; /*!< false -> 有效, true -> 无效 */
    uint16_t nrValidTemperatures[BS_NR_OF_STRINGS];                /*!< 每个串中有效温度的数量 */
} DATA_BLOCK_CELL_TEMPERATURE_s;

/** 最小和最大值数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                               /*!< 数据块头 */
    int16_t averageCellVoltage_mV[BS_NR_OF_STRINGS];          /*!< 平均单体电压，单位：mV */
    int16_t minimumCellVoltage_mV[BS_NR_OF_STRINGS];          /*!< 最低单体电压，单位：mV */
    int16_t previousMinimumCellVoltage_mV[BS_NR_OF_STRINGS];  /*!< 上一次最低单体电压，单位：mV */
    int16_t maximumCellVoltage_mV[BS_NR_OF_STRINGS];          /*!< 最高单体电压，单位：mV */
    int16_t previousMaximumCellVoltage_mV[BS_NR_OF_STRINGS];  /*!< 上一次最高单体电压，单位：mV */
    uint16_t nrModuleMinimumCellVoltage[BS_NR_OF_STRINGS];    /*!< 具有最低单体电压的模块编号 */
    uint16_t nrCellMinimumCellVoltage[BS_NR_OF_STRINGS];      /*!< 具有最低单体电压的电芯编号 */
    uint16_t nrModuleMaximumCellVoltage[BS_NR_OF_STRINGS];    /*!< 具有最高单体电压的模块编号 */
    uint16_t nrCellMaximumCellVoltage[BS_NR_OF_STRINGS];      /*!< 具有最高单体电压的电芯编号 */
    uint16_t validMeasuredCellVoltages[BS_NR_OF_STRINGS];     /*!< 有效测量单体电压的数量 */
    float_t averageTemperature_ddegC[BS_NR_OF_STRINGS];       /*!< 平均温度，单位：0.1 °C */
    int16_t minimumTemperature_ddegC[BS_NR_OF_STRINGS];       /*!< 最低温度，单位：0.1 °C */
    uint16_t nrModuleMinimumTemperature[BS_NR_OF_STRINGS];    /*!< 具有最低温度的模块编号 */
    uint16_t nrSensorMinimumTemperature[BS_NR_OF_STRINGS];    /*!< 具有最低温度的传感器编号 */
    int16_t maximumTemperature_ddegC[BS_NR_OF_STRINGS];       /*!< 最高温度，单位：0.1 °C */
    uint16_t nrModuleMaximumTemperature[BS_NR_OF_STRINGS];    /*!< 具有最高温度的模块编号 */
    uint16_t nrSensorMaximumTemperature[BS_NR_OF_STRINGS];    /*!< 具有最高温度的传感器编号 */
    uint16_t validMeasuredCellTemperatures[BS_NR_OF_STRINGS]; /*!< 有效测量单体温度的数量 */
} DATA_BLOCK_MIN_MAX_s;

/** 电池包测量值数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;       /*!< 数据块头 */
    int32_t packCurrent_mA;           /*!< 整个电池包的电流，单位：mA */
    uint8_t invalidPackCurrent;       /*!< 电流是否有效的位掩码。0->有效, 1->无效 */
    int32_t batteryVoltage_mV;        /*!< 电池正负极之间的电压，单位：mV */
    uint8_t invalidBatteryVoltage;    /*!< 电压是否有效的位掩码。0->有效, 1->无效 */
    int32_t highVoltageBusVoltage_mV; /*!< 电池负极与正极主接触器之后之间的电压(高压母线电压)，单位：mV */
    uint8_t invalidHvBusVoltage;      /*!< 电压是否有效的位掩码。0->有效, 1->无效 */
    int32_t packPower_W;              /*!< 电池包提供或吸收的功率，单位：W */
    uint8_t invalidPackPower;         /*!< 功率是否有效的位掩码。0->有效, 1->无效 */
    int32_t stringVoltage_mV[BS_NR_OF_STRINGS];     /*!< 每个串的电压，单位：mV */
    uint8_t invalidStringVoltage[BS_NR_OF_STRINGS]; /*!< 电压是否有效的位掩码。0->有效, 1->无效 */
    int32_t stringCurrent_mA[BS_NR_OF_STRINGS];     /*!< 每个串的电流，单位：mA */
    uint8_t invalidStringCurrent[BS_NR_OF_STRINGS]; /*!< 电流是否有效的位掩码。0->有效, 1->无效 */
    int32_t stringPower_W[BS_NR_OF_STRINGS];        /*!< 每个串的功率，单位：W */
    uint8_t invalidStringPower[BS_NR_OF_STRINGS];   /*!< 功率是否有效的位掩码。0->有效, 1->无效 */
} DATA_BLOCK_PACK_VALUES_s;

/** 电流测量数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                   /*!< 数据块头 */
    int32_t current_mA[BS_NR_OF_STRINGS];         /*!< 电流值，单位：mA */
    uint8_t invalidMeasurement[BS_NR_OF_STRINGS]; /*!< 0: 测量有效, 1: 测量无效 */
    uint8_t newCurrent;                           /*!< 0: 测量有效, 1: 测量无效 */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS]; /*!< 上一次测量的时间戳 */
    uint32_t timestamp[BS_NR_OF_STRINGS];         /*!< 当前测量的时间戳 */
} DATA_BLOCK_CURRENT_s;

/** 电流传感器温度测量数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                        /*!< 数据块头 */
    int32_t sensorTemperature_ddegC[BS_NR_OF_STRINGS]; /*!< 传感器温度，单位：0.1 °C */
    uint8_t invalidMeasurement[BS_NR_OF_STRINGS];      /*!< 0: 测量有效, 1: 测量无效 */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS];      /*!< 上一次测量的时间戳 */
    uint32_t timestamp[BS_NR_OF_STRINGS];              /*!< 当前测量的时间戳 */
} DATA_BLOCK_CURRENT_SENSOR_TEMPERATURE_s;

/** 电流传感器功率测量数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                   /*!< 数据块头 */
    int32_t power_W[BS_NR_OF_STRINGS];            /*!< 功率值，单位：W */
    uint8_t invalidMeasurement[BS_NR_OF_STRINGS]; /*!< 0: 测量有效, 1: 测量无效 */
    uint8_t newPower;                             /*!< 指示新功率测量的计数器 */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS]; /*!< 上一次测量的时间戳 */
    uint32_t timestamp[BS_NR_OF_STRINGS];         /*!< 当前测量的时间戳 */
} DATA_BLOCK_POWER_s;

/** 电流计数(库仑计)数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                   /*!< 数据块头 */
    int32_t currentCounter_As[BS_NR_OF_STRINGS];  /*!< 电流计数值，单位：A.s (安秒) */
    uint8_t invalidMeasurement[BS_NR_OF_STRINGS]; /*!< 0: 测量有效, 1: 测量无效 */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS]; /*!< 上一次测量的时间戳 */
    uint32_t timestamp[BS_NR_OF_STRINGS];         /*!< 当前测量的时间戳 */
} DATA_BLOCK_CURRENT_COUNTER_s;

/** 电流传感器能量计数数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                   /*!< 数据块头 */
    int32_t energyCounter_Wh[BS_NR_OF_STRINGS];   /*!< 能量计数值，单位：Wh */
    uint8_t invalidMeasurement[BS_NR_OF_STRINGS]; /*!< 0: 测量有效, 1: 测量无效 */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS]; /*!< 上一次测量的时间戳 */
    uint32_t timestamp[BS_NR_OF_STRINGS];         /*!< 当前测量的时间戳 */
} DATA_BLOCK_ENERGY_COUNTER_s;

/** 电流传感器电压U1测量数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                   /*!< 数据块头 */
    uint8_t invalidMeasurement[BS_NR_OF_STRINGS]; /*!< 0: 测量有效, 1: 测量无效 */
    int32_t highVoltage_mV[BS_NR_OF_STRINGS];     /*!< 高压值，单位：mV */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS]; /*!< 上一次测量的时间戳 */
    uint32_t timestamp[BS_NR_OF_STRINGS];         /*!< 当前测量的时间戳 */
} DATA_BLOCK_SYSTEM_VOLTAGE_1_s;

/** 电流传感器电压U2测量数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                   /*!< 数据块头 */
    uint8_t invalidMeasurement[BS_NR_OF_STRINGS]; /*!< 0: 测量有效, 1: 测量无效 */
    int32_t highVoltage_mV[BS_NR_OF_STRINGS];     /*!< 高压值，单位：mV */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS]; /*!< 上一次测量的时间戳 */
    uint32_t timestamp[BS_NR_OF_STRINGS];         /*!< 当前测量的时间戳 */
} DATA_BLOCK_SYSTEM_VOLTAGE_2_s;

/** 电流传感器电压U3测量数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                   /*!< 数据块头 */
    uint8_t invalidMeasurement[BS_NR_OF_STRINGS]; /*!< 0: 测量有效, 1: 测量无效 */
    int32_t highVoltage_mV[BS_NR_OF_STRINGS];     /*!< 高压值，单位：mV */
    uint32_t previousTimestamp[BS_NR_OF_STRINGS]; /*!< 上一次测量的时间戳 */
    uint32_t timestamp[BS_NR_OF_STRINGS];         /*!< 当前测量的时间戳 */
} DATA_BLOCK_SYSTEM_VOLTAGE_3_s;

/** 均衡控制数据结构体声明 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header; /*!< 数据块头 */
    bool enableBalancing;       /*!< 启用/禁用均衡的开关 */
    uint8_t threshold_mV;       /*!< 均衡阈值，单位：mV */
    uint8_t request;            /*!< 通过CAN请求均衡 */
    bool activateBalancing[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING]
                          [BS_NR_OF_CELL_BLOCKS_PER_MODULE]; /*!< 0: 未均衡, 1: 均衡激活 */
    uint32_t deltaCharge_mAs[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING]
                            [BS_NR_OF_CELL_BLOCKS_PER_MODULE]; /*!< 放电深度差值，单位：mAs */
    uint16_t nrBalancedCells[BS_NR_OF_STRINGS];                /*!< 正在均衡的单体数量 */
} DATA_BLOCK_BALANCING_CONTROL_s;

/** 用户IO控制数据结构体声明 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                            /*!< 数据块头 */
    uint8_t state;                                         /*!< 保留供将来使用 */
    uint32_t eepromReadAddressToUse;                       /*!< 要读取的从控EEPROM地址 */
    uint32_t eepromReadAddressLastUsed;                    /*!< 上次使用的从控EEPROM读取地址 */
    uint32_t eepromWriteAddressToUse;                      /*!< 要写入的从控EEPROM地址 */
    uint32_t eepromWriteAddressLastUsed;                   /*!< 上次使用的从控EEPROM写入地址 */
    uint8_t ioValueOut[BS_NR_OF_MODULES_PER_STRING];       /*!< 要写入端口扩展器的数据 */
    uint8_t ioValueIn[BS_NR_OF_MODULES_PER_STRING];        /*!< 从端口扩展器读取的数据 */
    uint8_t eepromValueWrite[BS_NR_OF_MODULES_PER_STRING]; /*!< 要写入从控EEPROM的数据 */
    uint8_t eepromValueRead[BS_NR_OF_MODULES_PER_STRING];  /*!< 从从控EEPROM读取的数据 */
    uint8_t
        externalTemperatureSensor[BS_NR_OF_MODULES_PER_STRING]; /*!< 从控上外部传感器读取的温度 */
} DATA_BLOCK_SLAVE_CONTROL_s;

/** 单体均衡反馈数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                                    /*!< 数据块头 */
    uint8_t state;                                                 /*!< 保留供将来使用 */
    uint16_t value[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING]; /*!< 值，单位：mV (光耦输出) */
} DATA_BLOCK_BALANCING_FEEDBACK_s;

/** 单体开路检测数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;             /*!< 数据块头 */
    uint8_t state;                          /*!< 保留供将来使用 */
    uint16_t nrOpenWires[BS_NR_OF_STRINGS]; /*!< 开路数量 */
    uint8_t openWire[BS_NR_OF_STRINGS]
                    [BS_NR_OF_MODULES_PER_STRING *
                     (BS_NR_OF_CELL_BLOCKS_PER_MODULE + 1u)]; /*!< 1 -> 开路, 0 -> 正常 */
} DATA_BLOCK_OPEN_WIRE_s;

/** GPIO电压数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header; /*!< 数据块头 */
    uint8_t state;              /*!< 保留供将来使用 */
    int16_t gpioVoltages_mV[BS_NR_OF_STRINGS]
                           [BS_NR_OF_MODULES_PER_STRING * SLV_NR_OF_GPIOS_PER_MODULE];                 /*!< GPIO电压，单位：mV */
    int16_t gpaVoltages_mV[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING * SLV_NR_OF_GPAS_PER_MODULE]; /*!< GPA电压，单位：mV */
    uint16_t invalidGpioVoltages[BS_NR_OF_STRINGS][BS_NR_OF_MODULES_PER_STRING]; /*!< 电压是否有效的位掩码。
                                                                                    0->有效, 1->无效 */
} DATA_BLOCK_ALL_GPIO_VOLTAGES_s;

/** 错误标志数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                                          /*!< 数据块头 */
    bool afeCommunicationCrcError[BS_NR_OF_STRINGS];                     /*!< false -> 无错误, true -> AFE通信CRC错误 */
    bool afeSlaveMultiplexerError[BS_NR_OF_STRINGS];                     /*!< false -> 无错误, true -> AFE从控多路复用器错误 */
    bool afeCommunicationSpiError[BS_NR_OF_STRINGS];                     /*!< false -> 无错误, true -> AFE通信SPI错误 */
    bool afeConfigurationError[BS_NR_OF_STRINGS];                        /*!< false -> 无错误, true -> AFE配置错误 */
    bool afeCellVoltageInvalidError[BS_NR_OF_STRINGS];                   /*!< false -> 无错误, true -> AFE单体电压无效错误 */
    bool afeCellTemperatureInvalidError[BS_NR_OF_STRINGS];               /*!< false -> 无错误, true -> AFE单体温度无效错误 */
    bool baseCellVoltageMeasurementTimeoutError;                         /*!< false -> 无错误, true -> 基准单体电压测量超时错误 */
    bool redundancy0CellVoltageMeasurementTimeoutError;                  /*!< false -> 无错误, true -> 冗余0单体电压测量超时错误 */
    bool baseCellTemperatureMeasurementTimeoutError;                     /*!< false -> 无错误, true -> 基准单体温度测量超时错误 */
    bool redundancy0CellTemperatureMeasurementTimeoutError;              /*!< false -> 无错误, true -> 冗余0单体温度测量超时错误 */
    bool currentMeasurementTimeoutError[BS_NR_OF_STRINGS];               /*!< false -> 无错误, true -> 电流测量超时错误 */
    bool currentMeasurementInvalidError[BS_NR_OF_STRINGS];               /*!< false -> 无错误, true -> 电流测量无效错误 */
    bool currentSensorVoltage1TimeoutError[BS_NR_OF_STRINGS];            /*!< false -> 无错误, true -> 电流传感器电压1超时错误 */
    bool currentSensorVoltage2TimeoutError[BS_NR_OF_STRINGS];            /*!< false -> 无错误, true -> 电流传感器电压2超时错误 */
    bool currentSensorVoltage3TimeoutError[BS_NR_OF_STRINGS];            /*!< false -> 无错误, true -> 电流传感器电压3超时错误 */
    bool currentSensorPowerTimeoutError[BS_NR_OF_STRINGS];               /*!< false -> 无错误, true -> 电流传感器功率超时错误 */
    bool currentSensorCoulombCounterTimeoutError[BS_NR_OF_STRINGS];      /*!< false -> 无错误, true -> 电流传感器库仑计超时错误 */
    bool currentSensorEnergyCounterTimeoutError[BS_NR_OF_STRINGS];       /*!< false -> 无错误, true -> 电流传感器能量计超时错误 */
    bool powerMeasurementInvalidError[BS_NR_OF_STRINGS];                 /*!< false -> 无错误, true -> 功率测量无效错误 */
    bool mainFuseError;                                                  /*!< false -> 熔断器正常,  true -> 主熔断器熔断 */
    bool stringFuseError[BS_NR_OF_STRINGS];                              /*!< false -> 熔断器正常,  true -> 串熔断器熔断 */
    bool openWireDetectedError[BS_NR_OF_STRINGS];                        /*!< false -> 无错误, true -> 检测到开路错误 */
    bool stateRequestTimingViolationError;                               /*!< false -> 无错误, true -> 状态请求时序违规错误 */
    bool canRxQueueFullError;                                            /*!< false -> 无错误, true -> CAN接收队列满错误 */
    bool canTxQueueFullError;                                            /*!< false -> 无错误, true -> CAN发送队列满错误 */
    bool coinCellLowVoltageError;                                        /*!< false -> 无错误, true -> 纽扣电池电压低错误 */
    bool plausibilityCheckPackVoltageError[BS_NR_OF_STRINGS];            /*!< false -> 无错误, true -> 电池包电压合理性检查错误 */
    bool plausibilityCheckCellVoltageError[BS_NR_OF_STRINGS];            /*!< false -> 无错误, true -> 单体电压合理性检查错误 */
    bool plausibilityCheckCellVoltageSpreadError[BS_NR_OF_STRINGS];      /*!< false -> 无错误, true -> 单体电压压差合理性检查错误 */
    bool plausibilityCheckCellTemperatureError[BS_NR_OF_STRINGS];        /*!< false -> 无错误, true -> 单体温度合理性检查错误 */
    bool plausibilityCheckCellTemperatureSpreadError[BS_NR_OF_STRINGS];  /*!< false -> 无错误, true -> 单体温差合理性检查错误 */
    bool currentSensorNotRespondingError[BS_NR_OF_STRINGS];              /*!< false -> 无错误, true -> 电流传感器无响应错误 */
    bool contactorInNegativePathOfStringFeedbackError[BS_NR_OF_STRINGS]; /*!< false -> 无错误, true -> 串负极接触器反馈错误 */
    bool contactorInPositivePathOfStringFeedbackError[BS_NR_OF_STRINGS]; /*!< false -> 无错误, true -> 串正极接触器反馈错误 */
    bool prechargeContactorFeedbackError[BS_NR_OF_STRINGS];              /*!< false -> 无错误, true -> 预充接触器反馈错误 */
    bool interlockOpenedError;                                           /*!< false -> 无错误, true -> 互锁断开错误 */
    bool insulationMeasurementInvalidError;                              /*!< false -> 无错误, true -> 绝缘测量无效错误 */
    bool criticalLowInsulationResistanceError;                           /*!< false -> 无临界电阻, true -> 临界低绝缘电阻 */
    bool warnableLowInsulationResistanceError;                           /*!< false -> 无警告电阻, true -> 警告级低绝缘电阻 */
    bool insulationGroundFaultDetectedError;                             /*!< false -> 未检测到HV与底盘间绝缘故障, true -> 检测到绝缘故障 */
    bool prechargeAbortedDueToVoltage[BS_NR_OF_STRINGS];                 /*!< false -> 无错误, true -> 因电压中止预充 */
    bool prechargeAbortedDueToCurrent[BS_NR_OF_STRINGS];                 /*!< false -> 无错误, true -> 因电流中止预充 */
    bool deepDischargeDetectedError[BS_NR_OF_STRINGS];                   /*!< false -> 无错误, true -> 检测到深度放电错误 */
    bool currentOnOpenStringDetectedError[BS_NR_OF_STRINGS];             /*!< false -> 无错误, true -> 断开串上检测到电流错误 */
    bool mcuDieTemperatureViolationError;                                /*!< false -> 无错误, true -> MCU芯片温度违规错误 */
    bool mcuSbcFinError;                                                 /*!< false -> 无错误, true -> 错误: 对RSTB短路 */
    bool mcuSbcRstbError;                                                /*!< false -> 无错误, true -> 错误: RSTB不工作 */
    bool pexI2cCommunicationError;                                       /*!< I2C端口扩展器未按预期工作 */
    bool i2cRtcError;                                                    /*!< 与RTC的I2C通信出现问题 */
    bool framReadCrcError;                                               /*!< 读取的CRC与读取数据的CRC不匹配时为true，否则为false */
    bool rtcClockIntegrityError;                                         /*!< RTC时间完整性无法保证，因为振荡器已停止 */
    bool rtcBatteryLowError;                                             /*!< RTC电池电压低 */
    bool taskEngineTimingViolationError;                                 /*!< 引擎任务中的时序违规 */
    bool task1msTimingViolationError;                                    /*!< 1ms任务中的时序违规 */
    bool task10msTimingViolationError;                                   /*!< 10ms任务中的时序违规 */
    bool task100msTimingViolationError;                                  /*!< 100ms任务中的时序违规 */
    bool task100msAlgoTimingViolationError;                              /*!< 100ms算法任务中的时序违规 */
    bool alertFlagSetError;                                              /*!< true: 检测到ALERT状况, false: 一切正常 */
    bool aerosolAlert;                                                   /*!< true: 检测到高浓度气溶胶 */
    bool supplyVoltageClamp30cError;                                     /*!< false -> 检测到30C供电电压, true: 30C无电压 */
    bool afeAlarmLineError;                                              /*!< true: 检测到报警线错误 */
} DATA_BLOCK_ERROR_STATE_s;

/** 接触器反馈数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header; /*!< 数据块头 */
    uint32_t contactorFeedback; /*!< 所有接触器的反馈，不包括互锁 */
} DATA_BLOCK_CONTACTOR_FEEDBACK_s;

/** 互锁反馈数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                   /*!< 数据块头 */
    uint8_t interlockFeedback_IL_STATE;           /*!< 互锁反馈，连接到引脚 */
    float_t interlockVoltageFeedback_IL_HS_VS_mV; /*!< 互锁电压反馈，连接到ADC输入2 */
    float_t interlockVoltageFeedback_IL_LS_VS_mV; /*!< 互锁电压反馈，连接到ADC输入3 */
    float_t interlockCurrentFeedback_IL_HS_CS_mA; /*!< 互锁电流反馈，连接到ADC输入4 */
    float_t interlockCurrentFeedback_IL_LS_CS_mA; /*!< 互锁电流反馈，连接到ADC输入5 */
} DATA_BLOCK_INTERLOCK_FEEDBACK_s;

/** SOF(功能状态)限制数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                        /*!< 数据块头 */
    float_t recommendedContinuousPackChargeCurrent_mA; /*!< 推荐的连续电池包充电电流 */
    float_t
        recommendedContinuousPackDischargeCurrent_mA; /*!< 推荐的连续电池包放电电流 */
    float_t recommendedPeakPackChargeCurrent_mA;      /*!< 推荐的峰值电池包充电电流 */
    float_t recommendedPeakPackDischargeCurrent_mA;   /*!< 推荐的峰值电池包放电电流 */
    float_t
        recommendedContinuousChargeCurrent_mA[BS_NR_OF_STRINGS]; /*!< 推荐的连续充电电流 */
    float_t recommendedContinuousDischargeCurrent_mA[BS_NR_OF_STRINGS]; /*!< 推荐的连续放电电流 */
    float_t recommendedPeakChargeCurrent_mA[BS_NR_OF_STRINGS];    /*!< 推荐的峰值充电电流 */
    float_t recommendedPeakDischargeCurrent_mA[BS_NR_OF_STRINGS]; /*!< 推荐的峰值放电电流 */
} DATA_BLOCK_SOF_s;

/** 系统状态数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header; /*!< 数据块头 */
    int32_t bmsCanState;        /*!< 用于CAN消息的系统状态 (如待机、正常) */
} DATA_BLOCK_SYSTEM_STATE_s;

/** 最大安全限制(MSL)数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                           /*!< 数据块头 */
    uint8_t packChargeOvercurrent;                        /*!< 0 -> 未违反MSL, 1 -> 违反MSL(包充电过流) */
    uint8_t packDischargeOvercurrent;                     /*!< 0 -> 未违反MSL, 1 -> 违反MSL(包放电过流) */
    uint8_t overVoltage[BS_NR_OF_STRINGS];                /*!< 0 -> 未违反MSL, 1 -> 违反MSL(过压) */
    uint8_t underVoltage[BS_NR_OF_STRINGS];               /*!< 0 -> 未违反MSL, 1 -> 违反MSL(欠压) */
    uint8_t overtemperatureCharge[BS_NR_OF_STRINGS];      /*!< 0 -> 未违反MSL, 1 -> 违反MSL(充电高温) */
    uint8_t overtemperatureDischarge[BS_NR_OF_STRINGS];   /*!< 0 -> 未违反MSL, 1 -> 违反MSL(放电高温) */
    uint8_t undertemperatureCharge[BS_NR_OF_STRINGS];     /*!< 0 -> 未违反MSL, 1 -> 违反MSL(充电低温) */
    uint8_t undertemperatureDischarge[BS_NR_OF_STRINGS];  /*!< 0 -> 未违反MSL, 1 -> 违反MSL(放电低温) */
    uint8_t cellChargeOvercurrent[BS_NR_OF_STRINGS];      /*!< 0 -> 未违反MSL, 1 -> 违反MSL(单体充电过流) */
    uint8_t stringChargeOvercurrent[BS_NR_OF_STRINGS];    /*!< 0 -> 未违反MSL, 1 -> 违反MSL(串充电过流) */
    uint8_t cellDischargeOvercurrent[BS_NR_OF_STRINGS];   /*!< 0 -> 未违反MSL, 1 -> 违反MSL(单体放电过流) */
    uint8_t stringDischargeOvercurrent[BS_NR_OF_STRINGS]; /*!< 0 -> 未违反MSL, 1 -> 违反MSL(串放电过流) */
    uint8_t pcbOvertemperature[BS_NR_OF_STRINGS];         /*!< 0 -> 未违反MSL, 1 -> 违反MSL(PCB高温) */
    uint8_t pcbUndertemperature[BS_NR_OF_STRINGS];        /*!< 0 -> 未违反MSL, 1 -> 违反MSL(PCB低温) */
} DATA_BLOCK_MSL_FLAG_s;

/** 推荐安全限制(RSL)数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                           /*!< 数据块头 */
    uint8_t overVoltage[BS_NR_OF_STRINGS];                /*!< 0 -> 未违反RSL, 1 -> 违反RSL(过压) */
    uint8_t underVoltage[BS_NR_OF_STRINGS];               /*!< 0 -> 未违反RSL, 1 -> 违反RSL(欠压) */
    uint8_t overtemperatureCharge[BS_NR_OF_STRINGS];      /*!< 0 -> 未违反RSL, 1 -> 违反RSL(充电高温) */
    uint8_t overtemperatureDischarge[BS_NR_OF_STRINGS];   /*!< 0 -> 未违反RSL, 1 -> 违反RSL(放电高温) */
    uint8_t undertemperatureCharge[BS_NR_OF_STRINGS];     /*!< 0 -> 未违反RSL, 1 -> 违反RSL(充电低温) */
    uint8_t undertemperatureDischarge[BS_NR_OF_STRINGS];  /*!< 0 -> 未违反RSL, 1 -> 违反RSL(放电低温) */
    uint8_t cellChargeOvercurrent[BS_NR_OF_STRINGS];      /*!< 0 -> 未违反RSL, 1 -> 违反RSL(单体充电过流) */
    uint8_t stringChargeOvercurrent[BS_NR_OF_STRINGS];    /*!< 0 -> 未违反RSL, 1 -> 违反RSL(串充电过流) */
    uint8_t cellDischargeOvercurrent[BS_NR_OF_STRINGS];   /*!< 0 -> 未违反RSL, 1 -> 违反RSL(单体放电过流) */
    uint8_t stringDischargeOvercurrent[BS_NR_OF_STRINGS]; /*!< 0 -> 未违反RSL, 1 -> 违反RSL(串放电过流) */
    uint8_t pcbOvertemperature[BS_NR_OF_STRINGS];         /*!< 0 -> 未违反RSL, 1 -> 违反RSL(PCB高温) */
    uint8_t pcbUndertemperature[BS_NR_OF_STRINGS];        /*!< 0 -> 未违反RSL, 1 -> 违反RSL(PCB低温) */
} DATA_BLOCK_RSL_FLAG_s;

/** 最大运行限制(MOL)数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                           /*!< 数据块头 */
    uint8_t overVoltage[BS_NR_OF_STRINGS];                /*!< 0 -> 未违反MOL, 1 -> 违反MOL(过压) */
    uint8_t underVoltage[BS_NR_OF_STRINGS];               /*!< 0 -> 未违反MOL, 1 -> 违反MOL(欠压) */
    uint8_t overtemperatureCharge[BS_NR_OF_STRINGS];      /*!< 0 -> 未违反MOL, 1 -> 违反MOL(充电高温) */
    uint8_t overtemperatureDischarge[BS_NR_OF_STRINGS];   /*!< 0 -> 未违反MOL, 1 -> 违反MOL(放电高温) */
    uint8_t undertemperatureCharge[BS_NR_OF_STRINGS];     /*!< 0 -> 未违反MOL, 1 -> 违反MOL(充电低温) */
    uint8_t undertemperatureDischarge[BS_NR_OF_STRINGS];  /*!< 0 -> 未违反MOL, 1 -> 违反MOL(放电低温) */
    uint8_t cellChargeOvercurrent[BS_NR_OF_STRINGS];      /*!< 0 -> 未违反MOL, 1 -> 违反MOL(单体充电过流) */
    uint8_t stringChargeOvercurrent[BS_NR_OF_STRINGS];    /*!< 0 -> 未违反MOL, 1 -> 违反MOL(串充电过流) */
    uint8_t cellDischargeOvercurrent[BS_NR_OF_STRINGS];   /*!< 0 -> 未违反MOL, 1 -> 违反MOL(单体放电过流) */
    uint8_t stringDischargeOvercurrent[BS_NR_OF_STRINGS]; /*!< 0 -> 未违反MOL, 1 -> 违反MOL(串放电过流) */
    uint8_t pcbOvertemperature[BS_NR_OF_STRINGS];         /*!< 0 -> 未违反MOL, 1 -> 违反MOL(PCB高温) */
    uint8_t pcbUndertemperature[BS_NR_OF_STRINGS];        /*!< 0 -> 未违反MOL, 1 -> 违反MOL(PCB低温) */
} DATA_BLOCK_MOL_FLAG_s;

/** SOC(荷电状态)数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                       /*!< 数据块头 */
    float_t averageSoc_perc[BS_NR_OF_STRINGS];        /*!< 平均SOC: 0.0 <= averageSoc <= 100.0 */
    float_t minimumSoc_perc[BS_NR_OF_STRINGS];        /*!< 最小SOC: 0.0 <= minSoc <= 100.0 */
    float_t maximumSoc_perc[BS_NR_OF_STRINGS];        /*!< 最大SOC: 0.0 <= maxSoc <= 100.0 */
    float_t chargeThroughput_As[BS_NR_OF_STRINGS];    /*!< 总充电吞吐量 */
    float_t dischargeThroughput_As[BS_NR_OF_STRINGS]; /*!< 总放电吞吐量 */
} DATA_BLOCK_SOC_s;

/** SOH(健康状态)数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                /*!< 数据块头 */
    float_t averageSoh_perc[BS_NR_OF_STRINGS]; /*!< 平均SOH: 0.0 <= averageSoh <= 100.0 */
    float_t minimumSoh_perc[BS_NR_OF_STRINGS]; /*!< 最小SOH: 0.0 <= minimumSoh <= 100.0  */
    float_t maximumSoh_perc[BS_NR_OF_STRINGS]; /*!< 最大SOH: 0.0 <= maximumSoh <= 100.0  */
} DATA_BLOCK_SOH_s;

/** SOE(能量状态)数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                             /*!< 数据块头 */
    float_t averageSoe_perc[BS_NR_OF_STRINGS];              /*!< 平均SOE: 0.0 <= averageSoe <= 100.0 */
    float_t minimumSoe_perc[BS_NR_OF_STRINGS];              /*!< 最小SOE: 0.0 <= minimumSoe <= 100.0  */
    float_t maximumSoe_perc[BS_NR_OF_STRINGS];              /*!< 最大SOE: 0.0 <= maximumSoe <= 100.0  */
    uint32_t maximumSoe_Wh[BS_NR_OF_STRINGS];               /*!< 最大串能量，单位：Wh */
    uint32_t averageSoe_Wh[BS_NR_OF_STRINGS];               /*!< 平均串能量，单位：Wh */
    uint32_t minimumSoe_Wh[BS_NR_OF_STRINGS];               /*!< 最小串能量，单位：Wh */
    float_t chargeEnergyThroughput_Wh[BS_NR_OF_STRINGS];    /*!< 充入能量 */
    float_t dischargeEnergyThroughput_Wh[BS_NR_OF_STRINGS]; /*!< 放出能量 */
} DATA_BLOCK_SOE_s;

/** CAN状态请求数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;         /*!< 数据块头 */
    uint8_t stateRequestViaCan;         /*!< 状态请求 */
    uint8_t previousStateRequestViaCan; /*!< 上一次的状态请求 */
    uint8_t stateRequestViaCanPending;  /*!< 待处理的状态请求 */
    uint8_t stateCounter;               /*!< 状态更新计数 */
} DATA_BLOCK_STATE_REQUEST_s;

/** 滑动平均算法数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                          /*!< 数据块头 */
    float_t movingAverageCurrent1sInterval_mA;           /*!< 过去1秒的电流滑动平均值 */
    float_t movingAverageCurrent5sInterval_mA;           /*!< 过去5秒的电流滑动平均值 */
    float_t movingAverageCurrent10sInterval_mA;          /*!< 过去10秒的电流滑动平均值 */
    float_t movingAverageCurrent30sInterval_mA;          /*!< 过去30秒的电流滑动平均值 */
    float_t movingAverageCurrent60sInterval_mA;          /*!< 过去60秒的电流滑动平均值 */
    float_t movingAverageCurrentConfigurableInterval_mA; /*!< 过去配置时间的电流滑动平均值 */
    float_t movingAveragePower1sInterval_mA;             /*!< 过去1秒的功率滑动平均值 */
    float_t movingAveragePower5sInterval_mA;             /*!< 过去5秒的功率滑动平均值 */
    float_t movingAveragePower10sInterval_mA;            /*!< 过去10秒的功率滑动平均值 */
    float_t movingAveragePower30sInterval_mA;            /*!< 过去30秒的功率滑动平均值 */
    float_t movingAveragePower60sInterval_mA;            /*!< 过去60秒的功率滑动平均值 */
    float_t movingAveragePowerConfigurableInterval_mA;   /*!< 过去配置时间的功率滑动平均值 */
} DATA_BLOCK_MOVING_AVERAGE_s;

/** 绝缘监测设备测量数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;         /*!< 数据块头 */
    bool isImdRunning;                  /*!< true -> 绝缘电阻测量激活, false -> 未激活 */
    bool isMeasurementValid;            /*!< true -> 电阻值有效, false -> 电阻值不可靠 */
    uint32_t insulationResistance_kOhm; /*!< 测量的绝缘电阻，单位：kOhm */
    bool areDeviceFlagsValid; /*!< true -> 此数据库条目下方的标志有效, false -> 标志不可靠(例如检测到设备错误) */
    bool dfIsCriticalResistanceDetected; /*!< 设备状态标志: false -> 电阻值正常, true -> 电阻值过低/错误 */
    bool dfIsWarnableResistanceDetected; /*!< true: 违反警告阈值, false: 无警告激活 */
    bool dfIsChassisFaultDetected;       /*!< true: 检测到HV电位与底盘短路, false: 无错误 */
    bool dfIsChassisShortToHvPlus;       /*!< true: 绝缘故障位置偏向HV正极 */
    bool dfIsChassisShortToHvMinus;      /*!< true: 绝缘故障位置偏向HV负极 */
    bool dfIsDeviceErrorDetected;        /*!< true: 检测到设备错误, false: 未检测到错误 */
    bool dfIsMeasurementUpToDate;        /*!< true: 测量最新, false: 已过时 */
} DATA_BLOCK_INSULATION_s;

/** I2C温湿度传感器数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header; /*!< 数据块头 */
    int16_t temperature_ddegC;  /*!< 温度，单位：0.1 °C */
    uint8_t humidity_perc;      /*!< 湿度，单位：% */
} DATA_BLOCK_HTSEN_s;

/** 内部ADC电压测量数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;                                 /*!< 数据块头 */
    float_t adc1ConvertedVoltages_mV[MCU_ADC1_MAX_NR_CHANNELS]; /*!< 内部ADC ADC1测量的电压 */
} DATA_BLOCK_ADC_VOLTAGE_s;

/** 数据库内置自检数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header; /*!< 数据块头 */
    uint8_t member1;            /*!< 自检结构体的第一个成员 */
    uint8_t member2;            /*!< 自检结构体的第二个成员 */
} DATA_BLOCK_DUMMY_FOR_SELF_TEST_s;

/** BAS6C-X00气溶胶传感器数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header;              /*!< 数据块头 */
    uint8_t sensorStatus;                    /*!< 0: 正常, 1: 报警, 2: 保留 */
    bool photoelectricError;                 /*!< 传感器光电装置故障时为true */
    bool supplyOvervoltageError;             /*!< 供电过压时为true */
    bool supplyUndervoltageError;            /*!< 供电欠压时为true */
    uint16_t particulateMatterConcentration; /*!< 颗粒物浓度，单位：微克/立方米 */
    uint8_t crcCheckCode;                    /*!< CRC校验码 */
} DATA_BLOCK_AEROSOL_SENSOR_s;

/** PHY(物理层)数据块结构体 */
typedef struct {
    /* 此结构体必须位于每个数据库条目的开头。在初始化数据库结构体时，
       必须将uniqueId设置为枚举DATA_BLOCK_ID_e中相应的数据库条目表示。 */
    DATA_BLOCK_HEADER_s header; /*!< 数据块头 */
    bool initialized;           /*!< PHY初始化完成时为true */
    bool aliveStatus;           /*!< PHY可达时为true */
    bool linkStatus;            /*!< PHY连接到网络时为true */
} DATA_BLOCK_PHY_s;

/** 数据库数组 */
extern DATA_BASE_s data_database[DATA_BLOCK_ID_MAX];

/*========== 外部常量与变量声明 ==============================================*/

/*========== 外部函数原型 ===================================================*/

/*========== 外部化的静态函数原型（单元测试） ================================*/
#ifdef UNITY_UNIT_TEST
#endif

#endif /* FOXBMS__DATABASE_CFG_H_ */
