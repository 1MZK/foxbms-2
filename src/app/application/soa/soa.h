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
 * @file    soa.h
 * @author  foxBMS 团队
 * @date    2020-10-14 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup APPLICATION
 * @prefix  SOA
 *
 * @brief   SOA 模块的头文件，负责根据安全限制检查电池参数
 * @details TODO
 */

#ifndef FOXBMS__SOA_H_
#define FOXBMS__SOA_H_

/*========== 包含文件 =======================================================*/
#include "soa_cfg.h"

#include "database.h"

#include <stdint.h>

/*========== 宏和定义 =========================================================*/

/*========== 外部常量和变量声明 ======================*/

/*========== 外部函数原型 =====================================*/

/**
 * @brief   检查是否遵守安全运行区域
 * @param[in]   pMinimumMaximumCellVoltages  指向包含最小和最大单体电压的数据库条目的指针
 * @details 验证单体电压测量值 (U)，检查最小值和最大值是否超出范围
 */
extern void SOA_CheckVoltages(DATA_BLOCK_MIN_MAX_s *pMinimumMaximumCellVoltages);

/**
 * @brief   检查是否遵守安全运行区域
 * @param[in]   pMinimumMaximumCellTemperatures  指向包含最小和最大单体温度的数据库条目的指针
 * @param[in]   pCurrent                         指向电池包值数据库条目的指针
 * @details 验证单体温度测量值 (T)，检查最小值和最大值是否超出范围
 */
extern void SOA_CheckTemperatures(
    DATA_BLOCK_MIN_MAX_s *pMinimumMaximumCellTemperatures,
    DATA_BLOCK_PACK_VALUES_s *pCurrent);

/**
 * @brief   检查是否遵守安全运行区域
 * @param[in]   pTablePackValues   指向电池包值数据库条目的指针
 * @details 验证单体电流测量值 (I)，检查最小值和最大值是否超出范围
 */
extern void SOA_CheckCurrent(DATA_BLOCK_PACK_VALUES_s *pTablePackValues);

/**
 * @brief   用于未来兼容；虚拟函数；请勿使用
 * @details 用于未来兼容；虚拟函数；请勿使用
 */
extern void SOA_CheckSlaveTemperatures(void);

/*========== 外部化的静态函数原型 (单元测试) ===========*/
#ifdef UNITY_UNIT_TEST
#endif

#endif /* FOXBMS__SOA_H_ */

