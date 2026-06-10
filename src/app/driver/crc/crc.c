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
 * @file    crc.c
 * @author  foxBMS 团队
 * @date    2022-02-22 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup DRIVERS
 * @prefix  CRC
 *
 * @brief   CRC 模块实现
 * @details 使用系统 CRC 硬件进行数据完整性计算
 */

/*========== 包含文件 =======================================================*/
#include "crc.h"          /* 包含CRC模块自身的头文件 */

#include "fassert.h"      /* 包含断言模块头文件 */
#include "fstd_types.h"   /* 包含标准类型定义头文件 */

#include <stdint.h>       /* 包含标准整数类型定义 */

/*========== 宏和定义 =======================================================*/

/*========== 静态常量和变量定义 ===============================================*/

/*========== 外部常量和变量定义 ===============================================*/

/*========== 静态函数原型 =====================================================*/

/*========== 静态函数实现 ====================================================*/

/*========== 外部函数实现 ====================================================*/

extern STD_RETURN_TYPE_e CRC_CalculateCrc(uint64_t *pCrc, uint8_t *pData, uint32_t lengthInBytes) {
    FAS_ASSERT(pCrc != NULL_PTR);   /* 断言：CRC结果指针不为空 */
    FAS_ASSERT(pData != NULL_PTR);  /* 断言：输入数据指针不为空 */

#ifndef UNITY_UNIT_TEST             /* 如果未定义单元测试宏 */
    static uint16_t crcCalls = 0u;  /* 静态局部变量，记录CRC计算函数被调用的次数，用于防止重入 */
#endif                              /* 结束 ifndef UNITY_UNIT_TEST */

    CRC_REGISTER_SIDE_e registerSide = CRC_REGISTER_LOW; /* 定义变量表示当前操作的是低32位还是高32位寄存器，初始化为低寄存器 */
    uint32_t dataBufferLow           = 0u;                /* 用于缓存低32位的数据 */
    uint32_t dataBufferHigh          = 0u;                /* 用于缓存高32位的数据 */
    uint32_t remainingBytes          = lengthInBytes;     /* 记录剩余未处理的字节数，初始化为总长度 */
    uint32_t remainingData           = 0u;                /* 用于缓存不足64位时的剩余数据 */
    STD_RETURN_TYPE_e retVal         = STD_OK;            /* 函数返回值，初始化为成功 */

    uint8_t *pRead = pData;                              /* 定义读写指针，初始化指向输入数据的首地址 */
    if (crcCalls == 0u) {                                 /* 如果CRC未被调用（防止重入） */
        crcCalls++;                                       /* 调用次数加1，标记正在计算 */

        /* 设置模式为数据捕获模式，否则写入种子会直接启动计算 */
        crcREG1->CTRL2 &= CRC_DATA_CAPTURE_MODE_CLEAR_MASK; /* 将CTRL2寄存器对应位清零，进入数据捕获模式 */
        /* 设置种子值 */
        crcREG1->PSA_SIGREGH1 = CRC_SEED_HIGH;              /* 写入高32位CRC种子值 */
        crcREG1->PSA_SIGREGL1 = CRC_SEED_LOW;               /* 写入低32位CRC种子值 */
        /* 设置模式为全CPU模式，以便在写入数据时启动计算 */
        crcREG1->CTRL2 |= CRC_FULL_CPU_MODE_SET_MASK;       /* 将CTRL2寄存器对应位置1，进入全CPU模式 */

        /* AXIVION Next Codeline Style MisraC2012-11.3: 需要64位访问，部分的32位访问会启动计算 */
        /* 指针用于访问两个签名寄存器，输入数据将被写入这些寄存器 */
        volatile uint64_t *pCrcRegister = (volatile uint64 *)(&crcREG1->PSA_SIGREGL1); /* 定义指向64位签名寄存器的指针 */

        /* 处理64位（8字节）的数据包 */
        while (remainingBytes >= CRC_REGISTER_SIZE_IN_BYTES) { /* 当剩余字节数大于等于8字节时循环 */
            /* 在进行64位写入之前，反转两个32位数据块，因为系统是大端模式 */
            if (registerSide == CRC_REGISTER_LOW) {            /* 如果当前处理的是低32位寄存器 */
                dataBufferLow = 0u;                            /* 清空低32位数据缓存 */
                for (uint8_t i = 0u; i < CRC_REGISTER_SIZE_IN_BYTES; i++) { /* 循环读取8个字节 */
                    uint8_t dataBuffer = *(pRead + i);         /* 读取当前字节 */
                    dataBufferLow |= ((uint32_t)dataBuffer) << ((CRC_REVERSE_BYTES_ORDER - i) * CRC_BYTE_SIZE_IN_BITS); /* 将字节按反转顺序移位并拼接到低32位缓存中 */
                }
                registerSide = CRC_REGISTER_HIGH;              /* 切换到处理高32位寄存器 */
            } else {                                          /* registerSide 是 CRC_REGISTER_HIGH（当前处理高32位寄存器） */
                dataBufferHigh = 0u;                           /* 清空高32位数据缓存 */
                for (uint8_t i = 0u; i < CRC_REGISTER_SIZE_IN_BYTES; i++) { /* 循环读取8个字节 */
                    uint8_t dataBuffer = *(pRead + i);         /* 读取当前字节 */
                    dataBufferHigh |= ((uint32_t)dataBuffer) << ((CRC_REVERSE_BYTES_ORDER - i) * CRC_BYTE_SIZE_IN_BITS); /* 将字节按反转顺序移位并拼接到高32位缓存中 */
                }
                /* 低32位和高32位签名数据都已准备好，写入硬件寄存器 */
                uint64_t crcData = (((uint64_t)dataBufferHigh) << CRC_REGISTER_SIZE_IN_BITS) | dataBufferLow; /* 将高32位和低32位合并为64位数据 */
                *pCrcRegister    = crcData;                    /* 将64位数据写入CRC寄存器，硬件自动开始计算 */
                registerSide     = CRC_REGISTER_LOW;           /* 重置为处理低32位寄存器，为下一组64位数据做准备 */
            }
            pRead = (pRead + CRC_REGISTER_SIZE_IN_BYTES);      /* 读取指针向后移动8个字节 */
            remainingBytes -= CRC_REGISTER_SIZE_IN_BYTES;      /* 剩余字节数减去8 */
        }

        if (remainingBytes > 0u) {                             /* 如果还有不足8个字节的剩余数据 */
            /* 现在处理存在的最后不足32位的数据包 */
            /* 将数据获取到32位变量中，用0填充剩余位 */
            while (remainingBytes > 0u) {                      /* 循环处理每个剩余字节 */
                uint8_t dataBuffer = *pRead;                   /* 读取当前字节 */
                remainingData |= ((uint32_t)(dataBuffer)) << (CRC_BYTE_SIZE_IN_BITS * remainingBytes); /* 将剩余字节按顺序移位拼接，注意这里的移位方式 */
                pRead++;                                       /* 读取指针向后移动1个字节 */
                remainingBytes--;                              /* 剩余字节数减1 */
            }
            if (registerSide == CRC_REGISTER_LOW) {            /* 如果当前处理的是低32位寄存器 */
                dataBufferLow = remainingData;                 /* 将剩余数据放入低32位缓存 */
                registerSide  = CRC_REGISTER_HIGH;             /* 切换到处理高32位寄存器 */
            } else {                                          /* registerSide 是 CRC_REGISTER_HIGH（当前处理高32位寄存器） */
                dataBufferHigh = remainingData;                /* 将剩余数据放入高32位缓存 */
                /* 低32位和高32位签名数据都已准备好，写入硬件寄存器 */
                uint64_t crcData = (((uint64_t)dataBufferHigh) << CRC_REGISTER_SIZE_IN_BITS) | dataBufferLow; /* 将高32位和低32位合并为64位数据 */
                *pCrcRegister    = crcData;                    /* 将64位数据写入CRC寄存器 */
                registerSide     = CRC_REGISTER_LOW;           /* 重置为处理低32位寄存器 */
            }
        }

        /* 没有剩余数据，但只有低32位寄存器数据可用时：计算CRC */
        if (registerSide == CRC_REGISTER_HIGH) {               /* 如果高32位寄存器处于等待状态（说明只有低32位有数据，高32位没凑满64位） */
            crcREG1->PSA_SIGREGL1 = dataBufferLow;             /* 直接将低32位数据写入低签名寄存器，触发硬件计算 */
        }

        *pCrc = crcREG1->PSA_SIGREGL1;                        /* 读取CRC计算结果的低32位 */
        *pCrc |= ((uint64_t)crcREG1->PSA_SIGREGH1) << CRC_REGISTER_SIZE_IN_BITS; /* 读取高32位结果并移位后与低32位合并，组成最终64位CRC结果 */
        crcCalls--;                                            /* 调用次数减1，释放计算锁 */
    } else {                                                   /* 如果CRC正在被调用（发生重入） */
        *pCrc  = 0u;                                           /* 将结果置为0 */
        retVal = STD_NOT_OK;                                   /* 返回状态设为不成功 */
    }

    return retVal;                                             /* 返回函数执行结果 */
}

/*========== 外部化的静态函数实现（单元测试）================================*/
#ifdef UNITY_UNIT_TEST         /* 如果定义了UNITY_UNIT_TEST宏 */

#endif                        /* 结束 #ifdef UNITY_UNIT_TEST */
