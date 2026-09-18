# 软件验证记录

验证对象为 STM32F103C8T6 的 HAL 固件和主机测试模型。实物只做了部分验证，见下文；Proteus 已完成一轮验收（仿真跑的是 Keil 的 soft 目标）；**PCB 尚未纳入本记录**。

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
| Proteus 仿真 | PASS |
| 实物 | PARTIAL |

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

烧进板子的是 CMake/GCC 构建（`tools/build_arm.sh arm-debug-hw`），不是 Keil 构建。Keil 的 **soft** 目标用于 Proteus 仿真。

## Proteus 仿真验收（2026-09-18）

跑的是 **Keil soft 目标**的固件。逐项走完[演示脚本](../hardware/proteus/README.md#八演示脚本)：状态机 `WARMUP → NORMAL → WARNING → ALARM → SAFE_WAIT → READY → KEY4`、三路单独报警与多路报警、四个 LED、蜂鸣器各档位、风扇继电器、舵机、五按键、两路虚拟终端、参数保存与报警历史读写，均通过。

### 舵机：把 Proteus 的模型缺口逼了出来

舵机最初接 PB8 / TIM4_CH3，在仿真里**一直是死的**——PB8 上量不到任何波形。中间试过三种做法（TIM4_CH3 硬件 AF、TIM4 中断软件翻转、TIM2 中断软件翻转）全都没用，其中还夹着我自己的一个 bug（把 TIM2 的比较通道配成了复位默认的「冻结」模式，比较中断根本不产生，引脚拉高后再没人拉低）。

真正把方向掰正的是一次 **1 Hz 方波诊断**：让 PB8 慢速翻转，肉眼就能看网络指示点。它一次证明了 PB8 的驱动和接线都是好的，于是问题只剩「这个仿真模型不实现那一路外设」。

**结论：该 Proteus 的 STM32 模型不实现 TIM4_CH3 走 PB8**，和它「不认 I2C1 引脚重映射」是同类的模型缺失，而且不报错、不提示，只表现为「引脚没波形」。两份能跑的参考工程用的都是 TIM2_CH2 / PA1，而 TIM2 在这个仿真里是好的（工程自己的 10 ms 系统节拍一直跑在它上面）。

**最终做法：仿真与实物用不同的引脚。** 差异全部收在 `#if USE_SOFT_I2C` 里，实物那条路径一行未动。

| | 舵机 | 定时器 | MQ6 | 阀门灯 |
| --- | --- | --- | --- | --- |
| 仿真（soft） | PA1 | TIM2_CH2 | PA5 | PB9 |
| 实物（hw） | PB8 | TIM4_CH3 | PA1 | PA5 |

PA1 让给舵机后 MQ6 必须搬走，而 C8 是 48 脚封装、没有 PC0~PC5，十个带 ADC 的引脚全被占满，所以又挤了一个器件出来——挑中阀门灯是因为它最不显眼。

### 仿真能力的边界（写进结论时别把话说满）

- Proteus 的舵机模型按 `PW_MIN`/`PW_MAX` 定义一个**有效脉宽窗口**，角度映射固定为 1 ms→-90°、2 ms→+90°，超窗口就钳到两端。所以仿真能确认「舵机通路是通的、两个档位落在两端」，**但分辨不出 500 µs 和 600 µs 这类中间值**——脉宽精度只能靠实物或逻辑分析仪。
- 仿真分支把 TIM2 的周期从 10 ms 改成 20 ms（舵机要 50 Hz），系统节拍由中断里补第二次 `app_tick_isr()` 维持 10 ms。副作用是蜂鸣器节奏的时间分辨率降到 20 ms；最短的一段是 80 ms，听感不变。
- 换页和翻历史记录时屏幕会停顿约 0.7 秒（软件 I2C 下整屏推一次的代价），换来的是不再出现新旧两页并存的撕裂。

## Keil5 构建

打开 `stm32f103/MDK-ARM/smart_gas_monitor.uvprojx`，按用途选 Target：`smart_gas_monitor_hw`（实物）或 `smart_gas_monitor_soft`（Proteus）。最终输出位于 `build/keil5/<hw|soft>/Artifacts/`，中间文件位于 `build/keil5/<hw|soft>/Listings/`。`tools/build_keil.ps1` 一次编两个并收集产物。

上面总表里的 PASS 仅表示软件检查、主机测试或构建已完成，不代表实物验证已经通过；实物上究竟验了什么、没验什么，以「实物验证（部分）」一节为准。
