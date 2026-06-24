/**
 *
 * @copyright &copy; 2010 - 2026, Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 * 保留所有权利。
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * 在满足以下条件的前提下，允许以源代码和二进制形式进行重新分发和使用，
 * 无论是否经过修改：
 *
 * 1. 源代码的重新分发必须保留上述版权声明、此条件列表以及以下免责声明。
 *
 * 2. 二进制形式的重新分发必须在随分发提供的文档和/或其他材料中
 * 复制上述版权声明、此条件列表以及以下免责声明。
 *
 * 3. 未经特定的事先书面许可，版权持有者的名称及其贡献者的名称
 * 均不得用于认可或推广基于本软件派生的产品。
 *
 * 本软件由版权持有者及贡献者“按原样”提供，不提供任何明示或暗示的保证，
 * 包括但不限于对适销性和特定用途适用性的暗示保证。在任何情况下，
 * 版权持有者或贡献者均不对任何直接、间接、偶然、特殊、惩罚性或后果性
 * 损害（包括但不限于替代商品或服务的采购、使用、数据或利润的损失，
 * 或业务中断）负责，无论该损害是如何引起的，也无论基于何种责任理论，
 * 无论是合同责任、严格责任还是侵权（包括疏忽或其他），即使已被告知
 * 可能发生此类损害。
 *
 * 我们恳请您在您的硬件、软件、文档或广告材料中使用以下一个或多个短语
 * 来指代 foxBMS：
 *
 * - "本产品使用了 foxBMS&reg; 的部分内容"
 * - "本产品包含了 foxBMS&reg; 的部分内容"
 * - "本产品派生自 foxBMS&reg;"
 *
 */

/**
 * @file    general.h
 * @author  foxBMS 团队
 * @date    2019-09-24 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup MAIN_CONFIGURATION
 * @prefix  GEN
 *
 * @brief   针对整个平台的通用宏和定义。
 * @details 待办事项
 */

#ifndef FOXBMS__GENERAL_H_
#define FOXBMS__GENERAL_H_

/*========== 头文件包含 =====================================================*/
#include "fassert.h"
#include "fstd_types.h"

#include <stdbool.h>
#include <stdint.h>

/*========== 宏和定义 =======================================================*/

/**
 * @brief   将某一位置为 1u
 * @param[in,out]   register    需要设置位的寄存器
 * @param[in]       bit         需要设置为 1u 的位号
 */
#define GEN_SET_BIT(register, bit) ((register) |= (uint32)((uint32)1u << (bit)))
/**
 * @brief   将某一位清零为 0u
 * @param[in,out]   register    需要清除位的寄存器
 * @param[in]       bit         需要清零为 0u 的位号
 */
#define GEN_CLEAR_BIT(register, bit) ((register) &= ~(uint32)((uint32)1u << (bit)))

/**
 * @brief 允许函数在 GCC 中针对未使用的返回值生成警告。
 *
 * 此属性允许标记函数的返回值必须被使用。
 * 当后续代码中未使用带有此标记的函数的返回值时，编译器将生成警告。
 */
#define GEN_MUST_CHECK_RETURN __attribute__((warn_unused_result))

/* AXIVION 下一行代码风格 MisraC2012-1.2: 出于性能原因，有时必须进行函数内联 */
/** 此属性告诉编译器应始终内联该函数 */
#define GEN_ALWAYS_INLINE __attribute__((always_inline))

/* 断言 fstd_types.h 中的基本数据类型完好无损 */
/* AXIVION 禁用风格 MisraC2012-10.4: 这些断言必须检查枚举和宏定义的实际值。 */
FAS_STATIC_ASSERT(false == 0, "false 似乎已被修改。");
FAS_STATIC_ASSERT(true != false, "true 似乎已被修改。");
FAS_STATIC_ASSERT(true == 1, "true 似乎已被修改。");

FAS_STATIC_ASSERT(STD_OK == 0, "STD_OK 似乎已被修改。");
FAS_STATIC_ASSERT(STD_OK != STD_NOT_OK, "STD_OK 或 STD_NOT_OK 似乎已被修改。");
FAS_STATIC_ASSERT(STD_NOT_OK == 1, "STD_NOT_OK 似乎已被修改。");
/* AXIVION 启用风格 MisraC2012-10.4: */

/**
 * 用于实现 #GEN_REPEAT_U() 的内部宏。请勿在外部使用。
 * @{
 */
