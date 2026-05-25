/**
 *
 * @copyright &copy; 2010 - 2026, Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 * 版权所有。
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * 在满足以下条件的前提下，允许以源代码和二进制形式进行重新分发和使用，无论是否经过修改：
 *
 * 1. 源代码的重新分发必须保留上述版权声明、此条件列表以及下述免责声明。
 *
 * 2. 二进制形式的重新分发必须在随分发提供的文档和/或其他材料中复制上述版权声明、
 *    此条件列表以及下述免责声明。
 *
 * 3. 未经特定事先书面许可，版权持有者的名称及其贡献者的名称不得用于支持或推广
 *    由本软件派生的产品。
 *
 * 本软件由版权持有者和贡献者“按原样”提供，不提供任何明示或暗示的保证，包括但不仅限于
 * 对适销性和特定用途适用性的暗示保证。在任何情况下，版权持有者或贡献者对任何直接、间接、
 * 偶然、特殊、惩罚性或后果性损害（包括但不仅限于替代商品或服务的采购、使用、数据或
 * 利润的损失或业务中断）不承担责任，无论其基于何种责任理论，无论是合同责任、严格责任
 * 或侵权（包括疏忽或其他），即使已被告知可能发生此类损害，也是如此。
 *
 * 我们恳请您在您的硬件、软件、文档或广告材料中使用以下一个或多个短语来指代 foxBMS：
 *
 * - "本产品使用了 foxBMS&reg; 的部分内容"
 * - "本产品包含了 foxBMS&reg; 的部分内容"
 * - "本产品派生自 foxBMS&reg;"
 *
 */

/**
 * @file    soa.c
 * @author  foxBMS 团队
 * @date    2020-10-14 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup APPLICATION
 * @prefix  SOA
 *
 * @brief   SOA 模块负责根据安全限制检查电池参数
 * @details TODO
 */

/*========== 包含文件 =======================================================*/
#include "soa.h"

#include "battery_cell_cfg.h"
#include "battery_system_cfg.h"

#include "bms.h"
#include "diag.h"
#include "foxmath.h"

#include <stdbool.h>
#include <stdint.h>

/*========== 宏和定义 =========================================================*/

/*========== 静态常量和变量定义 =======================*/

/*========== 外部常量和变量定义 =======================*/

/*========== 静态函数原型 =====================================*/

/*========== 静态函数实现 ================================*/

/*========== 外部函数实现 ================================*/

/**
 * @brief   检查电池电压是否违反安全运行区域（SOA）限制
 * @details 此函数遍历电池系统中的每一个串（string），提取该串中电芯的最大电压和最小电压，
 *          并将这些电压值与预定义的多级安全限制进行比较。
 *          安全限制通常分为三级：
 *          - MOL (Maximum Operating Limit): 最大运行限制，超出此限制记录警告
 *          - RSL (Recommended Safety Limit): 推荐安全限制，超出此限制记录严重警告
 *          - MSL (Maximum Safety Limit): 最大安全限制，超出此限制通常会导致系统进入致命错误状态并断开接触器
 *
 *          对于欠压情况，如果电压不仅低于 MSL，甚至低于深度放电阈值，
 *          还会触发深度放电诊断标志。
 *
 * @param[in] pMinimumMaximumCellVoltages  指向包含电芯最小/最大电压数据的数据库结构体指针。
 *                                        该结构体中包含了各串的最高单体电压和最低单体电压，
 *                                        单位通常为毫伏。
 *
 * @note    该函数通过调用 DIAG_Handler() 来上报诊断事件的状态（OK 或 NOT_OK）。
 *          它本身不返回值，诊断结果会记录在系统的诊断数据库中。
 */
