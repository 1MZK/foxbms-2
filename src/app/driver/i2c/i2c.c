/**
 *
 * @copyright &copy; 2010 - 2026, Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 * 保留所有权利。
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * 允许在源代码和二进制形式下的重新分发和使用，无论是否经过修改，
 * 只要满足以下条件：
 *
 * 1. 源代码的重新分发必须保留上述版权声明、此条件列表和以下免责声明。
 *
 * 2. 二进制形式的重新分发必须在随分发提供的文档和/或其他材料中
 * 复制上述版权声明、此条件列表和以下免责声明。
 *
 * 3. 未经特定事先书面许可，版权持有者的名称及其贡献者的名称
 * 不得用于认可或推广源自本软件的产品。
 *
 * 本软件由版权持有者和贡献者“按原样”提供，不提供任何明示或暗示的保证，
 * 包括但不限于对适销性和特定用途适用性的暗示保证。
 * 在任何情况下，版权持有者或贡献者均不对任何直接、间接、偶然、特殊、惩罚性或
 * 后果性损害（包括但不限于替代商品或服务的采购、使用、数据或利润的损失，
 * 或业务中断）负责，无论是基于何种责任理论，无论是合同责任、严格责任还是侵权
 * （包括疏忽或其他），即使已被告知可能发生此类损害，也是如此。
 *
 * 我们恳请您在您的硬件、软件、文档或广告材料中使用以下一个或多个短语来指代
 * foxBMS：
 *
 * - "本产品使用了 foxBMS&reg; 的部分内容"
 * - "本产品包含了 foxBMS&reg; 的部分内容"
 * - "本产品衍生自 foxBMS&reg;"
 *
 */

/**
 * @file    i2c.c
 * @author  foxBMS 团队
 * @date    2021-07-22 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup DRIVERS
 * @prefix  I2C
 *
 * @brief   I2C 模块驱动
 *
 */

/*========== 包含文件 =======================================================*/
#include "i2c.h"            /* 包含I2C模块自身的头文件 */

#include "HL_system.h"      /* 包含HAL层系统相关头文件 */

#include "database.h"       /* 包含数据库相关头文件 */
#include "diag.h"           /* 包含诊断模块头文件 */
#include "dma.h"            /* 包含DMA（直接内存访问）模块头文件 */
#include "fstd_types.h"     /* 包含标准类型定义头文件 */
#include "fsystem.h"        /* 包含系统功能头文件 */
#include "mcu.h"            /* 包含微控制器单元相关头文件 */
#include "os.h"             /* 包含操作系统相关头文件 */

#include <stdbool.h>        /* 包含布尔类型支持 */
#include <stdint.h>         /* 包含标准整数类型支持 */

/*========== 宏和定义 =======================================================*/

/*========== 静态常量和变量定义 ===============================================*/
/* 指向接收字节被写入的表的最后一个字节的指针，pI2cInterface (接口1) */
uint8_t i2c_rxLastByteInterface1 = 0u;
/* 指向接收字节被写入的表的最后一个字节的指针，i2cREG2 (接口2) */
uint8_t i2c_rxLastByteInterface2 = 0u;

/*========== 外部常量和变量定义 ===============================================*/

/*========== 静态函数原型 =====================================================*/
/**
 * @brief   返回一个字（word）的传输时间（微秒）。
 * @details 该函数使用接口的时钟设置来确定传输一个字所需的时间。
 *          字表示一个字节加上 ACK（应答）位。
 * @param   pI2cInterface 使用的 I2C 接口
 * @return  在 I2C 接口上传输一个字节所需的时间（微秒）
 */
static uint32_t I2C_GetWordTransmitTime(i2cBASE_t *pI2cInterface);
/**
 * @brief   等待 I2C 发送缓冲区为空。
 * @details 当缓冲区为空时，可以发送下一个字节。
 *          如果在 timeout_us 微秒超时之前缓冲区为空，则函数返回 true，
 *          否则返回 false。如果检测到 NACK（非应答）条件，函数也返回 false。
 * @param   pI2cInterface 使用的 I2C 接口
 * @param   timeout_us    等待缓冲区为空的超时时间（微秒）
 * @return  如果在超时时间内缓冲区为空则返回 true，否则返回 false
 */
static bool I2C_WaitTransmit(i2cBASE_t *pI2cInterface, uint32_t timeout_us);
/**
 * @brief   等待检测到停止条件。
 * @details 当发出停止条件时，此函数等待直到在总线上检测到停止条件。
 *          这意味着传输已完成。如果在 timeout_us 微秒超时之前检测到停止条件，
 *          函数返回 true，否则返回 false。
 * @param   pI2cInterface 使用的 I2C 接口
 * @param   timeout_us    等待检测到停止条件的超时时间（微秒）
 * @return  如果在超时时间内检测到停止条件则返回 true，否则返回 false
 */
static bool I2C_WaitStop(i2cBASE_t *pI2cInterface, uint32_t timeout_us);

/**
 * @brief   使用通知机制等待 I2C 发送通信完成
 *
 * @return  如果接收到通知则返回 I2C_TX_NOTIFIED_VALUE，
 *          如果超时则返回 I2C_NO_NOTIFIED_VALUE
 */
static uint32_t I2C_WaitForTxCompletedNotification(void);
/**
 * @brief   使用通知机制等待 I2C 接收通信完成
 *
 * @return  如果接收到通知则返回 I2C_RX_NOTIFIED_VALUE，
 *          如果超时则返回 I2C_NO_NOTIFIED_VALUE
 */
static uint32_t I2C_WaitForRxCompletedNotification(void);
/**
 * @brief   清除挂起的通知
 *
 */
static void I2C_ClearNotifications(void);

