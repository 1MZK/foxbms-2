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
 * @file    sof_trapezoid_cfg.c
 * @author  foxBMS Team
 * @date    2020-10-07 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup APPLICATION
 * @prefix  SOF
 *
 * @brief   SOF module configuration file
 * @details TODO
 */

/*========== Includes =======================================================*/
#include "sof_trapezoid_cfg.h"

#include <stdint.h>

/*========== Macros and Definitions =========================================*/

/*========== Static Constant and Variable Definitions =======================*/

/*========== Extern Constant and Variable Definitions =======================*/

/**
 * @brief SOF配置参数结构体常量定义
 * 该结构体包含了计算基于电压和温度的电流限制时所需的所有阈值、极限值和降额区间边界。
 * 使用 const 修饰，确保运行时这些关键安全参数不会被篡改。
 */
const SOF_CONFIG_s sof_recommendedCurrent = {
    /* ==================== 电流限制极值 ==================== */
    /* 允许的最大持续充电电流（单串），作为充电降额曲线的上限 */
    .maximumChargeCurrent_mA              = SOF_STRING_CURRENT_CONTINUOUS_CHARGE_mA,
    /* 允许的最大持续放电电流（单串），作为放电降额曲线的上限 */
    .maximumDischargeCurrent_mA           = SOF_STRING_CURRENT_CONTINUOUS_DISCHARGE_mA,
    /* 跛行回家电流：在极低温放电等极端恶劣条件下，允许的最小维持电流，
       保证车辆能以极低功率勉强行驶至安全区域，而不是直接抛锚 */
    .limpHomeCurrent_mA                   = SOF_STRING_CURRENT_LIMP_HOME_mA,

    /* ==================== 低温降额区间边界 ==================== */
    /* 低温放电截止温度：高于此温度时，放电电流不再受低温限制，允许达到最大值 */
    .cutoffLowTemperatureDischarge_ddegC  = SOF_TEMPERATURE_LOW_CUTOFF_DISCHARGE_ddegC,
    /* 低温放电极限温度：低于此温度时，放电电流降至跛行回家电流（极冷保护） */
    .limitLowTemperatureDischarge_ddegC   = SOF_TEMPERATURE_LOW_LIMIT_DISCHARGE_ddegC,
    /* 低温充电截止温度：高于此温度时，充电电流不再受低温限制，允许达到最大值 */
    .cutoffLowTemperatureCharge_ddegC     = SOF_TEMPERATURE_LOW_CUTOFF_CHARGE_ddegC,
    /* 低温充电极限温度：低于此温度时，禁止充电（电流设为0），防止低温析锂造成内短路 */
    .limitLowTemperatureCharge_ddegC      = SOF_TEMPERATURE_LOW_LIMIT_CHARGE_ddegC,

    /* ==================== 高温降额区间边界 ==================== */
    /* 高温放电截止温度：低于此温度时，放电电流不再受高温限制，允许达到最大值 */
    .cutoffHighTemperatureDischarge_ddegC = SOF_TEMPERATURE_HIGH_CUTOFF_DISCHARGE_ddegC,
    /* 高温放电极限温度：高于此温度时，禁止放电（电流设为0），防止热失控 */
    .limitHighTemperatureDischarge_ddegC  = SOF_TEMPERATURE_HIGH_LIMIT_DISCHARGE_ddegC,
    /* 高温充电截止温度：低于此温度时，充电电流不再受高温限制，允许达到最大值 */
    .cutoffHighTemperatureCharge_ddegC    = SOF_TEMPERATURE_HIGH_CUTOFF_CHARGE_ddegC,
    /* 高温充电极限温度：高于此温度时，禁止充电（电流设为0），防止热失控 */
    .limitHighTemperatureCharge_ddegC     = SOF_TEMPERATURE_HIGH_LIMIT_CHARGE_ddegC,

    /* ==================== 电压降额区间边界 ==================== */
    /* 充电电压上限极限：当最高单体电压达到此值时，充电电流降为0（满充保护） */
    .limitUpperCellVoltage_mV             = SOF_VOLTAGE_LIMIT_CHARGE_mV,
    /* 充电电压截止上限：当最高单体电压低于此值时，充电电流允许达到最大值（健康区间） */
    .cutoffUpperCellVoltage_mV            = SOF_VOLTAGE_CUTOFF_CHARGE_mV,
    /* 放电电压下限极限：当最低单体电压降至此值时，放电电流降为0（过放保护） */
    .limitLowerCellVoltage_mV             = SOF_VOLTAGE_LIMIT_DISCHARGE_mV,
    /* 放电电压截止下限：当最低单体电压高于此值时，放电电流允许达到最大值（健康区间） */
    .cutoffLowerCellVoltage_mV            = SOF_VOLTAGE_CUTOFF_DISCHARGE_mV
};

/*========== Static Function Prototypes =====================================*/

/*========== Static Function Implementations ================================*/

/*========== Extern Function Implementations ================================*/

/*========== Externalized Static Function Implementations (Unit Test) =======*/
#ifdef UNITY_UNIT_TEST
#endif
