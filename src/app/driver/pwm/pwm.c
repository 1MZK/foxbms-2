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
 * @file    pwm.c
 * @author  foxBMS Team
 * @date    2021-10-07 (date of creation)
 * @updated 2026-04-20 (date of last update)
 * @version v1.11.0
 * @ingroup DRIVERS
 * @prefix  PWM
 *
 * @brief   PWM模块的实现。
 * @details 待办事项
 */

/*========== 包含文件 =======================================================*/
#include "pwm.h"            // 包含PWM模块自身的头文件

#include "HL_ecap.h"        // 包含增强型捕获模块(ECAP)的硬件抽象层头文件
#include "HL_etpwm.h"       // 包含增强型PWM模块(ETPWM)的硬件抽象层头文件
#include "HL_system.h"      // 包含系统相关的硬件抽象层头文件(如HCLK_FREQ)

#include "fassert.h"        // 包含断言宏(FAS_ASSERT)的头文件
#include "foxmath.h"        // 包含数学相关宏(如UNIT_CONVERSION_FACTOR_100_FLOAT)的头文件
#include "fsystem.h"        // 包含系统权限控制函数(如提权/降级)的头文件

#include <math.h>           // 包含标准数学库头文件
#include <stdint.h>         // 包含标准整数类型定义(如uint16_t, uint32_t等)

/*========== 宏和定义 =========================================================*/
/** 千分比下限阈值 */
#define PWM_LOWER_THRESHOLD_PERM (1u)        // 定义占空比的下限为1‰，防止0%导致硬件异常

/** 千分比上限阈值 */
#define PWM_UPPER_THRESHOLD_PERM (999u)      // 定义占空比的上限为999‰，防止100%导致硬件异常

/** 千分比满周期 */
#define PWM_FULL_PERIOD_PERM (1000u)         // 定义满周期为1000‰，即100%

/** 存储模块不同部分的初始化状态 */
typedef struct {
    bool ecapInitialized;  /**< 增强捕获模块的初始化状态 */
    bool etpwmInitialized; /**< PWM模块的初始化状态 */
} PWM_INITIALIZATION_STATE_s;                // 定义用于记录PWM和ECAP初始化状态的结构体类型

/*========== 静态常量和变量定义 =======================*/
static PWM_INITIALIZATION_STATE_s pwm_state = { // 定义静态变量，记录模块的初始化状态
    .ecapInitialized  = false,                 // 初始状态下ECAP未初始化
    .etpwmInitialized = false,                 // 初始状态下ETPWM未初始化
};

/** 线性偏移量 (通过输出电路产生) */
static const int16_t pwm_kLinearOffset = 0;   // 定义静态常量，用于占空比的线性补偿，当前设为0

static PWM_SIGNAL_s ecap_inputPwmSignal = {   // 定义静态变量，存储ECAP捕获到的PWM信号数据
    .dutyCycle_perc = 0.0f,                    // 初始占空比设为0.0%
    .frequency_Hz = 0.0f                       // 初始频率设为0.0Hz
};

/*========== 外部常量和变量定义 =======================*/

/*========== 静态函数原型 =====================================*/
/** 返回ePWM的时间周期 (目前针对ePWM1A)
 * @return  以计数器刻度(ticks)为单位的ePWM时间周期
 */
static uint16_t PWM_GetEtpwmTimePeriod(void); // 静态函数声明：获取ePWM的周期计数值

/** 根据占空比返回一个ePWM计数器值
 * @param[in]   dutyCycle_perm  千分比表示的占空比
 * @return 计数器值
 */
static uint16_t PWM_ComputeCounterValueFromDutyCycle(uint16_t dutyCycle_perm); // 静态函数声明：将占空比转换为计数器比较值

/*========== 静态函数实现 ================================*/
static uint16_t PWM_GetEtpwmTimePeriod(void) {
    etpwm_config_reg_t etPwmConfig = {0};      // 定义ePWM配置寄存器结构体并清零初始化
    etpwm1GetConfigValue(&etPwmConfig, CurrentValue); /* 获取当前配置值 */
    return etPwmConfig.CONFIG_TBPRD;           // 返回时基周期寄存器的值
}