/*========== 静态函数实现 ====================================================*/
static uint32_t I2C_GetWordTransmitTime(i2cBASE_t *pI2cInterface) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    uint32_t i2cClock_kHz        = 0;      /* I2C时钟频率，单位千赫兹 */
    uint32_t prescaler           = 0;      /* 预分频器值 */
    uint32_t wordTransmitTime_us = 0u;     /* 字传输时间，单位微秒 */
    uint8_t dFactor              = 0u;     /* D因子，用于时钟计算 */

    /* 获取预分频器值 */
    prescaler = pI2cInterface->PSC & I2C_PRESCALER_MASK; /* 从预分频寄存器读取并掩码获取预分频值 */
    if (prescaler == 0u) {                               /* 如果预分频值为0 */
        dFactor = I2C_DFACTOR_VALUE_PRESCALER_0;         /* D因子取预分频为0时的值 */
    } else if (prescaler == 1u) {                        /* 如果预分频值为1 */
        dFactor = I2C_DFACTOR_VALUE_PRESCALER_1;         /* D因子取预分频为1时的值 */
    } else {                                             /* 如果预分频值为其他 */
        dFactor = I2C_DFACTOR_VALUE_PRESCALER_OTHER;     /* D因子取其他预分频时的值 */
    }
    /* 这是HAL中使用的方程；似乎与技术参考手册不同
        (文档参考: p.1769 eq.65, SPNU563A - 2018年3月) */
    i2cClock_kHz = (uint32_t)(AVCLK1_FREQ * I2C_FACTOR_MHZ_TO_HZ) / /* 计算I2C时钟频率(kHz)：AVCLK1频率转换为Hz后除以分频计算 */
                   (2u * (prescaler + 1u) * (pI2cInterface->CKH + dFactor)); /* 分母为2*(预分频+1)*(高电平时间+D因子) */
    wordTransmitTime_us = (I2C_FACTOR_WORD_TO_BITS * I2C_FACTOR_S_TO_US) / i2cClock_kHz; /* 计算字传输时间(us)：字位数*秒转微秒 / 时钟频率 */
    return wordTransmitTime_us; /* 返回计算出的字传输时间 */
}

static bool I2C_WaitTransmit(i2cBASE_t *pI2cInterface, uint32_t timeout_us) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    bool success          = true;          /* 成功标志，初始为true */
    bool timeElapsed      = false;         /* 超时标志，初始为false */
    uint32_t startCounter = MCU_GetFreeRunningCount(); /* 获取MCU自由运行计数器的起始值 */

    /* 循环等待：未发生NACK中断 且 未发生发送中断 且 未超时 */
    while (((pI2cInterface->STR & (uint32_t)I2C_NACK_INT) == 0u) &&
           ((pI2cInterface->STR & (uint32_t)I2C_TX_INT) == 0u) && (timeElapsed == false)) {
        timeElapsed = MCU_IsTimeElapsed(startCounter, timeout_us); /* 检查是否已经超时 */
    }

    if (timeElapsed == true) { /* 如果超时 */
        success = false;       /* 设置成功标志为false */
    }

    return success; /* 返回成功标志 */
}

static bool I2C_WaitStop(i2cBASE_t *pI2cInterface, uint32_t timeout_us) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    bool success          = true;          /* 成功标志，初始为true */
    bool timeElapsed      = false;         /* 超时标志，初始为false */
    uint32_t startCounter = MCU_GetFreeRunningCount(); /* 获取MCU自由运行计数器的起始值 */

    /* 循环等待：未检测到停止条件 且 未超时 */
    while ((i2cIsStopDetected(pI2cInterface) == 0u) && (timeElapsed == false)) {
        timeElapsed = MCU_IsTimeElapsed(startCounter, timeout_us); /* 检查是否已经超时 */
    }

    if (timeElapsed == true) { /* 如果超时 */
        success = false;       /* 设置成功标志为false */
    }

    return success; /* 返回成功标志 */
}

static uint32_t I2C_WaitForTxCompletedNotification(void) {
    uint32_t notifiedValueTx = I2C_NO_NOTIFIED_VALUE; /* 初始化发送通知值为无通知 */
    /**
     * 挂起任务并等待 I2C DMA 发送完成通知，
     * 进入和退出时清除通知值
     */
    OS_WaitForNotificationIndexed(I2C_NOTIFICATION_TX_INDEX, &notifiedValueTx, I2C_NOTIFICATION_TIMEOUT_ms); /* 等待发送通知 */
    return notifiedValueTx; /* 返回接收到的通知值 */
}

static uint32_t I2C_WaitForRxCompletedNotification(void) {
    uint32_t notifiedValueRx = I2C_NO_NOTIFIED_VALUE; /* 初始化接收通知值为无通知 */
    /**
     * 挂起任务并等待 I2C DMA 接收完成通知，
     * 进入和退出时清除通知值
     */
    OS_WaitForNotificationIndexed(I2C_NOTIFICATION_RX_INDEX, &notifiedValueRx, I2C_NOTIFICATION_TIMEOUT_ms); /* 等待接收通知 */
    return notifiedValueRx; /* 返回接收到的通知值 */
}

static void I2C_ClearNotifications(void) {
    OS_ClearNotificationIndexed(I2C_NOTIFICATION_TX_INDEX); /* 清除发送完成通知 */
    OS_ClearNotificationIndexed(I2C_NOTIFICATION_RX_INDEX); /* 清除接收完成通知 */
}

/*========== 外部函数实现 ====================================================*/
extern void I2C_Initialize(void) {
    i2cInit(); /* 调用底层HAL初始化函数初始化I2C模块 */
}

