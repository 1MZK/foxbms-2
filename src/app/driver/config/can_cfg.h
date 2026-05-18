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
 * @file    can_cfg.h
 * @author  foxBMS Team
 * @date    2019-12-04 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup DRIVERS
 * @prefix  CAN
 *
 * @brief   CAN模块配置的头文件
 * @details 在此处配置消息缓冲区的激活状态、长度以及接收的消息数量。
 */

#ifndef FOXBMS__CAN_CFG_H_
#define FOXBMS__CAN_CFG_H_

/*========== 包含文件 =======================================================*/
#include "foxbms_config.h"

#include "HL_can.h"

#include "database.h"

#include <stdint.h>

/*========== 宏与定义 =======================================================*/

/* ****************************************************************************
 *  CAN节点选项
 *****************************************************************************/
/** CAN节点配置结构体 */
typedef struct {
    canBASE_t *canNodeRegister; /*!< CAN接口所连接的寄存器 */
} CAN_NODE_s;

/** CAN节点定义 @{*/
#define CAN_NODE_1 ((CAN_NODE_s *)&can_node1)
#define CAN_NODE_2 ((CAN_NODE_s *)&can_node2Isolated)

#define CAN_NODE_DEBUG_MESSAGE       (CAN_NODE_1) /*!< 调试消息使用的CAN节点 */
#define CAN_NODE_IMD                 (CAN_NODE_1) /*!< 绝缘监测(IMD)使用的CAN节点 */
#define CAN_NODE_FATAL_ERROR_MESSAGE (CAN_NODE_1) /*!< 致命错误消息使用的CAN节点 */
#define CAN_NODE_CURRENT_SENSOR      (CAN_NODE_1) /*!< 电流传感器使用的CAN节点 */
#if (defined(FOXBMS_AFE_DRIVER_DEBUG_CAN) && (FOXBMS_AFE_DRIVER_DEBUG_CAN == 1))
#define CAN_NODE_RX_CELL_VOLTAGES     (CAN_NODE_1) /*!< 接收单体电压使用的CAN节点 */
#define CAN_NODE_RX_CELL_TEMPERATURES (CAN_NODE_1) /*!< 接收单体温度使用的CAN节点 */
#endif
/**@}*/

/**
 * CAN收发器引脚到相应端口扩展器引脚的配置。
 * @{
 */
/** CAN1所连接的IO寄存器方向控制 */
#define CAN_CAN1_IO_REG_DIR  (hetREG2->DIR)
/** CAN1所连接的IO寄存器数据输出 */
#define CAN_CAN1_IO_REG_DOUT (hetREG2->DOUT)
/** CAN1使能引脚 */
#define CAN_CAN1_ENABLE_PIN  (18u)
/** CAN1待机引脚 */
#define CAN_CAN1_STANDBY_PIN (23u)
/** CAN2使能引脚 */
#define CAN_CAN2_ENABLE_PIN  (PEX_PORT_0_PIN_2)
/** CAN2待机引脚 */
#define CAN_CAN2_STANDBY_PIN (PEX_PORT_0_PIN_3)
/**@}*/

/** 使用11位ID时的最大ID值 */
#define CAN_MAX_11BIT_ID (2048u)
/** 数据长度代码(DLC)的最大长度 */
#define CAN_MAX_DLC (8u)
/** 默认数据长度代码(DLC) */
#define CAN_DEFAULT_DLC (8u)
/** foxBMS项目自定义消息的默认DLC（即非第三方软件/硬件定义的消息） */
#define CAN_FOXBMS_MESSAGES_DEFAULT_DLC (8u)
/** 用于CAN消息布局配置的1位长度 */
#define CAN_BIT (1u)

#if (defined(FOXBMS_AFE_DRIVER_DEBUG_CAN) && (FOXBMS_AFE_DRIVER_DEBUG_CAN == 1))
/** 每条CAN消息接收的单体电压数量 */
#define CAN_NUM_OF_VOLTAGES_IN_CAN_CELL_VOLTAGES_MSG (4u)
/** 每条CAN消息接收的单体温度数量 */
#define CAN_NUM_OF_TEMPERATURES_IN_CAN_CELL_TEMPERATURES_MSG (6u)
#endif

/** CAN信号准备时的零偏移量 */
#define CAN_SIGNAL_OFFSET_0 (0.0f)

/* **************************************************************************************
 *  CAN BUFFER OPTIONS
 *****************************************************************************************/

