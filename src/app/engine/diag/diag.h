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
 * @file    diag.h
 * @author  foxBMS 团队
 * @date    2019-11-28 (创建日期)
 * @updated 2026-04-20 (最后更新日期)
 * @version v1.11.0
 * @ingroup ENGINE
 * @prefix  DIAG
 *
 * @brief   诊断驱动头文件
 * @details TODO
 */

#ifndef FOXBMS__DIAG_H_
#define FOXBMS__DIAG_H_

/*========== 包含文件 =======================================================*/
#include "diag_cfg.h"

#include "fstd_types.h"

#include <stdint.h>

/*========== 宏和定义 =========================================================*/

/** 诊断处理程序返回类型 */
typedef enum {
    DIAG_HANDLER_RETURN_OK,               /*!<  错误未发生，或已发生但未达到阈值 */
    DIAG_HANDLER_RETURN_ERR_OCCURRED,     /*!<  错误已发生且已启用 */
    DIAG_HANDLER_RETURN_WARNING_OCCURRED, /*!<  警告已发生（错误已发生但未启用） */
    DIAG_HANDLER_RETURN_WRONG_ID,         /*!<  错误的诊断 ID */
    DIAG_HANDLER_RETURN_UNKNOWN,          /*!<  未知的返回类型 */
    DIAG_HANDLER_INVALID_TYPE,            /*!<  无效的诊断类型，配置有误 */
    DIAG_HANDLER_INVALID_DATA,            /*!<  无效的数据，依赖于 diagHandler */
    DIAG_HANDLER_INVALID_ERR_IMPACT,      /*!<  事件既不是串级别也不是系统级别相关 */
    DIAG_HANDLER_RETURN_NOT_READY,        /*!<  诊断处理程序未就绪 */
} DIAG_RETURNTYPE_e;

/** 诊断模块的可能状态 */
typedef enum {
    DIAG_STATE_UNINITIALIZED, /*!< 诊断模块未初始化 */
    DIAG_STATE_INITIALIZED,   /*!< 诊断模块已初始化（可供使用） */
} DIAG_MODULE_STATE_e;

/** 诊断模块的中央状态结构体 */
typedef struct {
    DIAG_MODULE_STATE_e state;                                 /*!< 诊断模块的实际状态 */
    uint16_t totalErrorCount;                                  /*!< 诊断条目记录的总计数 */
    uint16_t reportedErrorCount;                               /*!< 向外部工具报告的错误计数 */
    uint32_t entry_event[DIAG_ID_MAX];                         /*!< 最后检测到的条目事件 */
    uint8_t entry_cnt[DIAG_ID_MAX];                            /*!< 用于限制的报告事件计数器 */
    uint16_t occurrenceCounter[BS_NR_OF_STRINGS][DIAG_ID_MAX]; /*!< 诊断事件发生的计数器 */
    uint8_t id2ch[DIAG_ID_MAX];                                /*!< 诊断 ID 到配置通道的选择器 */
    uint8_t nrOfConfiguredDiagnosisEntries;                    /*!< 已配置的诊断条目数量 */
    uint32_t errflag[(DIAG_ID_MAX + 31) / 32];                 /*!< 检测到的错误标志 (位号 = diag_id) */
    uint32_t warnflag[(DIAG_ID_MAX + 31) / 32];                /*!< 检测到的警告标志 (位号 = diag_id) */
    uint32_t err_enableflag[(DIAG_ID_MAX + 31) / 32];          /*!< 已启用的错误标志 (位号 = diag_id) */
} DIAG_DIAGNOSIS_STATE_s;

/*========== 外部常量和变量声明 ======================*/

/*========== 外部函数原型 =====================================*/

/**
 * @brief   DIAG_Handler 提供基于诊断组的通用错误处理。
 * @details 此函数根据调用的诊断组调用处理函数。它需要在每个需要应用
 *          某种诊断处理的函数中被调用。根据其返回值，进一步的处理可以
 *          留给调用模块本身，也可以在 diag_cfg.c 中定义的回调函数中完成。
 * @param   diagId 已发生事件的 #DIAG_ID_e
 * @param   event   发生的事件 (OK, NOK, RESET)
 * @param   impact  #DIAG_ID_e 的 #DIAG_IMPACT_LEVEL_e
 * @param   data    #DIAG_ID_e 的单独信息，例如串号等
 * @return  如果是无效的 #DIAG_EVENT_e 则返回 #DIAG_HANDLER_RETURN_UNKNOWN，
 *          否则返回 #DIAG_RETURNTYPE_e 的返回值
 */