extern STD_RETURN_TYPE_e I2C_Read(
    i2cBASE_t *pI2cInterface,
    uint32_t slaveAddress,
    uint32_t nrBytes,
    uint8_t *readData) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    FAS_ASSERT(readData != NULL_PTR);      /* 断言：接收数据缓冲区指针不为空 */
    FAS_ASSERT(nrBytes > 0u);              /* 断言：接收字节数大于0 */
    FAS_ASSERT(slaveAddress < 128u);       /* 断言：从机地址有效（小于128） */
    STD_RETURN_TYPE_e retVal = STD_OK;     /* 初始化返回值为成功 */

    if ((pI2cInterface->STR & (uint32_t)I2C_BUSBUSY) == 0u) { /* 如果I2C总线不忙 */
        /* 清除相关控制位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_STOP_COND);   /* 清除停止条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_START_COND);  /* 清除起始条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_REPEATMODE);  /* 清除重复模式位 */
        pI2cInterface->STR |= (uint32_t)I2C_TX_INT;         /* 清除发送中断标志（写1清除） */
        pI2cInterface->STR |= (uint32_t)I2C_RX_INT;         /* 清除接收中断标志（写1清除） */

        pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE;     /* 设置重复模式，因为读取通常在写操作之后 */

        i2cSetMode(pI2cInterface, (uint32_t)I2C_MASTER);        /* 设置为主机模式 */
        i2cSetDirection(pI2cInterface, (uint32_t)I2C_RECEIVER); /* 设置为接收方向 */
        i2cSetSlaveAdd(pI2cInterface, slaveAddress);            /* 设置从机地址 */
        i2cSetStart(pI2cInterface);                             /* 启动接收 */
        if (nrBytes == 1u) {                                    /* 如果只接收1个字节 */
            i2cSetStop(pI2cInterface); /* 立即产生停止条件，防止读取更多字节 */
        }
        /* 在轮询模式下接收 nrBytes 个字节 */
        for (uint16_t i = 0u; i < nrBytes; i++) { /* 循环接收每一个字节 */
            bool success = I2C_WaitReceive(pI2cInterface, I2C_TIMEOUT_us); /* 等待接收完成或超时 */
            if (((pI2cInterface->STR & (uint32_t)I2C_NACK_INT) == (uint32_t)I2C_NACK_INT) || (success == false)) {
                /* 如果收到NACK或等待超时 */
                pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
                i2cSetStop(pI2cInterface);                      /* 产生停止条件以终止通信 */
                retVal = STD_NOT_OK;                            /* 返回值为失败 */
                break;                                          /* 跳出循环 */
            }
            readData[i] = (uint8)(pI2cInterface->DRR & I2C_DDR_REGISTER_DATA_MASK); /* 读取数据寄存器并掩码获取有效数据 */
            if (i == (nrBytes - 2u)) { /* 如果当前是倒数第二个字节 */
                i2cSetStop(pI2cInterface); /* 产生停止条件，确保最后一个字节读取后总线释放 */
            }
        }

        bool success = I2C_WaitStop(pI2cInterface, I2C_TIMEOUT_us); /* 等待停止条件检测 */
        if (success == false) { /* 如果未检测到停止条件 */
            /* 设置停止条件 */
            pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
            i2cSetStop(pI2cInterface);                      /* 强制产生停止条件 */
            retVal = STD_NOT_OK;                            /* 返回值为失败 */
        }
    } else { /* 如果总线忙 */
        retVal = STD_NOT_OK; /* 返回值为失败 */
    }

    return retVal; /* 返回执行结果 */
}

extern STD_RETURN_TYPE_e I2C_Write(
    i2cBASE_t *pI2cInterface,
    uint32_t slaveAddress,
    uint32_t nrBytes,
    uint8_t *writeData) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    FAS_ASSERT(slaveAddress < 128u);       /* 断言：从机地址有效 */
    FAS_ASSERT(nrBytes > 0u);              /* 断言：发送字节数大于0 */
    FAS_ASSERT(writeData != NULL_PTR);     /* 断言：发送数据缓冲区指针不为空 */
    STD_RETURN_TYPE_e retVal = STD_OK;     /* 初始化返回值为成功 */

    if ((pI2cInterface->STR & (uint32_t)I2C_BUSBUSY) == 0u) { /* 如果I2C总线不忙 */
        /* 清除相关控制位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_STOP_COND);   /* 清除停止条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_START_COND);  /* 清除起始条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_REPEATMODE);  /* 清除重复模式位 */
        pI2cInterface->STR |= (uint32_t)I2C_TX_INT;         /* 清除发送中断标志 */
        pI2cInterface->STR |= (uint32_t)I2C_RX_INT;         /* 清除接收中断标志 */

        i2cSetMode(pI2cInterface, (uint32_t)I2C_MASTER);           /* 设置为主机模式 */
        i2cSetDirection(pI2cInterface, (uint32_t)I2C_TRANSMITTER); /* 设置为发送方向 */
        i2cSetSlaveAdd(pI2cInterface, slaveAddress);               /* 设置从机地址 */
        i2cSetStop(pI2cInterface);                                 /* 发送 nrBytes 字节后产生停止条件 */
        i2cSetCount(pI2cInterface, nrBytes);                       /* 设置在停止条件前发送的字节数 */
        i2cSetStart(pI2cInterface);                                /* 启动发送 */

        /* 在轮询模式下发送 nrBytes 个字节 */
        for (uint16_t i = 0u; i < nrBytes; i++) { /* 循环发送每一个字节 */
            pI2cInterface->DXR = (uint32_t)writeData[i]; /* 将数据写入数据发送寄存器 */
            bool success       = I2C_WaitTransmit(pI2cInterface, I2C_TIMEOUT_us); /* 等待发送完成或超时 */
            if (((pI2cInterface->STR & (uint32_t)I2C_NACK_INT) == (uint32_t)I2C_NACK_INT) || (success == false)) {
                /* 如果收到NACK或等待超时 */
                pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
                i2cSetStop(pI2cInterface);                      /* 产生停止条件 */
                retVal = STD_NOT_OK;                            /* 返回值为失败 */
                break;                                          /* 跳出循环 */
            }
        }

        bool success = I2C_WaitStop(pI2cInterface, I2C_TIMEOUT_us); /* 等待停止条件检测 */
        if (success == false) { /* 如果未检测到停止条件 */
            /* 设置停止条件 */
            pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
            i2cSetStop(pI2cInterface);                      /* 强制产生停止条件 */
            retVal = STD_NOT_OK;                            /* 返回值为失败 */
        }
    } else { /* 如果总线忙 */
        retVal = STD_NOT_OK; /* 返回值为失败 */
    }

    return retVal; /* 返回执行结果 */
}

