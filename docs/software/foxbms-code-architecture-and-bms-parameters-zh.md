# foxBMS 2 代码框架、运行逻辑与 BMS 保护参数说明

本文档面向初次阅读 `foxBMS 2` 工程的开发者，整理代码的整体搭建方式、主要运行逻辑，以及 BMS 保护板相关参数主要配置位置。

## 1. 工程整体框架

`foxBMS 2` 是一个面向电池管理系统的嵌入式工程，整体不是单文件程序，而是采用“配置 + 驱动 + 引擎 + 应用状态机”的分层结构。

仓库顶层目录含义如下：

| 路径 | 作用 |
| --- | --- |
| `cli/` | foxBMS 命令行工具，用于构建、测试、辅助操作 |
| `conf/` | 高层配置文件，例如 BMS 模块选择、编译器配置、HALCoGen 配置 |
| `docs/` | Sphinx/Doxygen 文档源码 |
| `hardware/` | 硬件资料、主板/从板/接口板相关说明 |
| `src/` | 嵌入式软件源码 |
| `tests/` | 单元测试、工具测试、构建测试 |
| `tools/` | 构建工具、DBC、CRC、调试工具等 |

嵌入式软件主目录是 `src/`，其中最重要的是：

| 路径 | 作用 |
| --- | --- |
| `src/app/` | BMS 应用层源码 |
| `src/bootloader/` | bootloader 源码 |
| `src/portable/` | 与处理器相关的移植代码 |
| `src/os/` | FreeRTOS 等操作系统源码 |
| `src/opt/` | 可选源码，例如特定电芯参数 |

`src/app/` 又分成几个层次：

| 路径 | 作用 |
| --- | --- |
| `src/app/main/` | 程序入口、启动流程、链接脚本 |
| `src/app/task/` | FreeRTOS 任务封装和周期任务配置 |
| `src/app/hal/` | HAL 相关代码 |
| `src/app/driver/` | 底层驱动层，例如 CAN、AFE、SPI、ADC、接触器、绝缘检测等 |
| `src/app/engine/` | 中间引擎层，例如数据库、诊断、系统监控、系统状态 |
| `src/app/application/` | BMS 业务应用层，例如 BMS 状态机、SOA、均衡、SOC/SOE/SOF |

可以把软件理解为以下结构：

```text
main.c
  -> 初始化硬件和基础驱动
  -> 初始化诊断、自检、操作系统
  -> 启动 FreeRTOS
      -> 周期任务 ftask_cfg.c
          -> 1ms task: 时间基准、诊断标志、测量控制、CAN 接收
          -> 10ms task: SYS、ADC、CAN、SOF、SBC、BMS_Trigger()
          -> 100ms task: SOC/SOE、均衡、绝缘检测、LED
          -> Engine task: DATA 数据库、SYSM 系统监控
              -> BMS_Trigger()
                  -> 读取测量值
                  -> SOA 安全检查
                  -> 接触器反馈检查
                  -> BMS 状态机跳转
```

## 2. 构建与高层配置

工程使用 Waf 构建系统，入口文件是：

```text
wscript
```

高层 BMS 配置文件是：

```text
conf/bms/bms.json
```

该 JSON 文件决定编译时选择哪些模块，例如：

```json
{
  "application": {
    "algorithm": {
      "state-estimation": {
        "soc": "counting",
        "soe": "counting",
        "sof": "trapezoid",
        "soh": "none"
      }
    },
    "balancing-strategy": "voltage",
    "current-sensor": {
      "type": "can",
      "manufacturer": "isabellenhuette",
      "model": "ivt-s"
    },
    "insulation-monitoring-device": "none"
  },
  "rtos": {
    "name": "freertos",
    "addons": []
  },
  "bms-slave": {
    "analog-front-end": {
      "ic": "6813-1",
      "manufacturer": "ltc"
    },
    "temperature-sensor": {
      "manufacturer": "epcos",
      "method": "polynomial",
      "model": "b57251v5103j060"
    }
  }
}
```

它主要用于选择：

- SOC/SOE/SOF/SOH 算法；
- 均衡策略；
- 电流传感器；
- 绝缘监测设备；
- RTOS；
- BMS 从板 AFE 芯片；
- 温度传感器型号。

注意：`conf/bms/bms.json` 主要决定“编译哪些模块”，并不是所有保护阈值都在这里。电压、电流、温度、预充、接触器等具体保护参数主要在 `src/app/application/config/` 和 `src/app/driver/config/` 中。