extern void SOA_CheckVoltages(DATA_BLOCK_MIN_MAX_s *pMinimumMaximumCellVoltages) {
    FAS_ASSERT(pMinimumMaximumCellVoltages != NULL_PTR);    // 断言传入的指针不为空
    DIAG_RETURNTYPE_e retvalUndervoltageMSL = DIAG_HANDLER_RETURN_ERR_OCCURRED; // 诊断结果，用于记录欠压情况下的 MSL 违规状态

    /* 遍历每个串并检查电压 */
    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
        int16_t voltageMax_mV = pMinimumMaximumCellVoltages->maximumCellVoltage_mV[s]; // 获取串的最高单体电压
        int16_t voltageMin_mV = pMinimumMaximumCellVoltages->minimumCellVoltage_mV[s]; // 获取串的最低单体电压

        if (voltageMax_mV >= BC_VOLTAGE_MAX_MOL_mV) {
            /* 超过最大运行限制过压 */
            DIAG_Handler(DIAG_ID_CELL_VOLTAGE_OVERVOLTAGE_MOL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
            if (voltageMax_mV >= BC_VOLTAGE_MAX_RSL_mV) {
                /* 超过推荐安全限制过压 */
                DIAG_Handler(DIAG_ID_CELL_VOLTAGE_OVERVOLTAGE_RSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                if (voltageMax_mV >= BC_VOLTAGE_MAX_MSL_mV) {
                    /* 超过最大安全限制过压 */
                    DIAG_Handler(DIAG_ID_CELL_VOLTAGE_OVERVOLTAGE_MSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                }
            }
        }
        if (voltageMax_mV < BC_VOLTAGE_MAX_MSL_mV) {
            /* 未超过最大安全限制过压 */
            DIAG_Handler(DIAG_ID_CELL_VOLTAGE_OVERVOLTAGE_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
            if (voltageMax_mV < BC_VOLTAGE_MAX_RSL_mV) {
                /* 未超过推荐安全限制过压 */
                DIAG_Handler(DIAG_ID_CELL_VOLTAGE_OVERVOLTAGE_RSL, DIAG_EVENT_OK, DIAG_STRING, s);
                if (voltageMax_mV < BC_VOLTAGE_MAX_MOL_mV) {
                    /* 未超过最大运行限制过压 */
                    DIAG_Handler(DIAG_ID_CELL_VOLTAGE_OVERVOLTAGE_MOL, DIAG_EVENT_OK, DIAG_STRING, s);
                }
            }
        }

        if (voltageMin_mV <= BC_VOLTAGE_MIN_MOL_mV) {
            /* 超过最大运行限制欠压 */
            DIAG_Handler(DIAG_ID_CELL_VOLTAGE_UNDERVOLTAGE_MOL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
            if (voltageMin_mV <= BC_VOLTAGE_MIN_RSL_mV) {
                /* 超过推荐安全限制欠压 */
                DIAG_Handler(DIAG_ID_CELL_VOLTAGE_UNDERVOLTAGE_RSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                if (voltageMin_mV <= BC_VOLTAGE_MIN_MSL_mV) {
                    /* 超过最大安全限制欠压 */
                    retvalUndervoltageMSL =
                        DIAG_Handler(DIAG_ID_CELL_VOLTAGE_UNDERVOLTAGE_MSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);

                    /* 如果设置了欠压标志并且违反了深度放电电压 */
                    if ((retvalUndervoltageMSL == DIAG_HANDLER_RETURN_ERR_OCCURRED) &&
                        (voltageMin_mV <= BC_VOLTAGE_DEEP_DISCHARGE_mV)) {
                        DIAG_Handler(DIAG_ID_DEEP_DISCHARGE_DETECTED, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                    }
                }
            }
        }
        if (voltageMin_mV > BC_VOLTAGE_MIN_MSL_mV) {
            /* 未超过最大安全限制欠压 */
            DIAG_Handler(DIAG_ID_CELL_VOLTAGE_UNDERVOLTAGE_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
            if (voltageMin_mV > BC_VOLTAGE_MIN_RSL_mV) {
                /* 未超过推荐安全限制欠压 */
                DIAG_Handler(DIAG_ID_CELL_VOLTAGE_UNDERVOLTAGE_RSL, DIAG_EVENT_OK, DIAG_STRING, s);
                if (voltageMin_mV > BC_VOLTAGE_MIN_MOL_mV) {
                    /* 未超过最大运行限制欠压 */
                    DIAG_Handler(DIAG_ID_CELL_VOLTAGE_UNDERVOLTAGE_MOL, DIAG_EVENT_OK, DIAG_STRING, s);
                }
            }
        }
    }
}

/**
 * @brief   检查电池温度是否违反安全运行区域（SOA）限制
 * @details 此函数用于监测电池系统中的温度参数，确保其在安全的运行范围内。
 *          它会遍历电池系统中的各个串，提取电芯的最高温度和最低温度，
 *          并结合当前的电流流向（充电或放电），将这些温度值与预定义的
 *          多级安全限制进行比较。
 *
 *          温度安全限制通常分为三级，且充电和放电的阈值通常不同（放电允许的
 *          温度上限通常高于充电）：
 *          - MOL (Maximum Operating Limit): 最大运行限制，超出记录警告
 *          - RSL (Recommended Safety Limit): 推荐安全限制，超出记录严重警告
 *          - MSL (Maximum Safety Limit): 最大安全限制，超出通常导致断开接触器
 *
 *          该函数会分别检查过温和欠温两种异常情况。
 *
 * @param[in] pMinimumMaximumCellTemperatures  指向包含电芯最小/最大温度数据的
 *                                            数据库结构体指针（DATA_BLOCK_MIN_MAX_s）。
 *                                            包含各串的最高单体温度和最低单体温度，
 *                                            单位通常为 0.1°C (ddegC)。
 * @param[in] pCurrent                        指向包含电池包当前测量值（如电流）的
 *                                            数据库结构体指针（DATA_BLOCK_PACK_VALUES_s）。
 *                                            此参数是必需的，因为温度的安全阈值
 *                                            取决于电池当前是处于充电状态还是放电状态
 *                                            （通过 BMS_GetCurrentFlowDirection() 获取）。
 *
 * @note    该函数通过调用 DIAG_Handler() 来上报诊断事件的状态（OK 或 NOT_OK），
 *          它本身不返回值，诊断结果会记录在系统的诊断数据库中。
 */
extern void SOA_CheckTemperatures(
    DATA_BLOCK_MIN_MAX_s *pMinimumMaximumCellTemperatures,  // 电芯最小/最大温度数据
    // 电池包当前测量值（如电流），用于确定充放电状态
    DATA_BLOCK_PACK_VALUES_s *pCurrent) {
    FAS_ASSERT(pMinimumMaximumCellTemperatures != NULL_PTR);    //检查电芯温度数据指针不为空
    FAS_ASSERT(pCurrent != NULL_PTR);            //检查电池包当前测量值指针不为空
    /* 遍历每个串并检查温度 */
    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
        int32_t i_current            = pCurrent->stringCurrent_mA[s];  //从电流数据结构体中读取当前串的电流值，单位为毫安（mA）
        int16_t temperatureMin_ddegC = pMinimumMaximumCellTemperatures->minimumTemperature_ddegC[s]; //获取串的最低单体温度，单位为0.1°C（ddegC）
        int16_t temperatureMax_ddegC = pMinimumMaximumCellTemperatures->maximumTemperature_ddegC[s]; //获取串的最高单体温度，单位为0.1°C（ddegC）

        /* 过温检查 */
        if (BMS_GetCurrentFlowDirection(i_current) == BMS_DISCHARGING) {
            /* 放电 */
            if (temperatureMax_ddegC >= BC_TEMPERATURE_MAX_DISCHARGE_MOL_ddegC) {
                /* 超过最大运行限制过温 */
                DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_DISCHARGE_MOL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                if (temperatureMax_ddegC >= BC_TEMPERATURE_MAX_DISCHARGE_RSL_ddegC) {
                    /* 超过推荐安全限制过温 */
                    DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_DISCHARGE_RSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                    if (temperatureMax_ddegC >= BC_TEMPERATURE_MAX_DISCHARGE_MSL_ddegC) {
                        /* 超过最大安全限制过温 */
                        DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_DISCHARGE_MSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                    }
                }
            }
            if (temperatureMax_ddegC < BC_TEMPERATURE_MAX_DISCHARGE_MSL_ddegC) {
                /* 未超过最大安全限制过温 */
                DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_DISCHARGE_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                if (temperatureMax_ddegC < BC_TEMPERATURE_MAX_DISCHARGE_RSL_ddegC) {
                    /* 未超过推荐安全限制过温 */
                    DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_DISCHARGE_RSL, DIAG_EVENT_OK, DIAG_STRING, s);
                    if (temperatureMax_ddegC < BC_TEMPERATURE_MAX_DISCHARGE_MOL_ddegC) {
                        /* 未超过最大运行限制过温 */
                        DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_DISCHARGE_MOL, DIAG_EVENT_OK, DIAG_STRING, s);
                    }
                }
            }
        } else {
            /* 充电 */
            if (temperatureMax_ddegC >= BC_TEMPERATURE_MAX_CHARGE_MOL_ddegC) {
                /* 超过最大运行限制过温 */
                DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_CHARGE_MOL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                if (temperatureMax_ddegC >= BC_TEMPERATURE_MAX_CHARGE_RSL_ddegC) {
                    /* 超过推荐安全限制过温 */
                    DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_CHARGE_RSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                    /* 超过最大安全限制过温 */
                    if (temperatureMax_ddegC >= BC_TEMPERATURE_MAX_CHARGE_MSL_ddegC) {
                        DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_CHARGE_MSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                    }
                }
            }
            if (temperatureMax_ddegC < BC_TEMPERATURE_MAX_CHARGE_MSL_ddegC) {
                /* 未超过最大安全限制过温 */
                DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_CHARGE_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                if (temperatureMax_ddegC < BC_TEMPERATURE_MAX_CHARGE_RSL_ddegC) {
                    /* 未超过推荐安全限制过温 */
                    DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_CHARGE_RSL, DIAG_EVENT_OK, DIAG_STRING, s);
                    if (temperatureMax_ddegC < BC_TEMPERATURE_MAX_CHARGE_MOL_ddegC) {
                        /* 未超过最大运行限制过温 */
                        DIAG_Handler(DIAG_ID_TEMP_OVERTEMPERATURE_CHARGE_MOL, DIAG_EVENT_OK, DIAG_STRING, s);
                    }
                }
            }
        }

        /* 欠温检查 */
        if (BMS_GetCurrentFlowDirection(i_current) == BMS_DISCHARGING) {
            /* 放电 */
            if (temperatureMin_ddegC <= BC_TEMPERATURE_MIN_DISCHARGE_MOL_ddegC) {
                /* 超过最大运行限制欠温 */
                DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_DISCHARGE_MOL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                if (temperatureMin_ddegC <= BC_TEMPERATURE_MIN_DISCHARGE_RSL_ddegC) {
                    /* 超过推荐安全限制欠温 */
                    DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_DISCHARGE_RSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                    if (temperatureMin_ddegC <= BC_TEMPERATURE_MIN_DISCHARGE_MSL_ddegC) {
                        /* 超过最大安全限制欠温 */
                        DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_DISCHARGE_MSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                    }
                }
            }
            if (temperatureMin_ddegC > BC_TEMPERATURE_MIN_DISCHARGE_MSL_ddegC) {
                /* 未超过最大安全限制欠温 */
                DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_DISCHARGE_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                if (temperatureMin_ddegC > BC_TEMPERATURE_MIN_DISCHARGE_RSL_ddegC) {
                    /* 未超过推荐安全限制欠温 */
                    DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_DISCHARGE_RSL, DIAG_EVENT_OK, DIAG_STRING, s);
                    if (temperatureMin_ddegC > BC_TEMPERATURE_MIN_DISCHARGE_MOL_ddegC) {
                        /* 未超过最大运行限制欠温 */
                        DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_DISCHARGE_MOL, DIAG_EVENT_OK, DIAG_STRING, s);
                    }
                }
            }
        } else {
            /* 充电 */
            if (temperatureMin_ddegC <= BC_TEMPERATURE_MIN_CHARGE_MOL_ddegC) {
                /* 超过最大运行限制欠温 */
                DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_CHARGE_MOL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                if (temperatureMin_ddegC <= BC_TEMPERATURE_MIN_CHARGE_RSL_ddegC) {
                    /* 超过推荐安全限制欠温 */
                    DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_CHARGE_RSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                    if (temperatureMin_ddegC <= BC_TEMPERATURE_MIN_CHARGE_MSL_ddegC) {
                        /* 超过最大安全限制欠温 */
                        DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_CHARGE_MSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                    }
                }
            }
            if (temperatureMin_ddegC > BC_TEMPERATURE_MIN_CHARGE_MSL_ddegC) {
                /* 未超过最大安全限制欠温 */
                DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_CHARGE_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                if (temperatureMin_ddegC > BC_TEMPERATURE_MIN_CHARGE_RSL_ddegC) {
                    /* 未超过推荐安全限制欠温 */
                    DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_CHARGE_RSL, DIAG_EVENT_OK, DIAG_STRING, s);
                    if (temperatureMin_ddegC > BC_TEMPERATURE_MIN_CHARGE_MOL_ddegC) {
                        /* 未超过最大运行限制欠温 */
                        DIAG_Handler(DIAG_ID_TEMP_UNDERTEMPERATURE_CHARGE_MOL, DIAG_EVENT_OK, DIAG_STRING, s);
                    }
                }
            }
        }
    }
}