extern STD_RETURN_TYPE_e I2C_WriteRead(
    i2cBASE_t *pI2cInterface,
    uint32_t slaveAddress,
    uint32_t nrBytesWrite,
    uint8_t *writeData,
    uint32_t nrBytesRead,
    uint8_t *readData) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    FAS_ASSERT(writeData != NULL_PTR);     /* 断言：发送数据缓冲区指针不为空 */
    FAS_ASSERT(nrBytesWrite > 0u);         /* 断言：发送字节数大于0 */
    FAS_ASSERT(readData != NULL_PTR);      /* 断言：接收数据缓冲区指针不为空 */
    FAS_ASSERT(nrBytesRead > 0u);          /* 断言：接收字节数大于0 */
    FAS_ASSERT(slaveAddress < 128u);       /* 断言：从机地址有效 */
    STD_RETURN_TYPE_e retVal = STD_OK;     /* 初始化返回值为成功 */

    if ((pI2cInterface->STR & (uint32_t)I2C_BUSBUSY) == 0u) { /* 如果I2C总线不忙 */
        /* 清除相关控制位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_STOP_COND);   /* 清除停止条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_START_COND);  /* 清除起始条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_REPEATMODE);  /* 清除重复模式位 */
        pI2cInterface->STR |= (uint32_t)I2C_TX_INT;         /* 清除发送中断标志 */
        pI2cInterface->STR |= (uint32_t)I2C_RX_INT;         /* 清除接收中断标志 */

        i2cSetMode(pI2cInterface, (uint32_t)I2C_MASTER);           /* 设置为主机模式 */
        i2cSetDirection(pI2cInterface, (uint32_t)I2C_TRANSMITTER); /* 设置为发送方向 */
        i2cSetSlaveAdd(pI2cInterface, slaveAddress);               /* 设置从机地址 */
        i2cSetStart(pI2cInterface);                                /* 启动发送 */

        bool success = true; /* 初始化成功标志 */

        /* 在轮询模式下发送 nrBytesWrite 个字节 */
        for (uint16_t i = 0u; i < nrBytesWrite; i++) { /* 循环发送每一个字节 */
            pI2cInterface->DXR = (uint32_t)writeData[i]; /* 将数据写入数据发送寄存器 */
            success            = I2C_WaitTransmit(pI2cInterface, I2C_TIMEOUT_us); /* 等待发送完成或超时 */
            if (((pI2cInterface->STR & (uint32_t)I2C_NACK_INT) == (uint32_t)I2C_NACK_INT) || (success == false)) {
                /* 如果收到NACK或等待超时 */
                pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
                i2cSetStop(pI2cInterface);                      /* 产生停止条件 */
                retVal = STD_NOT_OK;                            /* 返回值为失败 */
                break;                                          /* 跳出循环 */
            }
        }

        if (!(((pI2cInterface->STR & (uint32_t)I2C_NACK_INT) == (uint32_t)I2C_NACK_INT) || (success == false))) {
            /* 如果发送阶段没有发生NACK或超时，则继续接收 */
            pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式，发起重复起始条件 */
            i2cSetMode(pI2cInterface, (uint32_t)I2C_MASTER);        /* 设置为主机模式 */
            i2cSetDirection(pI2cInterface, (uint32_t)I2C_RECEIVER); /* 设置为接收方向 */
            i2cSetStart(pI2cInterface);                             /* 启动接收 */
            if (nrBytesRead == 1u) {                                /* 如果只接收1个字节 */
                i2cSetStop(pI2cInterface); /* 产生停止条件 */
            }
            /* 在轮询模式下接收 nrBytes 个字节 */
            for (uint16_t i = 0u; i < nrBytesRead; i++) { /* 循环接收每一个字节 */
                success = I2C_WaitReceive(pI2cInterface, I2C_TIMEOUT_us); /* 等待接收完成或超时 */
                if (((pI2cInterface->STR & (uint32_t)I2C_NACK_INT) == (uint32_t)I2C_NACK_INT) || (success == false)) {
                    /* 如果收到NACK或等待超时 */
                    pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
                    i2cSetStop(pI2cInterface);                      /* 产生停止条件 */
                    retVal = STD_NOT_OK;                            /* 返回值为失败 */
                    break;                                          /* 跳出循环 */
                }
                readData[i] = (uint8)(pI2cInterface->DRR & I2C_DDR_REGISTER_DATA_MASK); /* 读取数据寄存器并掩码获取有效数据 */
                if (i == (nrBytesRead - 2u)) { /* 如果当前是倒数第二个字节 */
                    i2cSetStop(pI2cInterface); /* 产生停止条件 */
                }
            }
        }

        success = I2C_WaitStop(pI2cInterface, I2C_TIMEOUT_us); /* 等待停止条件检测 */
        if (success == false) { /* 如果未检测到停止条件 */
            /* 设置停止条件 */
            pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
            i2cSetStop(pI2cInterface);                      /* 强制产生停止条件 */
            retVal = STD_NOT_OK;                            /* 返回值为失败 */
        }
    } else { /* 如果总线忙 */
        retVal = STD_NOT_OK; /* 返回值为失败 */
    }

    return retVal; /* 返回执行结果 */
}