## 3. 程序启动流程

程序入口：

```text
src/app/main/main.c
```

核心函数：

```c
int main(void)
```

启动流程如下：

1. 获取复位源。
2. 初始化 MCU 外设和基础驱动。
3. 初始化 SPI、ADC、I2C、DMA、PWM、UART 等。
4. 初始化诊断模块。
5. 执行启动自检。
6. 初始化 FreeRTOS。
7. 使能 IRQ 中断。
8. 启动 FreeRTOS 调度器。

关键调用包括：

```c
MINFO_SetResetSource(getResetSource());
muxInit();
gioInit();
SPI_Initialize();
adcInit();
I2C_Initialize();
DMA_Initialize();
PWM_Initialize();
DIAG_Initialize(&diag_device);
MATH_StartupSelfTest();
OS_InitializeOperatingSystem();
OS_StartScheduler();
```

`main.c` 的作用可以理解为“把硬件和操作系统启动起来”。真正的 BMS 控制逻辑不在 `main.c` 中，而是在 FreeRTOS 任务中周期运行。

## 4. 周期任务运行逻辑

周期任务配置文件：

```text
src/app/task/config/ftask_cfg.c
```

### 4.1 Engine task

相关函数：

```c
FTSK_InitializeUserCodeEngine()
FTSK_RunUserCodeEngine()
```

主要作用：

- 初始化 `DATA` 数据库；
- 初始化 FRAM；
- 初始化系统监控；
- 周期调用数据库管理；
- 检查任务通知和系统监控状态。

关键逻辑：

```c
DATA_Initialize();
FRAM_Initialize();
SYSM_Initialize();
DATA_Task();
SYSM_CheckNotifications();
```

### 4.2 Pre-cyclic 初始化

相关函数：

```c
FTSK_InitializeUserCodePreCyclicTasks()
```

主要作用：

- 初始化系统状态；
- 初始化端口扩展器；
- 初始化接触器；
- 初始化电源开关；
- 初始化测量模块；
- 初始化冗余模块。

关键逻辑：

```c
SYS_SetStateRequest(SYS_STATE_INITIALIZATION_REQUEST);
PEX_Initialize();
CONT_Initialize();
SPS_Initialize();
MEAS_Initialize();
MRC_Initialize();
```

### 4.3 1ms 周期任务

相关函数：

```c
FTSK_RunUserCodeCyclic1ms()
```

主要作用：

- 更新时间基；
- 更新诊断标志；
- 控制 AFE 测量；
- 读取 CAN 接收缓冲区。

关键逻辑：

```c
OS_IncrementTimer();
DIAG_UpdateFlags();
MEAS_Control();
CAN_ReadRxBuffer();
```

### 4.4 10ms 周期任务

相关函数：

```c
FTSK_RunUserCodeCyclic10ms()
```

这是 BMS 控制最关键的周期任务。

主要作用：

- 更新 FRAM 数据；
- 运行系统状态机；
- 检查互锁；
- 控制 ADC；
- 控制电源开关；
- CAN 主处理；
- SOF 计算；
- 监控算法执行时间；
- 触发 SBC；
- 校验 AFE/Pack 测量；
- 最后触发 BMS 状态机。

关键逻辑：

```c
SYSM_UpdateFramData();
SYS_Trigger(&sys_state);
ILCK_Trigger();
ADC_Control();
SPS_Ctrl();
CAN_MainFunction();
SOF_Calculation();
ALGO_MonitorExecutionTime();
SBC_Trigger(&sbc_stateMcuSupervisor);
BMS_Trigger();
```

`BMS_Trigger()` 被放在 10ms 任务最后执行，是为了让前面模块先更新数据，然后 BMS 状态机基于最新数据做判断和动作。

### 4.5 100ms 周期任务

相关函数：

```c
FTSK_RunUserCodeCyclic100ms()
```

主要作用：

- 每 1s 运行 SOC/SOE 状态估算；
- 触发均衡；
- 触发绝缘检测；
- LED 状态更新；
- 检查 30C 供电钳位。

关键逻辑：

```c
SE_RunStateEstimations();
BAL_Trigger();
IMD_Trigger();
LED_Trigger();
MINFO_CheckSupplyVoltageClamp30c();
```

## 5. BMS 状态机核心逻辑

BMS 主状态机文件：

```text
src/app/application/bms/bms.c
```

状态机接口声明：

```text
src/app/application/bms/bms.h
```

核心函数：

```c
void BMS_Trigger(void)
```

