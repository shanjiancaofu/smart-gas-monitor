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

板子上只接了 ST-Link、OLED 和串口，其余外设未连接。逐项说明验到什么程度：

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| ST-Link 烧写 | PASS | 烧写后 verify OK |
| 固件运行 | PASS | 读 `uwTick` 确认预热按设定时长结束、主循环时序正常 |
| OLED 显示 | PASS | 读回 1024 字节帧缓冲，与字模表逐格比对位差为 0；实时页、参数页、历史页、报警页四页内容均正确 |
| ADC 采样 | 部分 | 只有浮空读数（约 2050～2150），能转换但未接传感器 |
| I2C2 / AT24C64 | 未验证 | 器件未连接 |
| 按键 | 未验证 | 未连接 |
| 继电器 / 阀门 / 蜂鸣器 / LED | 未验证 | 未连接 |
| 串口协议 | 未验证 | 未实际收发命令 |
| HC-05 | 未验证 | 未连接 |

烧写用 `tools/openocd.sh`。**不要用 STM32CubeIDE 自带的那个 OpenOCD**：它配 `st_scripts` 会在 `swj-dp.tcl` 里报 `Infinite eval recursion`，一次也连不上。

烧进板子的是 CMake/GCC 构建（`tools/build_arm.sh arm-debug`），不是 Keil 构建。

## Keil5 构建

打开 `stm32f103/MDK-ARM/smart_gas_monitor.uvprojx`，选择 `smart_gas_monitor` Target 并执行 Build。最终输出位于 `build/keil5/Artifacts/`，中间文件位于 `build/keil5/Listings/`。

上面总表里的 PASS 仅表示软件检查、主机测试或构建已完成，不代表实物验证已经通过；实物上究竟验了什么、没验什么，以「实物验证（部分）」一节为准。