extern STD_RETURN_TYPE_e I2C_ReadDma(
    i2cBASE_t *pI2cInterface,
    uint32_t slaveAddress,
    uint32_t nrBytes,
    uint8_t *readData) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    FAS_ASSERT(readData != NULL_PTR);      /* 断言：接收数据缓冲区指针不为空 */
    FAS_ASSERT(nrBytes > 1u);              /* 断言：DMA接收字节数大于1 */
    FAS_ASSERT(slaveAddress < 128u);       /* 断言：从机地址有效 */
    STD_RETURN_TYPE_e retVal = STD_OK;     /* 初始化返回值为成功 */

    I2C_ClearNotifications(); /* 清除挂起的通知 */

    if ((pI2cInterface->STR & (uint32_t)I2C_BUSBUSY) == 0u) { /* 如果I2C总线不忙 */
        /* 清除相关控制位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_STOP_COND);   /* 清除停止条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_START_COND);  /* 清除起始条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_REPEATMODE);  /* 清除重复模式位 */
        pI2cInterface->STR |= (uint32_t)I2C_TX_INT;         /* 清除发送中断标志 */
        pI2cInterface->STR |= (uint32_t)I2C_RX_INT;         /* 清除接收中断标志 */

        /* DMA 配置 */
        dmaChannel_t channelRx = DMA_CH0; /* 初始化DMA接收通道 */
        if (pI2cInterface == i2cREG1) {   /* 如果是I2C接口1 */
            channelRx = DMA_CHANNEL_I2C1_RX; /* 使用I2C1的DMA接收通道 */
        } else if (pI2cInterface == i2cREG2) { /* 如果是I2C接口2 */
            channelRx = DMA_CHANNEL_I2C2_RX; /* 使用I2C2的DMA接收通道 */
        } else { /* 无效的I2C接口 */
            FAS_ASSERT(FAS_TRAP); /* 触发断言陷阱 */
        }

        OS_EnterTaskCritical(); /* 进入任务临界区 */

        /* 进入特权模式以写DMA配置寄存器 */
        const int32_t raisePrivilegeResult = FSYS_RaisePrivilege(); /* 提升系统特权级别 */
        FAS_ASSERT(raisePrivilegeResult == 0); /* 断言：特权提升成功 */

        /* 设置接收缓冲区地址 */
        /* AXIVION 禁用样式 MisraC2012-1.1: DMA配置必要的类型转换 */
        dmaRAMREG->PCP[(dmaChannel_t)channelRx].IDADDR = (uint32_t)readData; /* 将接收数据缓冲区地址写入DMA目的地址寄存器 */
        /* AXIVION 启用样式 MisraC2012-1.1: */
        /* 设置要接收的接收字节数，通过DMA接收 (nrBytes-1) 个字节 */
        dmaRAMREG->PCP[(dmaChannel_t)channelRx].ITCOUNT = ((nrBytes - 1u) << DMA_INITIAL_FRAME_COUNTER_POSITION) | 1u; /* 配置DMA帧计数 */

        dmaSetChEnable((dmaChannel_t)channelRx, (dmaTriggerType_t)DMA_HW); /* 使能DMA通道，硬件触发 */

        FSYS_SwitchToUserMode(); /* DMA配置寄存器已写入，退出特权模式，切回用户模式 */
        OS_ExitTaskCritical();   /* 退出任务临界区 */
        /* DMA 配置结束 */

        pI2cInterface->DMACR |= (uint32_t)I2C_RX_DMA_ENABLE; /* 激活 I2C DMA 接收 */

        pI2cInterface->MDR &= ~((uint32_t)I2C_STOP_COND);  /* 清除停止条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_START_COND); /* 清除起始条件位 */
        pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE;    /* 设置重复模式 */

        i2cSetMode(pI2cInterface, (uint32_t)I2C_MASTER);        /* 设置为主机模式 */
        i2cSetDirection(pI2cInterface, (uint32_t)I2C_RECEIVER); /* 设置为接收方向 */
        i2cSetSlaveAdd(pI2cInterface, slaveAddress);            /* 设置从机地址 */
        i2cSetStart(pI2cInterface);                             /* 启动接收 */

        uint32_t notificationRx = I2C_WaitForRxCompletedNotification(); /* 等待DMA接收完成通知 */
        if (notificationRx != I2C_RX_NOTIFIED_VALUE) { /* 如果未收到接收完成通知（超时） */
            /* 接收未发生，停用DMA */
            pI2cInterface->DMACR &= ~((uint32_t)I2C_RX_DMA_ENABLE); /* 禁用 I2C DMA 接收 */
            /* 设置停止条件 */
            pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
            i2cSetStop(pI2cInterface);                      /* 产生停止条件 */
            retVal = STD_NOT_OK;                            /* 返回值为失败 */
        } else { /* 接收成功发生 */
            if (pI2cInterface == i2cREG1) { /* 如果是I2C接口1 */
                readData[nrBytes - 1u] = i2c_rxLastByteInterface1; /* 从全局变量获取最后一个字节 */
            } else if (pI2cInterface == i2cREG2) { /* 如果是I2C接口2 */
                readData[nrBytes - 1u] = i2c_rxLastByteInterface2; /* 从全局变量获取最后一个字节 */
            } else { /* 无效的I2C接口 */
                FAS_ASSERT(FAS_TRAP); /* 触发断言陷阱 */
            }
            bool success = I2C_WaitStop(pI2cInterface, I2C_TIMEOUT_us); /* 等待停止条件检测 */
            if (success == false) { /* 如果未检测到停止条件 */
                /* 设置停止条件 */
                pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
                i2cSetStop(pI2cInterface);                      /* 强制产生停止条件 */
                retVal = STD_NOT_OK;                            /* 返回值为失败 */
            }
        }
    } else { /* 如果总线忙 */
        retVal = STD_NOT_OK; /* 返回值为失败 */
    }

    return retVal; /* 返回执行结果 */
}

