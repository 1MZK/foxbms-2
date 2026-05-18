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

C
 复制
 插入
 新文件

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
 * @date    2019-08-27 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup MAIN
 * @prefix  TODO
 *
 * @brief   主函数
 * @details 主函数实现了硬件初始化，并启动操作系统（应用程序在此上下文中运行）。
 */

/*========== 包含文件 =======================================================*/
#include "main.h"

#include "HL_adc.h"
#include "HL_crc.h"
#include "HL_etpwm.h"
#include "HL_gio.h"
#include "HL_het.h"
#include "HL_pinmux.h"
#include "HL_sys_core.h"

#include "adc.h"
#include "diag.h"
#include "dma.h"
#if (defined(FOXBMS_TCP_SUPPORT) && (FOXBMS_TCP_SUPPORT == 1))
#include "ethernet.h"
#endif
#include "foxmath.h"
#include "fstd_types.h"
#include "i2c.h"
#include "led.h"
#include "master_info.h"
#include "os.h"
#include "pwm.h"
#include "spi.h"
#if (defined(FOXBMS_UART_SUPPORT) && (FOXBMS_UART_SUPPORT == 1))
#include "uart.h"
#endif
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
    MINFO_SetResetSource(getResetSource()); /*!< 获取复位源并清除相应的标志 */
    muxInit();                              /*!< 初始化引脚复用 */
    gioInit();                              /*!< 初始化通用输入输出(GPIO) */
    SPI_Initialize();                       /*!< 初始化SPI通信 */
    adcInit();                              /*!< 初始化模数转换器(ADC) */
    hetInit();                              /*!< 初始化高级高性能定时器(HET) */
    etpwmInit();                            /*!< 初始化增强型脉宽调制(eTPWM) */
    crcInit();                              /*!< 初始化CRC模块 */
    LED_SetDebugLed();                      /*!< 设置调试LED */
    I2C_Initialize();                       /*!< 初始化I2C通信 */
    DMA_Initialize();                       /*!< 初始化直接内存访问(DMA) */
#if (defined(FOXBMS_UART_SUPPORT) && (FOXBMS_UART_SUPPORT == 1))
    UART_Initialize();                      /*!< 如果支持UART，则初始化UART通信 */
#endif
    PWM_Initialize();                       /*!< 初始化脉宽调制(PWM) */
    DIAG_Initialize(&diag_device);          /*!< 初始化诊断模块 */
    MATH_StartupSelfTest();                 /*!< 执行数学库启动自检 */
#if (defined(FOXBMS_TCP_SUPPORT) && (FOXBMS_TCP_SUPPORT == 1))
    ETH_Initialize();                       /*!< 如果支持TCP/IP，则初始化以太网 */
#endif
    const STD_RETURN_TYPE_e checkTimeHasPassedSelfTestReturnValue = OS_CheckTimeHasPassedSelfTest(); /*!< 检查自检是否在规定时间内完成 */
    FAS_ASSERT(checkTimeHasPassedSelfTestReturnValue == STD_OK); /*!< 断言：自检必须在规定时间内完成 */

    OS_InitializeOperatingSystem(); /*!< 初始化操作系统 */

    /* 在创建AFE任务之后使能IRQ中断，以防止DMA中断提前发生。
       因为DMA中断调用的函数需要一个有效的AFE任务句柄，
       而在创建AFE任务之前该句柄为NULL。 */
    _enable_IRQ_interrupt_();

    if (OS_INIT_PRE_OS != os_boot) {
        /* 无法创建队列、互斥量、事件和任务，此时系统无法继续启动，在此处拦截 */
        FAS_ASSERT(FAS_TRAP);
    }

    os_schedulerStartTime = OS_GetTickCount(); /*!< 获取并设置调度器启动时间戳 */

    OS_StartScheduler(); /*!< 启动操作系统调度器 */
    /* 程序绝不应该运行到这里；无法确定该程序的退出状态，
       但为了代码的正确性，此处返回错误代码 */
    return 1;
}

/*========== 外部化的静态函数实现（单元测试） ================================*/
#ifdef UNITY_UNIT_TEST
#endif