/** Enum for byte order (endianness)
 * \verbatim
 * CAN data example:
 *
 * LittleEndian
 * bitStart = 27; bitLength = 19
 * DataLE: 45-44-43-42-41-40-39-38-37-36-35-34-33-32-31-30-29-28-27
 *         MSB                                                   LSB
 *
 * BigEndian
 * bitStart = 21; bitLength = 19
 * DataBE: 21-20-19-18-17-16-31-30-29-28-27-26-25-24-39-38-37-36-35
 *         MSB                                                   LSB
 *                                             |||
 *                                   Receive data on CAN bus
 *                                             |||
 *                                             |||
 *                                             \_/
 *                  LE                          |                  BE
 * CAN Data Byte 0  07 06 05 04 03 02 01 00     | CAN Data Byte 0  07 06 05 04 03 02 01 00
 * CAN Data Byte 1  15 14 13 12 11 10 09 08     | CAN Data Byte 1  15 14 13 12 11 10 09 08
 * CAN Data Byte 2  23 22 21 20 19 18 17 16     | CAN Data Byte 2  23 22 21-20-19-18-17-16 MSB
 * CAN Data Byte 3  31-30-29-28-27 26 25 24 LSB | CAN Data Byte 3  31-30-29-28-27-26-25-24
 * CAN Data Byte 4  39-38-37-36-35-34-33-32     | CAN Data Byte 4  39-38-37-36-35 34 33 32 LSB
 * CAN Data Byte 5  47 46 45-44-43-42-41-40 MSB | CAN Data Byte 5  47 46 45 44 43 42 41 40
 * CAN Data Byte 6  55 54 53 52 51 50 49 48     | CAN Data Byte 6  55 54 53 52 51 50 49 48
 * CAN Data Byte 7  63 62 61 60 59 58 57 56     | CAN Data Byte 7  63 62 61 60 59 58 57 56
 *                                             |||
 *                                  Store received data in RAM
 *                                             |||
 *                                             |||
 *                                             \_/
 *                  LE                          |                  BE
 * CAN Data Byte 7  63 62 61 60 59 58 57 56     | CAN Data Byte 0  07 06 05 04 03 02 01 00     | RAM data[7]
 * CAN Data Byte 6  55 54 53 52 51 50 49 48     | CAN Data Byte 1  15 14 13 12 11 10 09 08     | RAM data[6]
 * CAN Data Byte 5  47 46 45-44-43-42-41-40 MSB | CAN Data Byte 2  23 22 21-20-19-18-17-16 MSB | RAM data[5]
 * CAN Data Byte 4  39-38-37-36-35-34-33-32     | CAN Data Byte 3  31-30-29-28-27-26-25-24     | RAM data[4]
 * CAN Data Byte 3  31-30-29-28-27 26 25 24 LSB | CAN Data Byte 4  39-38-37-36-35 34 33 32 LSB | RAM data[3]
 * CAN Data Byte 2  23 22 21 20 19 18 17 16     | CAN Data Byte 5  47 46 45 44 43 42 41 40     | RAM data[2]
 * CAN Data Byte 1  15 14 13 12 11 10 09 08     | CAN Data Byte 6  55 54 53 52 51 50 49 48     | RAM data[1]
 * CAN Data Byte 0  07 06 05 04 03 02 01 00     | CAN Data Byte 7  63 62 61 60 59 58 57 56     | RAM data[0]
 *                                      DataLE  =  DataBE
 * \endverbatim
 */
C
 复制
 插入
 新文件

/** CAN字节序枚举 */
typedef enum {
    CAN_LITTLE_ENDIAN, /*!< 小端模式 */
    CAN_BIG_ENDIAN,    /*!< 大端模式 */
} CAN_ENDIANNESS_e;

/** CAN标识符类型：标准标识符或扩展标识符 */
typedef enum {
    CAN_STANDARD_IDENTIFIER_11_BIT, /*!< 标准11位标识符 */
    CAN_EXTENDED_IDENTIFIER_29_BIT, /*!< 扩展29位标识符 */
    CAN_INVALID_TYPE,               /*!< 无效类型 */
} CAN_IDENTIFIER_TYPE_e;

/** 缓冲区元素，用于存储CAN接收消息的ID和数据 */
typedef struct {
    CAN_NODE_s *canNode;          /*!< 接收到该消息的CAN节点 */
    uint32_t id;                  /*!< CAN消息的ID */
    CAN_IDENTIFIER_TYPE_e idType; /*!< 标准或扩展标识符 */
    uint8_t data[CAN_MAX_DLC];    /*!< CAN消息的有效载荷(数据) */
} CAN_BUFFER_ELEMENT_s;