extern STD_RETURN_TYPE_e I2C_WriteDma(
    i2cBASE_t *pI2cInterface,
    uint32_t slaveAddress,
    uint32_t nrBytes,
    uint8_t *writeData) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    FAS_ASSERT(writeData != NULL_PTR);     /* 断言：发送数据缓冲区指针不为空 */
    FAS_ASSERT(nrBytes > 0u);              /* 断言：发送字节数大于0 */
    FAS_ASSERT(slaveAddress < 128u);       /* 断言：从机地址有效 */
    STD_RETURN_TYPE_e retVal = STD_OK;     /* 初始化返回值为成功 */

    I2C_ClearNotifications(); /* 清除挂起的通知 */

    if ((pI2cInterface->STR & (uint32_t)I2C_BUSBUSY) == 0u) { /* 如果I2C总线不忙 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_STOP_COND);   /* 清除停止条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_START_COND);  /* 清除起始条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_REPEATMODE);  /* 清除重复模式位 */
        pI2cInterface->STR |= (uint32_t)I2C_TX_INT;         /* 清除发送中断标志 */
        pI2cInterface->STR |= (uint32_t)I2C_RX_INT;         /* 清除接收中断标志 */

        /* DMA 配置 */
        dmaChannel_t channelTx = DMA_CH0; /* 初始化DMA发送通道 */
        if (pI2cInterface == i2cREG1) {   /* 如果是I2C接口1 */
            channelTx = DMA_CHANNEL_I2C1_TX; /* 使用I2C1的DMA发送通道 */
        } else if (pI2cInterface == i2cREG2) { /* 如果是I2C接口2 */
            channelTx = DMA_CHANNEL_I2C2_TX; /* 使用I2C2的DMA发送通道 */
        } else { /* 无效的I2C接口 */
            FAS_ASSERT(FAS_TRAP); /* 触发断言陷阱 */
        }

        OS_EnterTaskCritical(); /* 进入任务临界区 */

        /* 进入特权模式以写DMA配置寄存器 */
        const int32_t raisePrivilegeResult = FSYS_RaisePrivilege(); /* 提升系统特权级别 */
        FAS_ASSERT(raisePrivilegeResult == 0); /* 断言：特权提升成功 */

        /* 设置发送缓冲区地址 */
        /* AXIVION 禁用样式 MisraC2012-1.1: DMA配置必要的类型转换 */
        dmaRAMREG->PCP[(dmaChannel_t)channelTx].ISADDR = (uint32_t)writeData; /* 将发送数据缓冲区地址写入DMA源地址寄存器 */
        /* AXIVION 启用样式 MisraC2012-1.1: */
        /* 设置要发送的发送字节数 */
        dmaRAMREG->PCP[(dmaChannel_t)channelTx].ITCOUNT = (nrBytes << DMA_INITIAL_FRAME_COUNTER_POSITION) | 1u; /* 配置DMA帧计数 */

        dmaSetChEnable((dmaChannel_t)channelTx, (dmaTriggerType_t)DMA_HW); /* 使能DMA通道，硬件触发 */

        FSYS_SwitchToUserMode(); /* DMA配置寄存器已写入，退出特权模式，切回用户模式 */
        OS_ExitTaskCritical();   /* 退出任务临界区 */
        /* DMA 配置结束 */

        /* 清除相关控制位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_STOP_COND);   /* 清除停止条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_START_COND);  /* 清除起始条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_REPEATMODE);  /* 清除重复模式位 */
        pI2cInterface->STR |= (uint32_t)I2C_TX_INT;         /* 清除发送中断标志 */
        pI2cInterface->STR |= (uint32_t)I2C_RX_INT;         /* 清除接收中断标志 */

        i2cSetMode(pI2cInterface, (uint32_t)I2C_MASTER);           /* 设置为主机模式 */
        i2cSetDirection(pI2cInterface, (uint32_t)I2C_TRANSMITTER); /* 设置为发送方向 */
        i2cSetSlaveAdd(pI2cInterface, slaveAddress);               /* 设置从机地址 */
        i2cSetStop(pI2cInterface);                                 /* 发送 nrBytes 字节后产生停止条件 */
        i2cSetCount(pI2cInterface, nrBytes);                       /* 设置在停止条件前发送的字节数 */
        pI2cInterface->DMACR |= (uint32_t)I2C_TX_DMA_ENABLE;       /* 激活 I2C DMA 发送 */
        i2cSetStart(pI2cInterface);                                /* 启动发送 */

        uint32_t notificationTx = I2C_WaitForTxCompletedNotification(); /* 等待DMA发送完成通知 */
        if (notificationTx != I2C_TX_NOTIFIED_VALUE) { /* 如果未收到发送完成通知（超时） */
            /* 发送未发生，停用DMA */
            pI2cInterface->DMACR &= ~((uint32_t)I2C_TX_DMA_ENABLE); /* 禁用 I2C DMA 发送 */
            /* 设置停止条件 */
            pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
            i2cSetStop(pI2cInterface);                      /* 产生停止条件 */
            retVal = STD_NOT_OK;                            /* 返回值为失败 */
        } else { /* 发送成功发生 */
            bool success = I2C_WaitStop(pI2cInterface, I2C_TIMEOUT_us); /* 等待停止条件检测 */
            if (success == false) { /* 如果未检测到停止条件 */
                /* 设置停止条件 */
                pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
                i2cSetStop(pI2cInterface);                      /* 强制产生停止条件 */
                retVal = STD_NOT_OK;                            /* 返回值为失败 */
            }
        }
    } else { /* 如果总线忙 */
        retVal = STD_NOT_OK; /* 返回值为失败 */
    }

    return retVal; /* 返回执行结果 */
}

