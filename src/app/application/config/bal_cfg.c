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
 * @file    bal_cfg.c
 * @author  foxBMS Team
 * @date    2022-02-26 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup DRIVERS_CONFIGURATION
 * @prefix  BAL
 *
 * @brief   Implementation for the configuration for the driver for balancing
 * @details TODO
 */

/*========== Includes =======================================================*/
#include "bal_cfg.h"

#include "os.h"

#include <stdint.h>

/*========== 宏与定义 =======================================================*/

/*========== 静态常量与变量定义 ==============================================*/
/** 均衡电压阈值，单位：mV */
static int32_t bal_threshold_mV = BAL_DEFAULT_THRESHOLD_mV;

/*========== 外部常量与变量定义 ==============================================*/

/*========== 静态函数原型 ===================================================*/

/*========== 静态函数实现 ====================================================*/

/*========== 外部函数实现 ====================================================*/
/**
 * @brief 设置电池均衡电压阈值，并进行上下限边界保护
 * @param threshold_mV 要设置的均衡阈值，单位：mV
 */
extern void BAL_SetBalancingThreshold(int32_t threshold_mV) {
    int32_t boundedThreshold_mV = threshold_mV;
    /* 限制阈值不超过最大允许值 */
    if (boundedThreshold_mV > BAL_MAXIMUM_THRESHOLD_mV) {
        boundedThreshold_mV = BAL_MAXIMUM_THRESHOLD_mV;
    }
    /* 限制阈值不低于最小允许值 */
    if (boundedThreshold_mV < BAL_MINIMUM_THRESHOLD_mV) {
        boundedThreshold_mV = BAL_MINIMUM_THRESHOLD_mV;
    }
    /* 进入临界区保护，防止读写冲突 */
    OS_EnterTaskCritical();
    bal_threshold_mV = boundedThreshold_mV;
    /* 退出临界区 */
    OS_ExitTaskCritical();
}

/**
 * @brief 获取当前的均衡电压阈值
 * @return 当前的均衡阈值，单位：mV
 */
extern int32_t BAL_GetBalancingThreshold_mV(void) {
    return bal_threshold_mV;
}

/*========== 外部化的静态函数实现（单元测试） ================================*/
#ifdef UNITY_UNIT_TEST
#endif