#if (defined(FOXBMS_AFE_DRIVER_DEBUG_CAN) && (FOXBMS_AFE_DRIVER_DEBUG_CAN == 1))
/** 用于在 ftsk_canToAfeCellTemperaturesQueue 中传输的数据单元 */
typedef struct {
    uint8_t muxValue;                                                /*!< 复用值 */
    bool invalidFlag[CAN_NUM_OF_TEMPERATURES_IN_CAN_CELL_TEMPERATURES_MSG]; /*!< 温度无效标志 */
    int16_t cellTemperature[CAN_NUM_OF_TEMPERATURES_IN_CAN_CELL_TEMPERATURES_MSG]; /*!< 电池温度值 */
} CAN_CAN2AFE_CELL_TEMPERATURES_QUEUE_s;

/** 用于在 ftsk_canToAfeCellVoltagesQueue 中传输的数据单元 */
typedef struct {
    uint8_t muxValue;                                        /*!< 复用值 */
    bool invalidFlag[CAN_NUM_OF_VOLTAGES_IN_CAN_CELL_VOLTAGES_MSG]; /*!< 电压无效标志 */
    uint16_t cellVoltage[CAN_NUM_OF_VOLTAGES_IN_CAN_CELL_VOLTAGES_MSG]; /*!< 电池电压值 */
} CAN_CAN2AFE_CELL_VOLTAGES_QUEUE_s;
#endif

/** 复合类型，用于存储和传递本地数据库表的句柄 */
typedef struct {
    OS_QUEUE *pQueueImd;                                  /*!< 消息队列句柄 */
    DATA_BLOCK_CELL_VOLTAGE_s *pTableCellVoltage;         /*!< 数据库条目：单体电压 */
    DATA_BLOCK_CELL_TEMPERATURE_s *pTableCellTemperature; /*!< 数据库条目：单体温度 */
    DATA_BLOCK_CURRENT_s *pTableCurrent;                  /*!< 数据库条目：电流测量值 */
    DATA_BLOCK_CURRENT_SENSOR_TEMPERATURE_s
        *pTableCurrentSensorTemperature;                    /*!< 数据库条目：电流传感器温度 */
    DATA_BLOCK_POWER_s *pTablePower;                        /*!< 数据库条目：功率测量值 */
    DATA_BLOCK_CURRENT_COUNTER_s *pTableCurrentCounter;     /*!< 数据库条目：电流计数(库仑计) */
    DATA_BLOCK_ENERGY_COUNTER_s *pTableEnergyCounter;       /*!< 数据库条目：能量计数 */
    DATA_BLOCK_SYSTEM_VOLTAGE_1_s *pTableSystemVoltage1;    /*!< 数据库条目：系统电压U1测量值 */
    DATA_BLOCK_SYSTEM_VOLTAGE_2_s *pTableSystemVoltage2;    /*!< 数据库条目：电压U2测量值 */
    DATA_BLOCK_SYSTEM_VOLTAGE_3_s *pTableSystemVoltage3;    /*!< 数据库条目：电压U3测量值 */
    DATA_BLOCK_ERROR_STATE_s *pTableErrorState;             /*!< 数据库条目：错误状态变量 */
    DATA_BLOCK_INSULATION_s *pTableInsulation;              /*!< 数据库条目：绝缘监测信息 */
    DATA_BLOCK_MIN_MAX_s *pTableMinMax;                     /*!< 数据库条目：最小/最大值 */
    DATA_BLOCK_MOL_FLAG_s *pTableMol;                       /*!< 数据库条目：MOL(最大运行限制)标志 */
    DATA_BLOCK_MSL_FLAG_s *pTableMsl;                       /*!< 数据库条目：MSL(最大安全限制)标志 */
    DATA_BLOCK_OPEN_WIRE_s *pTableOpenWire;                 /*!< 数据库条目：开路状态 */
    DATA_BLOCK_PACK_VALUES_s *pTablePackValues;             /*!< 数据库条目：电池包数值 */
    DATA_BLOCK_RSL_FLAG_s *pTableRsl;                       /*!< 数据库条目：RSL(推荐安全限制)标志 */
    DATA_BLOCK_SOC_s *pTableSoc;                            /*!< 数据库条目：SOC值 */
    DATA_BLOCK_SOE_s *pTableSoe;                            /*!< 数据库条目：SOE值 */
    DATA_BLOCK_SOF_s *pTableSof;                            /*!< 数据库条目：SOF值 */
    DATA_BLOCK_SOH_s *pTableSoh;                            /*!< 数据库条目：SOH值 */
    DATA_BLOCK_STATE_REQUEST_s *pTableStateRequest;         /*!< 数据库条目：状态请求 */
    DATA_BLOCK_AEROSOL_SENSOR_s *pTableAerosolSensor;       /*!< 数据库条目：气溶胶传感器测量值 */
    DATA_BLOCK_BALANCING_CONTROL_s *pTableBalancingControl; /*!< 数据库条目：均衡信息 */
    DATA_BLOCK_PHY_s *pTablePhy;                            /*!< 数据库条目：PHY(物理层)信息 */
} CAN_SHIM_s;