/**
 * @brief   检查电池温度是否违反安全运行区域（SOA）限制
 * @details 此函数用于监测电池系统中的温度参数，确保其在安全的运行范围内。
 *          它会遍历电池系统中的各个串，提取电芯的最高温度和最低温度，
 *          并结合当前的电流流向（充电或放电），将这些温度值与预定义的
 *          多级安全限制进行比较。
 *
 *          温度安全限制通常分为三级，且充电和放电的阈值通常不同（放电允许的
 *          温度上限通常高于充电）：
 *          - MOL (Maximum Operating Limit): 最大运行限制，超出记录警告
 *          - RSL (Recommended Safety Limit): 推荐安全限制，超出记录严重警告
 *          - MSL (Maximum Safety Limit): 最大安全限制，超出通常导致断开接触器
 *
 *          该函数会分别检查过温和欠温两种异常情况。
 *
 * @param[in] pMinimumMaximumCellTemperatures  指向包含电芯最小/最大温度数据的
 *                                            数据库结构体指针（DATA_BLOCK_MIN_MAX_s）。
 *                                            包含各串的最高单体温度和最低单体温度，
 *                                            单位通常为 0.1°C (ddegC)。
 * @param[in] pCurrent                        指向包含电池包当前测量值（如电流）的
 *                                            数据库结构体指针（DATA_BLOCK_PACK_VALUES_s）。
 *                                            此参数是必需的，因为温度的安全阈值
 *                                            取决于电池当前是处于充电状态还是放电状态
 *                                            （通过 BMS_GetCurrentFlowDirection() 获取）。
 *
 * @note    该函数通过调用 DIAG_Handler() 来上报诊断事件的状态（OK 或 NOT_OK），
 *          它本身不返回值，诊断结果会记录在系统的诊断数据库中。
 */