/* AXIVION 禁用风格 Generic-NoUnsafeMacro MisraC2012Directive-4.9: 由于这些宏的特性，
   不可能将 REPEAT_Uxu(x) 标记用括号括起来。通过这些类似函数的宏实现了重复标记的功能。 */
/* 使用小写的 'u' 后缀来强调 'GEN_REPEAT_U*u' 宏的无符号特性。 */
#define GEN_REPEAT_U1u(x)  (x)
#define GEN_REPEAT_U2u(x)  GEN_REPEAT_U1u(x), (x)
#define GEN_REPEAT_U3u(x)  GEN_REPEAT_U2u(x), (x)
#define GEN_REPEAT_U4u(x)  GEN_REPEAT_U3u(x), (x)
#define GEN_REPEAT_U5u(x)  GEN_REPEAT_U4u(x), (x)
#define GEN_REPEAT_U6u(x)  GEN_REPEAT_U5u(x), (x)
#define GEN_REPEAT_U7u(x)  GEN_REPEAT_U6u(x), (x)
#define GEN_REPEAT_U8u(x)  GEN_REPEAT_U7u(x), (x)
#define GEN_REPEAT_U9u(x)  GEN_REPEAT_U8u(x), (x)
#define GEN_REPEAT_U10u(x) GEN_REPEAT_U9u(x), (x)
#define GEN_REPEAT_U11u(x) GEN_REPEAT_U10u(x), (x)
#define GEN_REPEAT_U12u(x) GEN_REPEAT_U11u(x), (x)
#define GEN_REPEAT_U13u(x) GEN_REPEAT_U12u(x), (x)
#define GEN_REPEAT_U14u(x) GEN_REPEAT_U13u(x), (x)
#define GEN_REPEAT_U15u(x) GEN_REPEAT_U14u(x), (x)
#define GEN_REPEAT_U16u(x) GEN_REPEAT_U15u(x), (x)
#define GEN_REPEAT_U17u(x) GEN_REPEAT_U16u(x), (x)
#define GEN_REPEAT_U18u(x) GEN_REPEAT_U17u(x), (x)
#define GEN_REPEAT_U19u(x) GEN_REPEAT_U18u(x), (x)
#define GEN_REPEAT_U20u(x) GEN_REPEAT_U19u(x), (x)
#define GEN_REPEAT_U21u(x) GEN_REPEAT_U20u(x), (x)
#define GEN_REPEAT_U22u(x) GEN_REPEAT_U21u(x), (x)
#define GEN_REPEAT_U23u(x) GEN_REPEAT_U22u(x), (x)
#define GEN_REPEAT_U24u(x) GEN_REPEAT_U23u(x), (x)
#define GEN_REPEAT_U25u(x) GEN_REPEAT_U24u(x), (x)
#define GEN_REPEAT_U26u(x) GEN_REPEAT_U25u(x), (x)
#define GEN_REPEAT_U27u(x) GEN_REPEAT_U26u(x), (x)
#define GEN_REPEAT_U28u(x) GEN_REPEAT_U27u(x), (x)
#define GEN_REPEAT_U29u(x) GEN_REPEAT_U28u(x), (x)
#define GEN_REPEAT_U30u(x) GEN_REPEAT_U29u(x), (x)
#define GEN_REPEAT_U31u(x) GEN_REPEAT_U30u(x), (x)
#define GEN_REPEAT_U32u(x) GEN_REPEAT_U31u(x), (x)
#define GEN_REPEAT_U33u(x) GEN_REPEAT_U32u(x), (x)
#define GEN_REPEAT_U34u(x) GEN_REPEAT_U33u(x), (x)
#define GEN_REPEAT_U35u(x) GEN_REPEAT_U34u(x), (x)
#define GEN_REPEAT_U36u(x) GEN_REPEAT_U35u(x), (x)
#define GEN_REPEAT_U37u(x) GEN_REPEAT_U36u(x), (x)
#define GEN_REPEAT_U38u(x) GEN_REPEAT_U37u(x), (x)
#define GEN_REPEAT_U39u(x) GEN_REPEAT_U38u(x), (x)
#define GEN_REPEAT_U40u(x) GEN_REPEAT_U39u(x), (x)
#define GEN_REPEAT_U41u(x) GEN_REPEAT_U40u(x), (x)
#define GEN_REPEAT_U42u(x) GEN_REPEAT_U41u(x), (x)
#define GEN_REPEAT_U43u(x) GEN_REPEAT_U42u(x), (x)
#define GEN_REPEAT_U44u(x) GEN_REPEAT_U43u(x), (x)
#define GEN_REPEAT_U45u(x) GEN_REPEAT_U44u(x), (x)
#define GEN_REPEAT_U46u(x) GEN_REPEAT_U45u(x), (x)
#define GEN_REPEAT_U47u(x) GEN_REPEAT_U46u(x), (x)
#define GEN_REPEAT_U48u(x) GEN_REPEAT_U47u(x), (x)
#define GEN_REPEAT_U49u(x) GEN_REPEAT_U48u(x), (x)
#define GEN_REPEAT_U50u(x) GEN_REPEAT_U49u(x), (x)
#define GEN_REPEAT_U51u(x) GEN_REPEAT_U50u(x), (x)
#define GEN_REPEAT_U52u(x) GEN_REPEAT_U51u(x), (x)
#define GEN_REPEAT_U53u(x) GEN_REPEAT_U52u(x), (x)
#define GEN_REPEAT_U54u(x) GEN_REPEAT_U53u(x), (x)
#define GEN_REPEAT_U55u(x) GEN_REPEAT_U54u(x), (x)
#define GEN_REPEAT_U56u(x) GEN_REPEAT_U55u(x), (x)
#define GEN_REPEAT_U57u(x) GEN_REPEAT_U56u(x), (x)
#define GEN_REPEAT_U58u(x) GEN_REPEAT_U57u(x), (x)
#define GEN_REPEAT_U59u(x) GEN_REPEAT_U58u(x), (x)
#define GEN_REPEAT_U60u(x) GEN_REPEAT_U59u(x), (x)
#define GEN_REPEAT_U61u(x) GEN_REPEAT_U60u(x), (x)
#define GEN_REPEAT_U62u(x) GEN_REPEAT_U61u(x), (x)
#define GEN_REPEAT_U63u(x) GEN_REPEAT_U62u(x), (x)
#define GEN_REPEAT_U64u(x) GEN_REPEAT_U63u(x), (x)
#define GEN_REPEAT_U65u(x) GEN_REPEAT_U64u(x), (x)
#define GEN_REPEAT_U66u(x) GEN_REPEAT_U65u(x), (x)
#define GEN_REPEAT_U67u(x) GEN_REPEAT_U66u(x), (x)
#define GEN_REPEAT_U68u(x) GEN_REPEAT_U67u(x), (x)
#define GEN_REPEAT_U69u(x) GEN_REPEAT_U68u(x), (x)
#define GEN_REPEAT_U70u(x) GEN_REPEAT_U69u(x), (x)
#define GEN_REPEAT_U71u(x) GEN_REPEAT_U70u(x), (x)
#define GEN_REPEAT_U72u(x) GEN_REPEAT_U71u(x), (x)
#define GEN_REPEAT_U73u(x) GEN_REPEAT_U72u(x), (x)
#define GEN_REPEAT_U74u(x) GEN_REPEAT_U73u(x), (x)
#define GEN_REPEAT_U75u(x) GEN_REPEAT_U74u(x), (x)
#define GEN_REPEAT_U76u(x) GEN_REPEAT_U75u(x), (x)
#define GEN_REPEAT_U77u(x) GEN_REPEAT_U76u(x), (x)
#define GEN_REPEAT_U78u(x) GEN_REPEAT_U77u(x), (x)
#define GEN_REPEAT_U79u(x) GEN_REPEAT_U78u(x), (x)
#define GEN_REPEAT_U80u(x) GEN_REPEAT_U79u(x), (x)
#define GEN_REPEAT_U81u(x) GEN_REPEAT_U80u(x), (x)
#define GEN_REPEAT_U82u(x) GEN_REPEAT_U81u(x), (x)
#define GEN_REPEAT_U83u(x) GEN_REPEAT_U82u(x), (x)
#define GEN_REPEAT_U84u(x) GEN_REPEAT_U83u(x), (x)
#define GEN_REPEAT_U85u(x) GEN_REPEAT_U84u(x), (x)
#define GEN_REPEAT_U86u(x) GEN_REPEAT_U85u(x), (x)
#define GEN_REPEAT_U87u(x) GEN_REPEAT_U86u(x), (x)
#define GEN_REPEAT_U88u(x) GEN_REPEAT_U87u(x), (x)
#define GEN_REPEAT_U89u(x) GEN_REPEAT_U88u(x), (x)
#define GEN_REPEAT_U90u(x) GEN_REPEAT_U89u(x), (x)
#define GEN_REPEAT_U91u(x) GEN_REPEAT_U90u(x), (x)
#define GEN_REPEAT_U92u(x) GEN_REPEAT_U91u(x), (x)
#define GEN_REPEAT_U93u(x) GEN_REPEAT_U92u(x), (x)
#define GEN_REPEAT_U94u(x) GEN_REPEAT_U93u(x), (x)
#define GEN_REPEAT_U95u(x) GEN_REPEAT_U94u(x), (x)
#define GEN_REPEAT_U96u(x) GEN_REPEAT_U95u(x), (x)
#define GEN_REPEAT_U97u(x) GEN_REPEAT_U96u(x), (x)
#define GEN_REPEAT_U98u(x) GEN_REPEAT_U97u(x), (x)
#define GEN_REPEAT_U99u(x) GEN_REPEAT_U98u(x), (x)