该函数每 10ms 运行一次。

### 5.1 BMS_Trigger() 前置检查

只要 BMS 已经不是 `UNINITIALIZED`，每次触发都会先执行：

```c
BMS_GetMeasurementValues();
BMS_UpdateBatterySystemState(&bms_tablePackValues);
SOA_CheckVoltages(&bms_tableMinMax);
SOA_CheckTemperatures(&bms_tableMinMax, &bms_tablePackValues);
SOA_CheckCurrent(&bms_tablePackValues);
SOA_CheckSlaveTemperatures();
BMS_CheckOpenSenseWire();
CONT_CheckFeedback();
```

这些动作含义如下：

| 调用 | 含义 |
| --- | --- |
| `BMS_GetMeasurementValues()` | 从数据库读取测量值 |
| `BMS_UpdateBatterySystemState()` | 更新充电/放电/静置状态 |
| `SOA_CheckVoltages()` | 检查单体电压是否越界 |
| `SOA_CheckTemperatures()` | 检查温度是否越界 |
| `SOA_CheckCurrent()` | 检查电流是否越界 |
| `SOA_CheckSlaveTemperatures()` | 检查从板温度 |
| `BMS_CheckOpenSenseWire()` | 检查采样线开路 |
| `CONT_CheckFeedback()` | 检查接触器反馈 |

### 5.2 BMS 主状态

主要状态定义在 `bms.h` 中：

```c
BMS_FSM_STATE_UNINITIALIZED
BMS_FSM_STATE_INITIALIZATION
BMS_FSM_STATE_INITIALIZED
BMS_FSM_STATE_IDLE
BMS_FSM_STATE_OPEN_CONTACTORS
BMS_FSM_STATE_STANDBY
BMS_FSM_STATE_PRECHARGE
BMS_FSM_STATE_NORMAL
BMS_FSM_STATE_DISCHARGE
BMS_FSM_STATE_CHARGE
BMS_FSM_STATE_ERROR
```

可以理解为：

| 状态 | 含义 |
| --- | --- |
| `UNINITIALIZED` | 未初始化，等待初始化请求 |
| `INITIALIZATION` | 初始化中 |
| `INITIALIZED` | 初始化完成 |
| `IDLE` | 空闲状态，检查错误和 CAN 请求 |
| `OPEN_CONTACTORS` | 打开接触器，用于进入待机或错误保护 |
| `STANDBY` | 待机状态，可允许均衡 |
| `PRECHARGE` | 预充流程 |
| `NORMAL` | 正常运行 |
| `DISCHARGE` | 放电路径 |
| `CHARGE` | 充电路径 |
| `ERROR` | 错误状态 |

### 5.3 典型运行流程

典型状态流如下：

```text
UNINITIALIZED
  -> INITIALIZATION
  -> INITIALIZED
  -> IDLE
  -> STANDBY
  -> PRECHARGE
  -> NORMAL / CHARGE / DISCHARGE
```

如果检测到严重故障，则通常走：

```text
任意运行状态
  -> OPEN_CONTACTORS
  -> ERROR
```

如果收到待机请求，则通常走：

```text
运行状态
  -> OPEN_CONTACTORS
  -> STANDBY
```

## 6. BMS 保护参数主要配置位置

保护参数分散在多个配置文件中。建议按参数类型查找。

### 6.1 电芯级保护参数

文件：

```text
src/app/application/config/battery_cell_cfg.h
src/app/application/config/battery_cell_cfg.c
```

主要参数：

| 参数 | 含义 |
| --- | --- |
| `BC_TEMPERATURE_MAX_DISCHARGE_MSL_ddegC` | 放电最高温度，最大安全限制 |
| `BC_TEMPERATURE_MAX_DISCHARGE_RSL_ddegC` | 放电最高温度，推荐安全限制 |
| `BC_TEMPERATURE_MAX_DISCHARGE_MOL_ddegC` | 放电最高温度，最大运行限制 |
| `BC_TEMPERATURE_MIN_DISCHARGE_MSL_ddegC` | 放电最低温度，最大安全限制 |
| `BC_TEMPERATURE_MAX_CHARGE_MSL_ddegC` | 充电最高温度，最大安全限制 |
| `BC_TEMPERATURE_MIN_CHARGE_MSL_ddegC` | 充电最低温度，最大安全限制 |
| `BC_VOLTAGE_MAX_MSL_mV` | 单体最高电压，最大安全限制 |
| `BC_VOLTAGE_MAX_RSL_mV` | 单体最高电压，推荐安全限制 |
| `BC_VOLTAGE_MAX_MOL_mV` | 单体最高电压，最大运行限制 |
| `BC_VOLTAGE_MIN_MSL_mV` | 单体最低电压，最大安全限制 |
| `BC_VOLTAGE_MIN_RSL_mV` | 单体最低电压，推荐安全限制 |
| `BC_VOLTAGE_MIN_MOL_mV` | 单体最低电压，最大运行限制 |
| `BC_VOLTAGE_DEEP_DISCHARGE_mV` | 深度放电电压 |
| `BC_CURRENT_MAX_DISCHARGE_MSL_mA` | 单体最大放电电流，最大安全限制 |
| `BC_CURRENT_MAX_CHARGE_MSL_mA` | 单体最大充电电流，最大安全限制 |
| `BC_CAPACITY_mAh` | 电芯容量，用于 SOC |
| `BC_ENERGY_Wh` | 电芯能量 |

