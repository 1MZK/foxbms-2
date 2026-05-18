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
 * @file    bal_cfg.h
 * @author  foxBMS Team
 * @date    2020-02-24 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup DRIVERS_CONFIGURATION
 * @prefix  BAL
 *
 * @brief   均衡驱动配置的头文件
 * @details 本文件包含了电池均衡驱动相关的宏定义、参数配置及函数声明
 */

#ifndef FOXBMS__BAL_CFG_H_
#define FOXBMS__BAL_CFG_H_

/*========== 包含文件 =======================================================*/

#include <stdint.h>

/*========== 宏与定义 =======================================================*/

/** 均衡状态机短时间定义，单位：100*ms */
#define BAL_FSM_SHORTTIME_100ms (1u)

/** 均衡状态机长时间定义，单位：100*ms */
#define BAL_FSM_LONGTIME_100ms (50u)

/** 均衡状态机均衡执行时间，单位：100*ms */
#define BAL_FSM_BALANCING_TIME_100ms (10u)

/** 均衡电压阈值的默认值，单位：mV */
#define BAL_DEFAULT_THRESHOLD_mV (200)

/** 均衡电压阈值允许设置的最大值，单位：mV */
#define BAL_MAXIMUM_THRESHOLD_mV (5000)

/** 均衡电压阈值允许设置的最小值，单位：mV */
#define BAL_MINIMUM_THRESHOLD_mV (0)

/** 均衡结束时的电压阈值滞后量（回差），单位：mV */
#define BAL_HYSTERESIS_mV (200)

/** 允许开启均衡的电压下限，单位：mV */
#define BAL_LOWER_VOLTAGE_LIMIT_mV (2000)

/** 允许开启均衡的温度上限，单位：0.1°C (deci °C) */
#define BAL_UPPER_TEMPERATURE_LIMIT_ddegC (700)

/*========== 外部常量与变量声明 ==============================================*/

/*========== 外部函数原型 ===================================================*/
/**
 * @brief   设置均衡电压阈值
 * @param   threshold_mV 均衡阈值，单位：mV
 */
extern void BAL_SetBalancingThreshold(int32_t threshold_mV);

/**
 * @brief   获取均衡电压阈值
 * @return  当前均衡阈值，单位：mV
 */
extern int32_t BAL_GetBalancingThreshold_mV(void);

/*========== 外部化的静态函数原型（单元测试） ================================*/
#ifdef UNITY_UNIT_TEST
#endif

#endif /* FOXBMS__BAL_CFG_H_ */