/** CAN消息的定义（不包含数据部分） */
typedef struct {
    uint32_t id;                  /*!< 消息ID */
    CAN_IDENTIFIER_TYPE_e idType; /*!< 标准或扩展标识符 */
    uint8_t dlc;                  /*!< 数据长度代码 */
    CAN_ENDIANNESS_e endianness;  /*!< 字节序(大端或小端) */
} CAN_MESSAGE_PROPERTIES_s;

/** CAN发送(TX)消息的时序信息 */
typedef struct {
    uint32_t period; /*!< CAN消息周期时间 */
    uint32_t phase;  /*!< CAN消息启动(首次发送)偏移量 */
} CAN_TX_MESSAGE_TIMING_s;

/** CAN接收(RX)消息的时序信息 */
typedef struct {
    uint32_t period; /*!< 期望的CAN消息周期时间 */
} CAN_RX_MESSAGE_TIMING_s;

/** CAN消息中使用的发送回调函数的类型定义 */
typedef uint32_t (*CAN_TxCallbackFunction_f)(
    CAN_MESSAGE_PROPERTIES_s message,
    uint8_t *canData,
    uint8_t *pMuxId,
    const CAN_SHIM_s *const kpkCanShim);

/** CAN消息中使用的接收回调函数的类型定义 */
typedef uint32_t (*CAN_RxCallbackFunction_f)(
    CAN_MESSAGE_PROPERTIES_s message,
    const uint8_t *const kpkCanData,
    const CAN_SHIM_s *const kpkCanShim);

/** CAN发送(TX)消息结构体的类型定义 */
typedef struct {
    CAN_NODE_s *canNode;                       /*!< 传输该消息的CAN节点 */
    CAN_MESSAGE_PROPERTIES_s message;          /*!< CAN消息属性 */
    CAN_TX_MESSAGE_TIMING_s timing;            /*!< 周期和相位 */
    CAN_TxCallbackFunction_f callbackFunction; /*!< 消息发送后的CAN回调函数 */
    uint8_t *pMuxId;                           /*!< 对于复用信号：回调函数可将其用作复用变量的指针，未使用时为NULL_PTR */
} CAN_TX_MESSAGE_TYPE_s;

/* TODO: 接收消息的时间检查尚未实现！ */
/** CAN接收(RX)消息结构体的类型定义 */
typedef struct {
    CAN_NODE_s *canNode;                       /*!< 接收该消息的CAN节点 */
    CAN_MESSAGE_PROPERTIES_s message;          /*!< CAN消息属性 */
    CAN_RX_MESSAGE_TIMING_s timing;            /*!< 周期和相位 */
    CAN_RxCallbackFunction_f callbackFunction; /*!< 消息接收后的CAN回调函数 */
} CAN_RX_MESSAGE_TYPE_s;

/*========== 外部常量与变量声明 ==============================================*/
/** 用于存储和传递本地数据库表句柄的变量 */
extern const CAN_SHIM_s can_kShim;

/** CAN1和CAN2(隔离)的CAN节点配置 @{*/
extern const CAN_NODE_s can_node1;
extern const CAN_NODE_s can_node2Isolated;
/**@}*/

/** CAN接收和发送消息配置结构体 @{*/
extern const CAN_TX_MESSAGE_TYPE_s can_txMessages[];
extern const CAN_RX_MESSAGE_TYPE_s can_rxMessages[];
/**@}*/

/** 发送和接收CAN消息定义的数组长度 @{*/
extern const uint8_t can_txMessagesLength;
extern const uint8_t can_rxMessagesLength;
/**@}*/

/*========== 外部函数原型 ===================================================*/

/*========== 外部化的静态函数原型（单元测试） ================================*/
#ifdef UNITY_UNIT_TEST
#endif

#endif /* FOXBMS__CAN_CFG_H_ */