其中：

- `MSL` 表示 Maximum Safety Limit，最大安全限制。触发后通常会请求错误状态并断开接触器。
- `RSL` 表示 Recommended Safety Limit，推荐安全限制。通常作为告警。
- `MOL` 表示 Maximum Operating Limit，最大运行限制。通常作为运行边界提示。

### 6.2 电池系统拓扑与整包保护参数

文件：

```text
src/app/application/config/battery_system_cfg.h
src/app/application/config/battery_system_cfg.c
```

主要参数：

| 参数 | 含义 |
| --- | --- |
| `BS_NR_OF_STRINGS` | 并联 string 数量 |
| `BS_NR_OF_MODULES_PER_STRING` | 每个 string 中模块数量 |
| `BS_NR_OF_CELL_BLOCKS_PER_MODULE` | 每个模块中 cell block 数量 |
| `BS_NR_OF_PARALLEL_CELLS_PER_CELL_BLOCK` | 每个 cell block 并联电芯数量 |
| `BS_NR_OF_TEMP_SENSORS_PER_MODULE` | 每个模块温度传感器数量 |
| `BS_NR_OF_CELL_BLOCKS` | 总 cell block 数量，由宏自动计算 |
| `BS_NR_OF_TEMP_SENSORS` | 总温度传感器数量，由宏自动计算 |
| `BS_CURRENT_MEASUREMENT_RESPONSE_TIMEOUT_ms` | 电流测量响应超时 |
| `BS_COULOMB_COUNTING_MEASUREMENT_RESPONSE_TIMEOUT_ms` | 库仑计量响应超时 |
| `BS_ENERGY_COUNTING_MEASUREMENT_RESPONSE_TIMEOUT_ms` | 能量计量响应超时 |
| `BS_MAIN_CONTACTORS_MAXIMUM_BREAK_CURRENT_mA` | 主接触器允许分断的最大电流 |
| `BS_MAIN_FUSE_MAXIMUM_TRIGGER_DURATION_ms` | 等待熔断器动作的最大时间 |
| `BS_MAXIMUM_STRING_CURRENT_mA` | 单串最大电流 |
| `BS_MAXIMUM_PACK_CURRENT_mA` | 整包最大电流 |
| `BS_IGNORE_INTERLOCK_FEEDBACK` | 是否忽略互锁反馈 |
| `BS_CHECK_CAN_TIMING` | 是否检查 CAN 请求周期 |
| `BS_BALANCING_DEFAULT_INACTIVE` | 均衡默认是否关闭 |
| `BS_NR_OF_CONTACTORS` | 接触器总数 |
| `BS_REST_CURRENT_mA` | 判断电池静置状态的电流阈值 |
| `BS_CS_THRESHOLD_NO_CURRENT_mA` | 电流传感器零电流抖动阈值 |

### 6.3 BMS 状态机与预充参数

文件：

```text
src/app/application/config/bms_cfg.h
```

主要参数：