static uint16_t PWM_ComputeCounterValueFromDutyCycle(uint16_t dutyCycle_perm) {
    FAS_ASSERT((dutyCycle_perm >= PWM_LOWER_THRESHOLD_PERM) && (dutyCycle_perm <= PWM_UPPER_THRESHOLD_PERM)); // 断言：确保传入的占空比在1‰到999‰之间

    uint16_t basePeriod   = PWM_GetEtpwmTimePeriod(); // 获取ePWM的基准周期计数值
    uint32_t counterValue = (((uint32_t)basePeriod * (uint32_t)dutyCycle_perm) / PWM_FULL_PERIOD_PERM); // 计算占空比对应的计数器值：基准周期 * 占空比 / 1000，转uint32_t防溢出

    FAS_ASSERT(counterValue <= (uint16_t)UINT16_MAX); // 断言：确保计算出的计数值没有超出16位无符号整数的最大值
    return (uint16_t)counterValue;                    // 将计算结果强制转换为uint16_t并返回
}

/*========== 外部函数实现 ================================*/
extern void PWM_Initialize(void) {
    etpwmInit();                               // 初始化ETPWM模块
    pwm_state.etpwmInitialized = true;         // 将ETPWM的初始化状态标志置为true
    ecapInit();                                // 初始化ECAP模块
    pwm_state.ecapInitialized = true;          // 将ECAP的初始化状态标志置为true
}

extern void PWM_StartPwm(void) {
    /* 进入特权模式以访问控制寄存器 */
    const int32_t raisePrivilegeResult = FSYS_RaisePrivilege(); // 请求提升CPU权限至特权模式，并获取返回结果
    FAS_ASSERT(raisePrivilegeResult == 0);    // 断言：确保提权成功(返回值为0)
    etpwmStartTBCLK();                        // 启动ePWM时基时钟
    /* 完成；返回用户模式 */
    FSYS_SwitchToUserMode();                  // 切换回用户模式(降权)，保证系统安全
}

extern void PWM_StopPwm(void) {
    /* 进入特权模式以访问控制寄存器 */
    const int32_t raisePrivilegeResult = FSYS_RaisePrivilege(); // 请求提升CPU权限至特权模式，并获取返回结果
    FAS_ASSERT(raisePrivilegeResult == 0);    // 断言：确保提权成功(返回值为0)
    etpwmStopTBCLK();                         // 停止ePWM时基时钟
    /* 完成；返回用户模式 */
    FSYS_SwitchToUserMode();                  // 切换回用户模式(降权)，保证系统安全
}

extern void PWM_SetDutyCycle(uint16_t dutyCycle_perm) {
    FAS_ASSERT(dutyCycle_perm <= (uint16_t)INT16_MAX); // 断言：确保传入的占空比不超过16位有符号整数的最大值
    int16_t intermediateDutyCycle_perm = (int16_t)dutyCycle_perm + pwm_kLinearOffset; // 将占空比转为有符号数并加上线性偏移量
    /* 防止向下溢出(负数越界) */
    if (intermediateDutyCycle_perm < 0) {     // 如果加上偏移量后占空比变为负数
        intermediateDutyCycle_perm = 0;       // 则将其钳位至0，防止溢出
    }
    uint16_t correctedDutyCycle_perm = (uint16_t)intermediateDutyCycle_perm; // 将修正后的有符号占空比转换回无符号类型

    if (correctedDutyCycle_perm < PWM_LOWER_THRESHOLD_PERM) { // 如果修正后的占空比小于下限阈值(1‰)
        correctedDutyCycle_perm = PWM_LOWER_THRESHOLD_PERM;   // 则将其钳位至下限阈值
    }

    if (correctedDutyCycle_perm > PWM_UPPER_THRESHOLD_PERM) { // 如果修正后的占空比大于上限阈值(999‰)
        correctedDutyCycle_perm = PWM_UPPER_THRESHOLD_PERM;   // 则将其钳位至上限阈值
    }

    etpwmSetCmpA(etpwmREG1, PWM_ComputeCounterValueFromDutyCycle(correctedDutyCycle_perm)); // 根据最终修正的占空比计算计数器值，并设置到ePWM1的比较A寄存器中
}