extern DIAG_RETURNTYPE_e DIAG_Handler(DIAG_ID_e diagId, DIAG_EVENT_e event, DIAG_IMPACT_LEVEL_e impact, uint32_t data);

/**
 * @brief   DIAG_CheckEvent 提供了一个简单的接口来检查 #STD_OK 的事件
 * @details DIAG_CheckEvent 是 #DIAG_Handler() 的封装函数。在简单情况下，
 *          当一个非 #STD_OK（或被强制转换为 #STD_OK 的 0）的返回值需要
 *          增加诊断通道中的错误计数器时，应使用此函数而不是直接调用 #DIAG_Handler()。
 * @param   cond    条件
 * @param   diagId  已发生事件的事件 ID
 * @param   impact  #DIAG_ID_e 的 #DIAG_IMPACT_LEVEL_e
 * @param   data    #DIAG_ID_e 的单独信息，例如串号等
 * @return  如果正常则返回 STD_OK，如果不正常则返回 STD_NOT_OK
 */
extern STD_RETURN_TYPE_e DIAG_CheckEvent(
    STD_RETURN_TYPE_e cond,
    DIAG_ID_e diagId,
    DIAG_IMPACT_LEVEL_e impact,
    uint32_t data);

/**
 * @brief   DIAG_Init 初始化所有需要的结构/缓冲区。
 * @details 此函数提供诊断模块的初始化。如果发生异常行为，它将调用复位，
 *          并在数据库中添加条目以确保数据有效性/报告故障
 * @param   diag_dev_pointer 诊断设备指针
 * @return  如果正常则返回 #STD_OK，如果不正常则返回 #STD_NOT_OK
 */
extern STD_RETURN_TYPE_e DIAG_Initialize(DIAG_DEV_s *diag_dev_pointer);

/**
 * @brief   检查传递的诊断条目是否已被触发
 * @param   diagnosisEntry 诊断条目的事件 ID
 * @return  如果诊断条目未超过错误阈值则返回 #STD_OK，否则返回 #STD_NOT_OK
 */
extern STD_RETURN_TYPE_e DIAG_GetDiagnosisEntryState(DIAG_ID_e diagnosisEntry);

/**
 * @brief   根据用户请求打印错误缓冲区的内容。
 */
extern void DIAG_PrintErrors(void);

/**
 * @brief   获取传递的诊断条目的配置延迟
 * @param   diagnosisEntry 诊断条目的事件 ID
 * @return  配置的延迟时间（毫秒）
 */
extern uint32_t DIAG_GetDelay(DIAG_ID_e diagnosisEntry);

/**
 * @brief   检查是否设置了任何致命错误
 * @return  如果设置了严重级别为 #DIAG_FATAL_ERROR 的诊断条目则返回 true，
 *          否则返回 false
 */
extern bool DIAG_IsAnyFatalErrorSet(void);

/*========== 外部化的静态函数原型 (单元测试) ===========*/
#ifdef UNITY_UNIT_TEST
extern void TEST_DIAG_SetDiagTotalErrorCount(uint16_t errors);
extern DIAG_DIAGNOSIS_STATE_s *TEST_DIAG_GetDiag(void);
extern void TEST_DIAG_Reset(void);
extern uint8_t TEST_DIAG_GetFatalErrorCount(void);
extern void TEST_DIAG_SetDiagOccurrenceCounter(uint16_t errors);
extern void TEST_DIAG_ClearFatalErrorById(DIAG_ID_e xEventID);
extern void TEST_DIAG_SetFatalErrorById(DIAG_ID_e xEventID);
extern uint8_t TEST_DIAG_GetFatalErrorArrayCount(DIAG_ID_e xEventID);
extern void TEST_DIAG_ResendFatalErrors(void);
extern void TEST_DIAG_SetActiveFatalErrorCounter(uint16_t errors);
extern void TEST_DIAG_SetActiveFatalErrorArray(uint16_t errors);
#endif

#endif /* FOXBMS__DIAG_H_ */

