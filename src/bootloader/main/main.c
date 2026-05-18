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
 * @file    main.c
 * @author  foxBMS Team
 * @date    2024-08-27 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup MAIN
 * @prefix  MAIN
 *
 * @brief   主函数
 * @details 主函数实现了一个状态机，用于完成引导加载程序(Bootloader)的工作流程。
 */

/*========== 包含文件 =======================================================*/
#include "main.h"

#include "boot_cfg.h"

#include "HL_gio.h"
#include "HL_pinmux.h"
#include "HL_rti.h"
#include "HL_system.h"

#include "boot.h"
#include "can.h"
#include "fstd_types.h"
#include "fstring.h"
#include "infinite-loop-helper.h"
#include "rti.h"

#include <stdint.h>

/*========== 宏与定义 =======================================================*/

/*========== 静态常量与变量定义 ==============================================*/

/*========== 外部常量与变量定义 ==============================================*/

/*========== 静态函数原型 ===================================================*/

/*========== 静态函数实现 ====================================================*/

/*========== 外部函数实现 ====================================================*/
#ifndef UNITY_UNIT_TEST
int main(void)
#else
int unit_test_main(void) /*!< 单元测试时的主函数入口 */
#endif
{
    /* 在CPSR寄存器中使能IRQ和FIQ中断模式 */
    _enable_interrupt_();

    /* 初始化部分模块 */
    muxInit();              /*!< 初始化引脚复用 */
    gioInit();              /*!< 初始化通用输入输出(GPIO) */
    CAN_Initialize();       /*!< 初始化CAN通信 */
    rtiInit();              /*!< 初始化实时中断定时器(RTI) */

    /* 将Flash相关的 .text 和 .const 段拷贝到RAM中运行 */
    memcpy(&main_textRunStartFlashC, &main_textLoadStartFlashC, (uint32_t)&main_textSizeFlashC);
    memcpy(&main_constRunStartFlashCfgC, &main_constLoadStartFlashCfgC, (uint32_t)&main_constSizeFlashCfgC);

    CAN_SendBootMessage(); /*!< 通过CAN发送引导启动消息 */

    /* 启动rti定时器 */
    RTI_ResetFreeRunningCount();                          /*!< 重置自由运行计数器 */
    rtiStartCounter(rtiREG1, rtiCOUNTER_BLOCK0);         /*!< 启动RTI计数器块0 */
    uint32_t startCounter   = RTI_GetFreeRunningCount();  /*!< 获取起始计数值 */
    bool finishedOneAutoRun = false;                      /*!< 标记是否已完成一次自动运行(超时跳转) */

    while (FOREVER()) {
        /* 使能IRQ中断以确保可以处理接收到的CAN消息，
           因为在以下上下文中调用的子函数可能会潜在地禁用IRQ。 */
        _enable_IRQ_interrupt_();

        /* 检查是否发生超时 */
        if ((finishedOneAutoRun == false) && (RTI_IsTimeElapsed(startCounter, MAIN_TIME_OUT_us) == true)) {
            if (boot_state == BOOT_FSM_STATE_WAIT) {
                /* 如果超时且当前处于等待状态，则切换到运行状态以跳转至应用程序 */
                boot_state = BOOT_FSM_STATE_RUN;
                rtiStopCounter(rtiREG1, rtiCOUNTER_BLOCK0); /*!< 停止RTI计数器 */
                RTI_ResetFreeRunningCount();                /*!< 重置自由运行计数器 */
                finishedOneAutoRun = true;                  /*!< 标记自动运行已完成，防止再次触发 */
            }
        }

        switch (boot_state) {
            case BOOT_FSM_STATE_WAIT:
                /* 根据CAN有限状态机状态获取启动类型 */
                boot_state = BOOT_GetBootState();
                break;

            case BOOT_FSM_STATE_LOAD:
                /* 在加载期间获取启动类型，只能是 ERROR、RESET 或 LOAD 状态 */
                boot_state = BOOT_GetBootStateDuringLoad();
                break;

            case BOOT_FSM_STATE_RUN:
                /* 从Flash加载程序信息到变量，检查是否已有程序加载到Flash，
                   并通过比较保存的CRC签名和当前计算的CRC签名来检查其有效性 */
                if (BOOT_IsProgramAvailableAndValidated()) {
                    if (BOOT_JumpInToLastFlashedProgram() == STD_OK) {
                        /* 在真实硬件上永远无法执行到此行，此处仅用于测试 */
                        boot_state = BOOT_FSM_STATE_WAIT;
                    } else {
                        /* 跳转失败，进入错误状态 */
                        boot_state = BOOT_FSM_STATE_ERROR;
                    }
                } else {
                    /* 程序不可用或校验失败，回到等待状态 */
                    boot_state = BOOT_FSM_STATE_WAIT;
                }
                break;

            case BOOT_FSM_STATE_RESET:
                /* 将引导加载程序复位到初始状态 */
                if (BOOT_ResetBootloader() == STD_NOT_OK) {
                    /* 复位失败，进入错误状态 */
                    boot_state = BOOT_FSM_STATE_ERROR;
                } else {
                    /* 在真实硬件上永远无法执行到此行，此处仅用于测试 */
                    boot_state = BOOT_FSM_STATE_WAIT;
                }
                break;

            case BOOT_FSM_STATE_ERROR:
                /* 等待CAN请求以处理错误（目前仅支持复位处理） */
                boot_state = BOOT_GetBootStateDuringError();
                break;

            default:
                /* 如果 boot_state 是除已注册状态之外的任何状态，
                   则将其分配为错误FSM状态 */
                boot_state = BOOT_FSM_STATE_ERROR;
                break;
        }
    }
#pragma diag_push
#pragma diag_suppress 112
    /* AXIVION Next Codeline Style MisraC2012-2.1: 在目标硬件上运行时，我们永远不应到达这里 */
    return 1;
#pragma diag_pop
}

/*========== 外部化的静态函数实现（单元测试） ================================*/
#ifdef UNITY_UNIT_TEST
#endif