extern STD_RETURN_TYPE_e I2C_WriteReadDma(
    i2cBASE_t *pI2cInterface,
    uint32_t slaveAddress,
    uint32_t nrBytesWrite,
    uint8_t *writeData,
    uint32_t nrBytesRead,
    uint8_t *readData) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    FAS_ASSERT(slaveAddress < 128u);       /* 断言：从机地址有效 */
    FAS_ASSERT(nrBytesWrite > 0u);         /* 断言：发送字节数大于0 */
    FAS_ASSERT(writeData != NULL_PTR);     /* 断言：发送数据缓冲区指针不为空 */
    FAS_ASSERT(nrBytesRead > 1u);          /* 断言：DMA接收字节数大于1 */
    FAS_ASSERT(readData != NULL_PTR);      /* 断言：接收数据缓冲区指针不为空 */
    STD_RETURN_TYPE_e retVal = STD_OK;     /* 初始化返回值为成功 */
    dmaChannel_t channelRx   = DMA_CH0;   /* 初始化DMA接收通道 */
    dmaChannel_t channelTx   = DMA_CH0;   /* 初始化DMA发送通道 */

    I2C_ClearNotifications(); /* 清除挂起的通知 */

    if ((pI2cInterface->STR & (uint32_t)I2C_BUSBUSY) == 0u) { /* 如果I2C总线不忙 */
        /* 首先写入字节 */

        /* 清除相关控制位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_STOP_COND);   /* 清除停止条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_START_COND);  /* 清除起始条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_REPEATMODE);  /* 清除重复模式位 */
        pI2cInterface->STR |= (uint32_t)I2C_TX_INT;         /* 清除发送中断标志 */
        pI2cInterface->STR |= (uint32_t)I2C_RX_INT;         /* 清除接收中断标志 */

        /* DMA 配置 */
        if (pI2cInterface == i2cREG1) { /* 如果是I2C接口1 */
            channelTx = DMA_CHANNEL_I2C1_TX; /* 使用I2C1的DMA发送通道 */
        } else if (pI2cInterface == i2cREG2) { /* 如果是I2C接口2 */
            channelTx = DMA_CHANNEL_I2C2_TX; /* 使用I2C2的DMA发送通道 */
        } else { /* 无效的I2C接口 */
            FAS_ASSERT(FAS_TRAP); /* 触发断言陷阱 */
        }

        OS_EnterTaskCritical(); /* 进入任务临界区 */

        /* 进入特权模式以写DMA配置寄存器 */
        const int32_t raisePrivilegeResultWrite = FSYS_RaisePrivilege(); /* 提升系统特权级别 */
        FAS_ASSERT(raisePrivilegeResultWrite == 0); /* 断言：特权提升成功 */

        /* 设置发送缓冲区地址 */
        /* AXIVION 禁用样式 MisraC2012-1.1: DMA配置必要的类型转换 */
        dmaRAMREG->PCP[(dmaChannel_t)channelTx].ISADDR = (uint32_t)writeData; /* 将发送数据缓冲区地址写入DMA源地址寄存器 */
        /* AXIVION 启用样式 MisraC2012-1.1: */
        /* 设置要发送的发送字节数 */
        dmaRAMREG->PCP[(dmaChannel_t)channelTx].ITCOUNT = (nrBytesWrite << DMA_INITIAL_FRAME_COUNTER_POSITION) | 1u; /* 配置DMA帧计数 */

        dmaSetChEnable((dmaChannel_t)channelTx, (dmaTriggerType_t)DMA_HW); /* 使能DMA通道，硬件触发 */

        FSYS_SwitchToUserMode(); /* DMA配置寄存器已写入，退出特权模式，切回用户模式 */
        OS_ExitTaskCritical();   /* 退出任务临界区 */
        /* DMA 配置结束 */

        /* 清除相关控制位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_STOP_COND);   /* 清除停止条件位 */
        pI2cInterface->MDR &= ~((uint32_t)I2C_START_COND);  /* 清除起始条件位 */
        pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE;     /* 设置重复模式 */
        pI2cInterface->STR |= (uint32_t)I2C_TX_INT;         /* 清除发送中断标志 */
        pI2cInterface->STR |= (uint32_t)I2C_RX_INT;         /* 清除接收中断标志 */

        i2cSetMode(pI2cInterface, (uint32_t)I2C_MASTER);           /* 设置为主机模式 */
        i2cSetDirection(pI2cInterface, (uint32_t)I2C_TRANSMITTER); /* 设置为发送方向 */
        i2cSetSlaveAdd(pI2cInterface, slaveAddress);               /* 设置从机地址 */
        pI2cInterface->DMACR |= (uint32_t)I2C_TX_DMA_ENABLE;       /* 激活 I2C DMA 发送 */
        i2cSetStart(pI2cInterface);                                /* 启动发送 */

        uint32_t notificationTx = I2C_WaitForTxCompletedNotification(); /* 等待DMA发送完成通知 */
        if (notificationTx != I2C_TX_NOTIFIED_VALUE) { /* 如果未收到发送完成通知（超时） */
            /* 发送未发生，停用DMA */
            pI2cInterface->DMACR &= ~((uint32_t)I2C_TX_DMA_ENABLE); /* 禁用 I2C DMA 发送 */
            /* 设置停止条件 */
            pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
            i2cSetStop(pI2cInterface);                      /* 产生停止条件 */
            retVal = STD_NOT_OK;                            /* 返回值为失败 */
        } else { /* 发送成功，现在开始接收 */

            /* DMA 配置 */
            if (pI2cInterface == i2cREG1) { /* 如果是I2C接口1 */
                channelRx = DMA_CHANNEL_I2C1_RX; /* 使用I2C1的DMA接收通道 */
            } else if (pI2cInterface == i2cREG2) { /* 如果是I2C接口2 */
                channelRx = DMA_CHANNEL_I2C2_RX; /* 使用I2C2的DMA接收通道 */
            } else { /* 无效的I2C接口 */
                FAS_ASSERT(FAS_TRAP); /* 触发断言陷阱 */
            }

            OS_EnterTaskCritical(); /* 进入任务临界区 */

            /* 进入特权模式以写DMA配置寄存器 */
            const int32_t raisePrivilegeResultRead = FSYS_RaisePrivilege(); /* 提升系统特权级别 */
            FAS_ASSERT(raisePrivilegeResultRead == 0); /* 断言：特权提升成功 */

            /* 设置接收缓冲区地址 */
            /* AXIVION 禁用样式 MisraC2012-1.1: DMA配置必要的类型转换 */
            dmaRAMREG->PCP[(dmaChannel_t)channelRx].IDADDR = (uint32_t)readData; /* 将接收数据缓冲区地址写入DMA目的地址寄存器 */
            /* AXIVION 启用样式 MisraC2012-1.1: */
            /* 设置要接收的接收字节数 */
            dmaRAMREG->PCP[(dmaChannel_t)channelRx].ITCOUNT =
                ((nrBytesRead - 1u) << DMA_INITIAL_FRAME_COUNTER_POSITION) | 1u; /* 配置DMA帧计数 */

            dmaSetChEnable((dmaChannel_t)channelRx, (dmaTriggerType_t)DMA_HW); /* 使能DMA通道，硬件触发 */

            FSYS_SwitchToUserMode(); /* DMA配置寄存器已写入，退出特权模式，切回用户模式 */
            OS_ExitTaskCritical();   /* 退出任务临界区 */
            /* DMA 配置结束 */

            /* 由于我们不能等待停止条件来确保所有字节已在总线上发送完毕，
               等待直到传输完成 */
            uint32_t wordTransmitTime_us = I2C_GetWordTransmitTime(pI2cInterface) + I2C_TX_TIME_MARGIN_us; /* 计算带余量的字传输时间 */

            MCU_Delay_us(wordTransmitTime_us); /* 微秒级延时，确保发送完全结束 */

            /* 清除相关控制位 */
            pI2cInterface->MDR &= ~((uint32_t)I2C_STOP_COND);   /* 清除停止条件位 */
            pI2cInterface->MDR &= ~((uint32_t)I2C_START_COND);  /* 清除起始条件位 */
            pI2cInterface->MDR &= ~((uint32_t)I2C_REPEATMODE);  /* 清除重复模式位 */
            pI2cInterface->STR |= (uint32_t)I2C_TX_INT;         /* 清除发送中断标志 */
            pI2cInterface->STR |= (uint32_t)I2C_RX_INT;         /* 清除接收中断标志 */

            pI2cInterface->DMACR |= (uint32_t)I2C_RX_DMA_ENABLE; /* 激活 I2C DMA 接收 */

            pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
            i2cSetMode(pI2cInterface, (uint32_t)I2C_MASTER);        /* 设置为主机模式 */
            i2cSetDirection(pI2cInterface, (uint32_t)I2C_RECEIVER); /* 设置为接收方向 */
            i2cSetStart(pI2cInterface);                             /* 启动接收 */
                                                                    /* 使用 DMA 接收 nrBytes 个字节 */

            uint32_t notificationRx = I2C_WaitForRxCompletedNotification(); /* 等待DMA接收完成通知 */
            if (notificationRx != I2C_RX_NOTIFIED_VALUE) { /* 如果未收到接收完成通知（超时） */
                /* 接收未发生，停用DMA */
                pI2cInterface->DMACR &= ~((uint32_t)I2C_RX_DMA_ENABLE); /* 禁用 I2C DMA 接收 */
                /* 设置停止条件 */
                pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
                i2cSetStop(pI2cInterface);                      /* 产生停止条件 */
                retVal = STD_NOT_OK;                            /* 返回值为失败 */
            } else { /* 接收成功发生 */
                if (pI2cInterface == i2cREG1) { /* 如果是I2C接口1 */
                    readData[nrBytesRead - 1u] = i2c_rxLastByteInterface1; /* 从全局变量获取最后一个字节 */
                } else if (pI2cInterface == i2cREG2) { /* 如果是I2C接口2 */
                    readData[nrBytesRead - 1u] = i2c_rxLastByteInterface2; /* 从全局变量获取最后一个字节 */
                } else { /* 无效的I2C接口 */
                    FAS_ASSERT(FAS_TRAP); /* 触发断言陷阱 */
                }
                bool success = I2C_WaitStop(pI2cInterface, I2C_TIMEOUT_us); /* 等待停止条件检测 */
                if (success == false) { /* 如果未检测到停止条件 */
                    /* 设置停止条件 */
                    pI2cInterface->MDR |= (uint32_t)I2C_REPEATMODE; /* 设置重复模式 */
                    i2cSetStop(pI2cInterface);                      /* 强制产生停止条件 */
                    retVal = STD_NOT_OK;                            /* 返回值为失败 */
                }
            }
        }
    } else { /* 如果总线忙 */
        retVal = STD_NOT_OK; /* 返回值为失败 */
    }

    return retVal; /* 返回执行结果 */
}