/** 在发生ECAP中断时调用，在HAL层中定义为弱函数 */
/* AXIVION Next Codeline Style Linker-Multiple_Definition: TI HAL仅提供弱实现 */
/* AXIVION Next Codeline Style MisraC2012-2.7: API需要此参数 */
extern void ecapNotification(ecapBASE_t *ecap, uint16 flags) {
    FAS_ASSERT(ecap != NULL_PTR);             // 断言：确保ECAP寄存器基地址指针不为空
    /* AXIVION Routine Generic-MissingParameterAssert: flags: 参数接受整个范围 */

    /* 上升沿的计数值 */
    uint32_t capture1 = ecapGetCAP1(ecapREG1); // 获取捕获寄存器1的值(上升沿时间点)
    /* 下降沿的计数值 */
    uint32_t capture2 = ecapGetCAP2(ecapREG1); // 获取捕获寄存器2的值(下降沿时间点)
    /* 下一个上升沿的计数值 */
    uint32_t capture3 = ecapGetCAP3(ecapREG1); // 获取捕获寄存器3的值(下一个上升沿时间点)

    if (capture3 != capture1) {               // 如果capture3不等于capture1，说明测到了完整周期
        /* 计数器3 - 计数器1: 以计数器刻度表示的周期 */
        /* 将MHz转换为Hz */
        ecap_inputPwmSignal.frequency_Hz = 1.0f / ((float_t)(capture3 - capture1) / (HCLK_FREQ * 1000000.0f)); // 计算频率：周期时间 = (capture3-capture1)/时钟频率；频率 = 1/周期时间

        /* 计数器2 - 计数器1: 以计数器刻度表示的占空比 */
        ecap_inputPwmSignal.dutyCycle_perc = (float_t)(capture2 - capture1) / (float_t)(capture3 - capture1) *
                                             UNIT_CONVERSION_FACTOR_100_FLOAT; // 计算占空比：(高电平时间 / 周期时间) * 100.0f 转换为百分比
    } else {                                  // 如果capture3等于capture1，说明无信号或频率为0
        ecap_inputPwmSignal.frequency_Hz   = 0.0f; // 频率置0
        ecap_inputPwmSignal.dutyCycle_perc = 0.0f;  // 占空比置0
    }
}

bool PWM_IsEcapModuleInitialized(void) {
    return pwm_state.ecapInitialized;         // 返回ECAP模块的初始化状态
}

extern PWM_SIGNAL_s PWM_GetPwmData(void) {
    /* TODO: 如何确保数值已被更新？添加时间戳？添加计数器？*/
    return ecap_inputPwmSignal;               // 返回ECAP捕获到的PWM信号数据(频率和占空比)
}

/*========== 静态变量的Getter函数 (单元测试) ========================*/
#ifdef UNITY_UNIT_TEST                         // 如果定义了UNITY_UNIT_TEST宏(单元测试环境)
extern int16_t TEST_PWM_GetLinearOffset(void) {
    return pwm_kLinearOffset;                 // 测试专用：获取线性偏移量
}

extern void TEST_PWM_GetInitializedBools(bool *ecap, bool *etpwm) {
    *ecap  = pwm_state.ecapInitialized;       // 测试专用：通过指针输出ECAP初始化状态
    *etpwm = pwm_state.etpwmInitialized;      // 测试专用：通过指针输出ETPWM初始化状态
}
#endif

/*========== 外部化的静态函数实现 (单元测试) =======*/
#ifdef UNITY_UNIT_TEST                         // 如果定义了UNITY_UNIT_TEST宏(单元测试环境)
#endif