| 参数 | 含义 |
| --- | --- |
| `BMS_REQ_ID_STANDBY` | 通过 CAN 请求 STANDBY 的 ID |
| `BMS_REQ_ID_NORMAL` | 通过 CAN 请求 NORMAL 的 ID |
| `BMS_REQ_ID_CHARGE` | 通过 CAN 请求 CHARGE 的 ID |
| `BMS_STATEMACHINE_TASK_CYCLE_CONTEXT_MS` | BMS 状态机运行周期，当前为 10ms |
| `BMS_WAIT_TIME_AFTER_CLOSING_STRING_CONTACTOR` | 闭合 string 接触器后的等待时间 |
| `BMS_WAIT_TIME_AFTER_OPENING_STRING_CONTACTOR` | 打开 string 接触器后的等待时间 |
| `BMS_STRING_CLOSE_TIMEOUT` | string 闭合超时 |
| `BMS_STRING_OPEN_TIMEOUT` | string 打开超时 |
| `BMS_NEXT_STRING_VOLTAGE_LIMIT_MV` | 允许闭合下一 string 的电压差限制 |
| `BMS_AVERAGE_STRING_CURRENT_LIMIT_MA` | 允许闭合下一 string 的平均电流限制 |
| `BMS_PRECHARGE_MONITORING_PARAMETERS` | 预充监控方式 |
| `BMS_PRECHARGE_TRIES` | 允许预充尝试次数 |
| `BMS_PRECHARGE_VOLTAGE_THRESHOLD_mV` | 预充完成电压差阈值 |
| `BMS_PRECHARGE_CURRENT_THRESHOLD_mA` | 预充完成电流阈值 |
| `BMS_MAXIMUM_PRECHARGE_DURATION_ms` | 最大预充持续时间 |
| `BMS_PRECHARGE_CLOSE_TIMEOUT` | 预充接触器闭合超时 |
| `BMS_PRECHARGE_OPEN_TIMEOUT` | 预充接触器打开超时 |

### 6.4 SOA 安全边界判断逻辑

文件：

```text
src/app/application/soa/soa.c
src/app/application/config/soa_cfg.c
src/app/application/config/soa_cfg.h
```

`soa_cfg.c` 中的函数会把运行时测量值和配置阈值做比较，例如：

```c
SOA_IsPackCurrentLimitViolated()
SOA_IsStringCurrentLimitViolated()
SOA_IsCellCurrentLimitViolated()
```

它们引用的阈值主要来自：

```text
BS_MAXIMUM_PACK_CURRENT_mA
BS_MAXIMUM_STRING_CURRENT_mA
BC_CURRENT_MAX_CHARGE_MSL_mA
BC_CURRENT_MAX_DISCHARGE_MSL_mA
```

### 6.5 诊断事件、故障等级与延时

文件：

```text
src/app/engine/config/diag_cfg.c
src/app/engine/config/diag_cfg.h
```

`diag_cfg.c` 中的 `diag_diagnosisIdConfiguration[]` 决定每类诊断事件的：

- 诊断 ID；
- 敏感度；
- 严重等级；
- 延时；
- 是否启用；
- 回调函数。

典型诊断项包括：

| 诊断项 | 含义 |
| --- | --- |
| `DIAG_ID_CELL_VOLTAGE_OVERVOLTAGE_MSL` | 单体过压，最大安全限制 |
| `DIAG_ID_CELL_VOLTAGE_UNDERVOLTAGE_MSL` | 单体欠压，最大安全限制 |
| `DIAG_ID_TEMP_OVERTEMPERATURE_CHARGE_MSL` | 充电过温 |
| `DIAG_ID_OVERCURRENT_CHARGE_CELL_MSL` | 单体充电过流 |
| `DIAG_ID_PACK_OVERCURRENT_DISCHARGE_MSL` | 整包放电过流 |
| `DIAG_ID_PACK_OVERCURRENT_CHARGE_MSL` | 整包充电过流 |
| `DIAG_ID_CURRENT_SENSOR_RESPONDING` | 电流传感器响应错误 |
| `DIAG_ID_STRING_MINUS_CONTACTOR_FEEDBACK` | 负极接触器反馈错误 |
| `DIAG_ID_STRING_PLUS_CONTACTOR_FEEDBACK` | 正极接触器反馈错误 |
| `DIAG_ID_PRECHARGE_CONTACTOR_FEEDBACK` | 预充接触器反馈错误 |

诊断严重等级中，`DIAG_FATAL_ERROR` 通常会导致 BMS 进入错误处理流程，最终打开接触器。

### 6.6 接触器配置

文件：

```text
src/app/driver/config/contactor_cfg.c
src/app/driver/config/contactor_cfg.h
```

关键数组：

```c
CONT_CONTACTOR_STATE_s cont_contactorStates[BS_NR_OF_CONTACTORS]
```

这里配置每个接触器：

- 初始状态；
- 反馈类型；
- 所属 string；
- 接触器类型：正极、负极、预充；
- 控制通道；
- 推荐分断方向。

典型接触器类型：