/* AXIVION 禁用风格 MisraC2012-20.10: 只要遵守文档中的说明，即允许使用。 */
#define GEN_REPEAT_Ux(x, n) GEN_REPEAT_U##n(x)
/* AXIVION 启用风格 Generic-NoUnsafeMacro MisraC2012Directive-4.9 MisraC2012-20.10: */
/**@}*/

/** #GEN_REPEAT_U() 中支持的最大重复次数。如果更改实现，请相应调整。*/
#define GEN_REPEAT_MAXIMUM_REPETITIONS (99u)

/**
 * @brief   帮助生成一系列字面量的宏（用于数组初始化器）。
 * @details 此宏为数组初始化器生成一系列字面量。
 *          当数组的大小由宏定义且需要将其初始化为非零值时，
 *          可以使用此宏。如果要将数组初始化为零，应使用标准的 {0}。
 *
 * @param   x   需要重复的标记，例如 true
 * @param   n   重复的次数（通过 #GEN_STRIP() 去除括号，并描述为无符号整数字面量）
 *              （最多 16 次，即 #GEN_REPEAT_MAXIMUM_REPETITIONS 次重复）
 *
 *          用法示例：
  \verbatim
  #define ARRAY_SIZE (4u)
  bool variable[ARRAY_SIZE] = {GEN_REPEAT_U(false, GEN_STRIP(ARRAY_SIZE))};
  \endverbatim
 *          这将展开为：
  \verbatim
  bool variable[ARRAY_SIZE] = {false, false, false, false};
  \endverbatim
 */