extern uint8_t I2C_ReadLastRxByte(i2cBASE_t *pI2cInterface) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    uint8_t lastReadByte = (uint8)(pI2cInterface->DRR & I2C_DDR_REGISTER_DATA_MASK); /* 读取数据寄存器并掩码获取最后一次接收的字节 */
    return lastReadByte; /* 返回最后接收的字节 */
}

extern bool I2C_WaitReceive(i2cBASE_t *pI2cInterface, uint32_t timeout_us) {
    FAS_ASSERT(pI2cInterface != NULL_PTR); /* 断言：I2C接口指针不为空 */
    bool success          = true;          /* 成功标志，初始为true */
    bool timeElapsed      = false;         /* 超时标志，初始为false */
    uint32_t startCounter = MCU_GetFreeRunningCount(); /* 获取MCU自由运行计数器的起始值 */

    /* 循环等待：未发生接收中断 且 未超时 */
    while (((pI2cInterface->STR & (uint32_t)I2C_RX_INT) == 0u) && (timeElapsed == false)) {
        timeElapsed = MCU_IsTimeElapsed(startCounter, timeout_us); /* 检查是否已经超时 */
    }

    if (timeElapsed == true) { /* 如果超时 */
        success = false;       /* 设置成功标志为false */
    }

    return success; /* 返回成功标志 */
}

/*========== 外部化的静态函数实现（单元测试）================================*/
#ifdef UNITY_UNIT_TEST
extern uint32_t TEST_I2C_GetWordTransmitTime(i2cBASE_t *pI2cInterface) {
    return I2C_GetWordTransmitTime(pI2cInterface); /* 单元测试包装：获取字传输时间 */
}

extern bool TEST_I2C_WaitTransmit(i2cBASE_t *pI2cInterface, uint32_t timeout_us) {
    return I2C_WaitTransmit(pI2cInterface, timeout_us); /* 单元测试包装：等待发送完成 */
}

extern bool TEST_I2C_WaitStop(i2cBASE_t *pI2cInterface, uint32_t timeout_us) {
    return I2C_WaitStop(pI2cInterface, timeout_us); /* 单元测试包装：等待停止条件 */
}

extern uint32_t TEST_I2C_WaitForTxCompletedNotification(void) {
    return I2C_WaitForTxCompletedNotification(); /* 单元测试包装：等待发送完成通知 */
}

extern uint32_t TEST_I2C_WaitForRxCompletedNotification(void) {
    return I2C_WaitForRxCompletedNotification(); /* 单元测试包装：等待接收完成通知 */
}

extern void TEST_I2C_ClearNotifications(void) {
    I2C_ClearNotifications(); /* 单元测试包装：清除通知 */
}
#endif