```c
CONT_PLUS
CONT_MINUS
CONT_PRECHARGE
```

### 6.7 均衡参数

文件：

```text
src/app/application/config/bal_cfg.h
src/app/application/config/bal_cfg.c
```

主要参数：

| 参数 | 含义 |
| --- | --- |
| `BAL_DEFAULT_THRESHOLD_mV` | 默认均衡阈值 |
| `BAL_MAXIMUM_THRESHOLD_mV` | 最大允许均衡阈值 |
| `BAL_MINIMUM_THRESHOLD_mV` | 最小允许均衡阈值 |
| `BAL_HYSTERESIS_mV` | 均衡滞回 |
| `BAL_LOWER_VOLTAGE_LIMIT_mV` | 允许均衡的最低电压 |
| `BAL_UPPER_TEMPERATURE_LIMIT_ddegC` | 允许均衡的最高温度 |

### 6.8 合理性检查参数

文件：

```text
src/app/application/config/plausibility_cfg.h
```

主要参数：

| 参数 | 含义 |
| --- | --- |
| `PL_STRING_VOLTAGE_TOLERANCE_mV` | string 电压一致性容差 |
| `PL_CELL_VOLTAGE_TOLERANCE_mV` | 单体电压冗余测量容差 |
| `PL_CELL_TEMPERATURE_TOLERANCE_dK` | 单体温度冗余测量容差 |
| `PL_CELL_VOLTAGE_SPREAD_TOLERANCE_mV` | 单体电压离散度容差 |
| `PL_CELL_TEMPERATURE_SPREAD_TOLERANCE_dK` | 单体温度离散度容差 |

### 6.9 BMS 从板参数

文件：

```text
src/app/application/config/bms-slave_cfg.h
```

主要参数：

| 参数 | 含义 |
| --- | --- |
| `SLV_NR_OF_GPIOS_PER_MODULE` | 每模块 GPIO 数量 |
| `SLV_USE_MUX_FOR_TEMP` | 温度采样是否使用多路复用器 |
| `SLV_NR_OF_GPAS_PER_MODULE` | 每模块 GPA 数量 |
| `SLV_BALANCING_RESISTANCE_ohm` | 从板均衡电阻值 |
| `SLV_CELL_INPUT_CAPACITOR_CAPACITANCE_nF` | 单体输入电容 |

## 7. 如果要修改 BMS 保护板参数，建议按这个顺序找

### 7.1 修改电芯规格

优先看：

```text
src/app/application/config/battery_cell_cfg.h
src/app/application/config/battery_cell_cfg.c
```

适合修改：

- 单体过压/欠压；
- 充放电温度范围；
- 充放电电流限制；
- 容量；
- 能量；
- SOC 查表。

### 7.2 修改电池包拓扑

优先看：

```text
src/app/application/config/battery_system_cfg.h
```

适合修改：

- 串并联结构；
- 模块数量；
- 温度传感器数量；
- contactor 数量；
- 整包/单串电流限制；
- 互锁和 CAN timing 行为。

### 7.3 修改预充和接触器动作

优先看：

```text
src/app/application/config/bms_cfg.h
src/app/application/bms/bms.c
src/app/driver/config/contactor_cfg.c
```

适合修改：

- 预充完成电压阈值；
- 预充完成电流阈值；
- 最大预充时间；
- 接触器开关等待时间；
- 接触器反馈配置；
- 接触器动作顺序。

### 7.4 修改故障等级和诊断延时

优先看：

```text
src/app/engine/config/diag_cfg.c
src/app/engine/config/diag_cfg.h
```

适合修改：

- 某个故障是 fatal、warning 还是 info；
- 故障触发敏感度；
- 故障确认延时；
- 是否启用某个诊断项；
- 故障回调函数。

### 7.5 修改模块选型

优先看：

```text
conf/bms/bms.json
```

适合修改：

- AFE 型号；
- 温度传感器型号；
- 电流传感器型号；
- 绝缘检测设备；
- SOC/SOE/SOF/SOH 算法；
- 均衡策略。

## 8. 一句话总结

`main.c` 负责启动硬件和操作系统；`ftask_cfg.c` 负责任务周期调度；`BMS_Trigger()` 是 BMS 运行逻辑核心；`battery_cell_cfg.h`、`battery_system_cfg.h`、`bms_cfg.h`、`diag_cfg.c` 是保护参数和保护行为最关键的配置位置；`conf/bms/bms.json` 则决定工程编译时启用哪些硬件和算法模块。