/* AXIVION 下一行代码风格 MisraC2012Directive-4.9: 此功能需要类似函数的宏。 */
#define GEN_REPEAT_U(x, n) GEN_REPEAT_Ux(x, n)

/** #GEN_STRIP() 的内部辅助宏。请勿在外部使用。
 * @{
 */
/* AXIVION 禁用风格 MisraC2012Directive-4.9 MisraC2012-20.7 Generic-NoUnsafeMacro CertC-PRE01: 
   此功能需要类似函数的宏。此处旨在去除括号。 */
#define GEN_GET_ARGS(...)   __VA_ARGS__
#define GEN_STRIP_PARENS(x) x
/**@}*/
/** 去除标记周围的外层括号。 */
#define GEN_STRIP(x) GEN_STRIP_PARENS(GEN_GET_ARGS x)
/* AXIVION 启用风格 MisraC2012Directive-4.9 MisraC2012-20.7 Generic-NoUnsafeMacro: */

/** 定义平台的字长（以字节为单位） */
#if defined(__TI_COMPILER_VERSION__) && defined(__ARM_32BIT_STATE) && defined(__TMS470__)
#define GEN_BYTES_PER_WORD (4u)
#elif defined(UNITY_UNIT_TEST)
/* 由于此定义仅影响任务大小，在单元测试中可以安全地将其设置为
   嵌入式平台上使用的值 */
#define GEN_BYTES_PER_WORD (4u)
#elif defined(DOCUMENTATION)
#define GEN_BYTES_PER_WORD (4u)
#else
#warning "未指定的平台，默认为每字 4 字节。"
#define GEN_BYTES_PER_WORD (4u)
#endif

/*========== 外部常量和变量声明 ============================================*/

/*========== 外部函数原型 ===================================================*/

/*========== 外部化的静态函数原型（单元测试） ===============================*/
#ifdef UNITY_UNIT_TEST
#endif

#endif /* FOXBMS__GENERAL_H_ */
