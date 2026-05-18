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
 * @file    can_cfg.c
 * @author  foxBMS Team
 * @date    2019-12-04 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup DRIVERS_CONFIGURATION
 * @prefix  CAN
 *
 * @brief   CAN模块配置
 * @details 在此处指定CAN总线设置、接收的消息及其接收处理方式。
 */

/*========== 包含文件 =======================================================*/
#include "can_cfg.h"

#include "database.h"
#include "ftask.h"

#include <stdint.h>

/*========== 宏与定义 =======================================================*/

/*========== 静态常量与变量定义 ==============================================*/

/*========== 外部常量与变量定义 ==============================================*/
/** CAN节点1配置，使用canREG1寄存器 */
const CAN_NODE_s can_node1 = {
    .canNodeRegister = canREG1,
};

/** CAN节点2(隔离)配置，使用canREG2寄存器 */
const CAN_NODE_s can_node2Isolated = {
    .canNodeRegister = canREG2,
};

/** 数据库表的本地副本 */
/**@{*/
static DATA_BLOCK_CELL_TEMPERATURE_s can_tableTemperatures = {.header.uniqueId = DATA_BLOCK_ID_CELL_TEMPERATURE};             /*!< 静态单体温度数据表，初始化唯一标识为单体温度ID */
static DATA_BLOCK_CELL_VOLTAGE_s can_tableCellVoltages     = {.header.uniqueId = DATA_BLOCK_ID_CELL_VOLTAGE};                 /*!< 静态单体电压数据表，初始化唯一标识为单体电压ID */
static DATA_BLOCK_CURRENT_s can_tableCurrent               = {.header.uniqueId = DATA_BLOCK_ID_CURRENT};                     /*!< 静态电流测量数据表，初始化唯一标识为电流ID */
static DATA_BLOCK_CURRENT_SENSOR_TEMPERATURE_s can_tableCurrentSensorTemperature = {                   /*!< 静态电流传感器温度数据表，初始化唯一标识为电流传感器温度ID */
    .header.uniqueId = DATA_BLOCK_ID_CURRENT_SENSOR_TEMPERATURE};                                                                /*!< 电流传感器温度ID赋值 */
static DATA_BLOCK_POWER_s can_tablePower                        = {.header.uniqueId = DATA_BLOCK_ID_POWER};                    /*!< 静态功率测量数据表，初始化唯一标识为功率ID */
static DATA_BLOCK_CURRENT_COUNTER_s can_tableCurrentCounter     = {.header.uniqueId = DATA_BLOCK_ID_CURRENT_COUNTER};          /*!< 静态电流计数(库仑计)数据表，初始化唯一标识为电流计数ID */
static DATA_BLOCK_ENERGY_COUNTER_s can_tableEnergyCounter       = {.header.uniqueId = DATA_BLOCK_ID_ENERGY_COUNTER};           /*!< 静态能量计数数据表，初始化唯一标识为能量计数ID */
static DATA_BLOCK_SYSTEM_VOLTAGE_1_s can_tableSystemVoltage1    = {.header.uniqueId = DATA_BLOCK_ID_SYSTEM_VOLTAGE_1};         /*!< 静态系统电压U1数据表，初始化唯一标识为系统电压1 ID */
static DATA_BLOCK_SYSTEM_VOLTAGE_2_s can_tableSystemVoltage2    = {.header.uniqueId = DATA_BLOCK_ID_SYSTEM_VOLTAGE_2};         /*!< 静态系统电压U2数据表，初始化唯一标识为系统电压2 ID */
static DATA_BLOCK_SYSTEM_VOLTAGE_3_s can_tableSystemVoltage3    = {.header.uniqueId = DATA_BLOCK_ID_SYSTEM_VOLTAGE_3};         /*!< 静态系统电压U3数据表，初始化唯一标识为系统电压3 ID */
static DATA_BLOCK_ERROR_STATE_s can_tableErrorState             = {.header.uniqueId = DATA_BLOCK_ID_ERROR_STATE};              /*!< 静态错误状态数据表，初始化唯一标识为错误状态ID */
static DATA_BLOCK_INSULATION_s can_tableInsulation              = {.header.uniqueId = DATA_BLOCK_ID_INSULATION};               /*!< 静态绝缘监测数据表，初始化唯一标识为绝缘ID */
static DATA_BLOCK_MIN_MAX_s can_tableMinimumMaximumValues       = {.header.uniqueId = DATA_BLOCK_ID_MIN_MAX};                  /*!< 静态最小/最大值数据表，初始化唯一标识为最小最大值ID */
static DATA_BLOCK_MOL_FLAG_s can_tableMolFlags                  = {.header.uniqueId = DATA_BLOCK_ID_MOL_FLAG};                 /*!< 静态MOL(最大运行限制)标志数据表，初始化唯一标识为MOL标志ID */
static DATA_BLOCK_MSL_FLAG_s can_tableMslFlags                  = {.header.uniqueId = DATA_BLOCK_ID_MSL_FLAG};                 /*!< 静态MSL(最大安全限制)标志数据表，初始化唯一标识为MSL标志ID */
static DATA_BLOCK_OPEN_WIRE_s can_tableOpenWire                 = {.header.uniqueId = DATA_BLOCK_ID_OPEN_WIRE_BASE};            /*!< 静态开路状态数据表，初始化唯一标识为开路基准ID */
static DATA_BLOCK_PACK_VALUES_s can_tablePackValues             = {.header.uniqueId = DATA_BLOCK_ID_PACK_VALUES};              /*!< 静态电池包数值数据表，初始化唯一标识为电池包值ID */
static DATA_BLOCK_RSL_FLAG_s can_tableRslFlags                  = {.header.uniqueId = DATA_BLOCK_ID_RSL_FLAG};                 /*!< 静态RSL(推荐安全限制)标志数据表，初始化唯一标识为RSL标志ID */
static DATA_BLOCK_SOC_s can_tableSoc                            = {.header.uniqueId = DATA_BLOCK_ID_SOC};                      /*!< 静态SOC(荷电状态)数据表，初始化唯一标识为SOC ID */
static DATA_BLOCK_SOE_s can_tableSoe                            = {.header.uniqueId = DATA_BLOCK_ID_SOE};                      /*!< 静态SOE(能量状态)数据表，初始化唯一标识为SOE ID */
static DATA_BLOCK_SOF_s can_tableSof                            = {.header.uniqueId = DATA_BLOCK_ID_SOF};                      /*!< 静态SOF(功能状态)数据表，初始化唯一标识为SOF ID */
static DATA_BLOCK_SOH_s can_tableSoh                            = {.header.uniqueId = DATA_BLOCK_ID_SOH};                      /*!< 静态SOH(健康状态)数据表，初始化唯一标识为SOH ID */
static DATA_BLOCK_STATE_REQUEST_s can_tableStateRequest         = {.header.uniqueId = DATA_BLOCK_ID_STATE_REQUEST};            /*!< 静态状态请求数据表，初始化唯一标识为状态请求ID */
static DATA_BLOCK_AEROSOL_SENSOR_s can_tableAerosolSensor       = {.header.uniqueId = DATA_BLOCK_ID_AEROSOL_SENSOR};           /*!< 静态气溶胶传感器数据表，初始化唯一标识为气溶胶传感器ID */
static DATA_BLOCK_BALANCING_CONTROL_s can_tableBalancingControl = {.header.uniqueId = DATA_BLOCK_ID_BALANCING_CONTROL};        /*!< 静态均衡控制数据表，初始化唯一标识为均衡控制ID */
static DATA_BLOCK_PHY_s can_tablePhy                            = {.header.uniqueId = DATA_BLOCK_ID_PHY};                      /*!< 静态PHY(物理层)数据表，初始化唯一标识为PHY ID */

