# 软件验证记录

验证对象为 STM32F103C8T6 的 HAL 固件和主机测试模型。实物只做了部分验证，见下文；Proteus 和 PCB 尚未纳入本记录。

| 项目 | 结果 |
| --- | --- |
| MQ4/MQ6/MQ7 三路采样 | PASS |
| 五按键和 OLED 页面 | PASS |
| 报警、继电器和蜂鸣器节奏 | PASS |
| AT24C64 参数与历史 | PASS |
| lockout 安全锁存 | PASS |
| USART1/HC-05 协议 | PASS |
| Host Test | PASS |
| Keil5 工程编译 | PASS |
| 实物 / Proteus | PARTIAL / NOT VERIFIED |

## 实物验证（部分，2026-09-16）

台面配置：ST-Link、OLED、USB-TTL 串口、HC-05 模块、继电器模块、蜂鸣器模块、按键、LED 均已接；**存储芯片未接**；MQ4 位接的是 **MQ135**（不是 MQ-4），MQ6/MQ7 的 ADC 输入已接地。逐项说明验到什么程度：

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| ST-Link 烧写 | PASS | 烧写后 verify OK |
| 固件运行 | PASS | 读 `uwTick` 确认预热按设定时长结束、主循环时序正常 |
| OLED 显示 | PASS | I2C1 走**默认引脚 PB6/PB7**（无重映射）验证通过：`oled.ready = true` 说明面板真实应答，读回 1024 字节帧缓冲与字模表逐格比对位差为 0，实时页、参数页、历史页、报警页内容均正确，按键翻页时帧缓冲实时跟着变。此前在 PB8/PB9 加重映射的接法上也是通过的，改用默认引脚是为了让 Proteus 仿真也能跑 |
| 串口协议 | PASS | `tools/serial_check.py COM11 --alarm` 共 36 项检查全过：命令分发、大小写不敏感、参数解析、范围校验、空行不回话、多行应答、远程关阀拒绝、`ALARM` 主动推送、`alarm_count` 递增 |
| USART2（HC-05） | 部分 | 寄存器级确认：BRR=3750（9600 精确）、CR1 的 UE/TE/RE 全开、PA2/PA3 引脚配置与 `usart.c` 逐位一致。**空中链路未验**，本机未与 HC-05 配对 |
| ADC 采样 | 部分 | MQ135 在 MQ4 位读到约 650～1250 计数；MQ6/MQ7 接地后约 260～280。能转换、量程正常，但未做标定 |
| 蜂鸣器 | 部分 | 实物为**低电平触发**模块，`BUZZER_ON_LEVEL` 已按此改为 `GPIO_PIN_RESET`，并测得 PA7 空闲电平为高（不响）。报警时的实际发声未验 |
| 继电器 / 阀门 | 未验证 | 模块已接，负载用灯或小电机代替，动作未实测 |
| LED / 按键 | 未验证 | 已接，未逐个实测 |
| I2C2 / 存储 | 未验证 | 器件未接；`HISTORY?` 因写入失败始终显示 `HISTORY 0/510` |

烧写用 `tools/openocd.sh`。**不要用 STM32CubeIDE 自带的那个 OpenOCD**：它配 `st_scripts` 会在 `swj-dp.tcl` 里报 `Infinite eval recursion`，一次也连不上。

烧进板子的是 CMake/GCC 构建（`tools/build_arm.sh arm-debug`），不是 Keil 构建。Keil 构建用于 Proteus 仿真。

## Keil5 构建

打开 `stm32f103/MDK-ARM/smart_gas_monitor.uvprojx`，选择 `smart_gas_monitor` Target 并执行 Build。最终输出位于 `build/keil5/Artifacts/`，中间文件位于 `build/keil5/Listings/`。

上面总表里的 PASS 仅表示软件检查、主机测试或构建已完成，不代表实物验证已经通过；实物上究竟验了什么、没验什么，以「实物验证（部分）」一节为准。