extern void SOA_CheckCurrent(DATA_BLOCK_PACK_VALUES_s *pTablePackValues) {
    FAS_ASSERT(pTablePackValues != NULL_PTR); // 检查传入的电池数据包指针是否为空指针

    /* 遍历每个串并检查电流 */
    for (uint8_t s = 0u; s < BS_NR_OF_STRINGS; s++) {
        /* 仅当电流值有效时执行检查 */
        if (pTablePackValues->invalidStringCurrent[s] == 0u) {
            BMS_CURRENT_FLOW_STATE_e currentDirection =
                BMS_GetCurrentFlowDirection(pTablePackValues->stringCurrent_mA[s]); // 获取电流方向
            uint32_t absStringCurrent_mA = (uint32_t)abs(pTablePackValues->stringCurrent_mA[s]);
            /* 根据电流方向检查各种电流限制 */
            bool stringOvercurrent = SOA_IsStringCurrentLimitViolated(absStringCurrent_mA, currentDirection);
            bool cellOvercurrent   = SOA_IsCellCurrentLimitViolated(absStringCurrent_mA, currentDirection);
            if (currentDirection == BMS_CHARGING) {
                /* 检查串电流限制 */
                if (stringOvercurrent == true) {
                    DIAG_Handler(DIAG_ID_STRING_OVERCURRENT_CHARGE_MSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                } else {
                    DIAG_Handler(DIAG_ID_STRING_OVERCURRENT_CHARGE_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                }
                /* 检查电池电芯限制 */
                if (cellOvercurrent == true) {
                    DIAG_Handler(DIAG_ID_OVERCURRENT_CHARGE_CELL_MSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                } else {
                    DIAG_Handler(DIAG_ID_OVERCURRENT_CHARGE_CELL_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                }
            } else if (currentDirection == BMS_DISCHARGING) {
                /* 检查串电流限制 */
                if (stringOvercurrent == true) {
                    DIAG_Handler(DIAG_ID_STRING_OVERCURRENT_DISCHARGE_MSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                } else {
                    DIAG_Handler(DIAG_ID_STRING_OVERCURRENT_DISCHARGE_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                }
                /* 检查电池电芯限制 */
                if (cellOvercurrent == true) {
                    DIAG_Handler(DIAG_ID_OVERCURRENT_DISCHARGE_CELL_MSL, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
                } else {
                    DIAG_Handler(DIAG_ID_OVERCURRENT_DISCHARGE_CELL_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                }
            } else {
                /* 无电流流过 -> 一切正常 */
                DIAG_Handler(DIAG_ID_STRING_OVERCURRENT_CHARGE_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                DIAG_Handler(DIAG_ID_OVERCURRENT_CHARGE_CELL_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                DIAG_Handler(DIAG_ID_STRING_OVERCURRENT_DISCHARGE_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
                DIAG_Handler(DIAG_ID_OVERCURRENT_DISCHARGE_CELL_MSL, DIAG_EVENT_OK, DIAG_STRING, s);
            }

            /* 检查接触器断开时是否有电流流过 */
            if (SOA_IsCurrentOnOpenString(currentDirection, s) == false) {
                DIAG_Handler(DIAG_ID_CURRENT_ON_OPEN_STRING, DIAG_EVENT_OK, DIAG_STRING, s);
            } else {
                DIAG_Handler(DIAG_ID_CURRENT_ON_OPEN_STRING, DIAG_EVENT_NOT_OK, DIAG_STRING, s);
            }
        }
    }

    /* 检查电池包电流 */
    if (pTablePackValues->invalidPackCurrent == 0u) {
        BMS_CURRENT_FLOW_STATE_e currentDirection = BMS_GetCurrentFlowDirection(pTablePackValues->packCurrent_mA);
        uint32_t absPackCurrent_mA                = (uint32_t)abs(pTablePackValues->packCurrent_mA);
        bool packOvercurrent                      = SOA_IsPackCurrentLimitViolated(absPackCurrent_mA, currentDirection);

        if (currentDirection == BMS_CHARGING) {
            if (packOvercurrent == true) {
                DIAG_Handler(DIAG_ID_PACK_OVERCURRENT_CHARGE_MSL, DIAG_EVENT_NOT_OK, DIAG_SYSTEM, 0u);
            } else {
                DIAG_Handler(DIAG_ID_PACK_OVERCURRENT_CHARGE_MSL, DIAG_EVENT_OK, DIAG_SYSTEM, 0u);
            }
        } else if (currentDirection == BMS_DISCHARGING) {
            if (packOvercurrent == true) {
                DIAG_Handler(DIAG_ID_PACK_OVERCURRENT_DISCHARGE_MSL, DIAG_EVENT_NOT_OK, DIAG_SYSTEM, 0u);
            } else {
                DIAG_Handler(DIAG_ID_PACK_OVERCURRENT_DISCHARGE_MSL, DIAG_EVENT_OK, DIAG_SYSTEM, 0u);
            }
        } else {
            /* 无电流流过 -> 一切正常 */
            DIAG_Handler(DIAG_ID_PACK_OVERCURRENT_CHARGE_MSL, DIAG_EVENT_OK, DIAG_SYSTEM, 0u);
            DIAG_Handler(DIAG_ID_PACK_OVERCURRENT_DISCHARGE_MSL, DIAG_EVENT_OK, DIAG_SYSTEM, 0u);
        }
    }
}

extern void SOA_CheckSlaveTemperatures(void) { /* TODO: 待实现 */
}

/*========== 外部化的静态函数实现 (单元测试) =======*/
#ifdef UNITY_UNIT_TEST
#endif