/**@}*/

/** CAN Shim结构体：用于存储和传递本地数据库表句柄及IMD队列 */
const CAN_SHIM_s can_kShim = {
    .pQueueImd                      = &ftsk_imdCanDataQueue,             /*!< IMD消息队列句柄 */
    .pTableCellTemperature          = &can_tableTemperatures,            /*!< 单体温度数据表指针 */
    .pTableCellVoltage              = &can_tableCellVoltages,            /*!< 单体电压数据表指针 */
    .pTableCurrent                  = &can_tableCurrent,                 /*!< 电流测量数据表指针 */
    .pTableCurrentSensorTemperature = &can_tableCurrentSensorTemperature,/*!< 电流传感器温度数据表指针 */
    .pTablePower                    = &can_tablePower,                   /*!< 功率测量数据表指针 */
    .pTableCurrentCounter           = &can_tableCurrentCounter,          /*!< 电流计数数据表指针 */
    .pTableEnergyCounter            = &can_tableEnergyCounter,           /*!< 能量计数数据表指针 */
    .pTableSystemVoltage1           = &can_tableSystemVoltage1,          /*!< 系统电压U1数据表指针 */
    .pTableSystemVoltage2           = &can_tableSystemVoltage2,          /*!< 系统电压U2数据表指针 */
    .pTableSystemVoltage3           = &can_tableSystemVoltage3,          /*!< 系统电压U3数据表指针 */
    .pTableErrorState               = &can_tableErrorState,              /*!< 错误状态数据表指针 */
    .pTableInsulation               = &can_tableInsulation,              /*!< 绝缘监测数据表指针 */
    .pTableMinMax                   = &can_tableMinimumMaximumValues,    /*!< 最小/最大值数据表指针 */
    .pTableMol                      = &can_tableMolFlags,                /*!< MOL(最大运行限制)标志数据表指针 */
    .pTableMsl                      = &can_tableMslFlags,                /*!< MSL(最大安全限制)标志数据表指针 */
    .pTableOpenWire                 = &can_tableOpenWire,                /*!< 开路状态数据表指针 */
    .pTablePackValues               = &can_tablePackValues,              /*!< 电池包数值数据表指针 */
    .pTableRsl                      = &can_tableRslFlags,                /*!< RSL(推荐安全限制)标志数据表指针 */
    .pTableSoc                      = &can_tableSoc,                     /*!< SOC值数据表指针 */
    .pTableSoe                      = &can_tableSoe,                     /*!< SOE值数据表指针 */
    .pTableSof                      = &can_tableSof,                     /*!< SOF值数据表指针 */
    .pTableSoh                      = &can_tableSoh,                     /*!< SOH值数据表指针 */
    .pTableStateRequest             = &can_tableStateRequest,            /*!< 状态请求数据表指针 */
    .pTableAerosolSensor            = &can_tableAerosolSensor,           /*!< 气溶胶传感器数据表指针 */
    .pTableBalancingControl         = &can_tableBalancingControl,        /*!< 均衡控制数据表指针 */
    .pTablePhy                      = &can_tablePhy,                     /*!< PHY(物理层)数据表指针 */
};

/*========== 静态函数原型 ===================================================*/

/*========== 静态函数实现 ====================================================*/

/*========== 外部函数实现 ====================================================*/

/*========== 外部化的静态函数实现（单元测试） ================================*/
#ifdef UNITY_UNIT_TEST
#endif
