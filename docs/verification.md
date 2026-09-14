# 软件验证记录

日期：2026-09-14。

## 当前 HEAD（OLED、EEPROM 报警记录与双串口协议）

先 `make clean` 再从无缓存状态完整构建。三个功能提交（`fe153fd`、`4d8c404`、`8fdcf1f`）合并验证一次。

| 检查 | 结果 |
| --- | --- |
| CubeMX 重新生成 | PASS：main.c 的应用初始化/轮询、异常处理关阀、TIM2 中断钩子等 USER CODE 均保留；GPIO User Labels 生成 `*_Pin` / `*_GPIO_Port` 宏 |
| ARM 编译和链接 | PASS：GNU Arm 14.3.rel1，Cortex-M3，零 warning/error |
| 主机 C 单元测试 | PASS：MSVC 19.38，C11，/W4 /WX，`test_gas_monitor`、`test_settings`、`test_history`、`test_protocol` 四个全部通过 |
| 中断向量表指向 | PASS：TIM2、USART1、USART2、EXTI15_10 四个槽位 |
| 实物 / Proteus | NOT VERIFIED |

最终 ARM 构建：text = 27504 bytes，data = 92 bytes，bss = 4148 bytes。BIN 为 27596 bytes，占 Flash 64 KB 的 42%，RAM 20 KB 的 20%。本轮修复 OLED 行缓冲、TIM2 启动失败处理、历史写入失败状态传播，并将蜂鸣器 5 秒窗口改为 TIM2 的 500 个 10 ms tick。

BIN SHA256：`d61ef6a29bf48cec8084abd206c2e22de17125e5c4ddb1f846a5613efee13d84`。

### 中断向量表

四个槽位的值与符号相符（向量表项最低位为 Thumb 标志，故比符号地址大 1）：

| 槽位偏移 | 偏移地址 | 内容 | 符号 |
| --- | --- | --- | --- |
| 0xB0（IRQ 28，TIM2） | 0x080000b0 | 0x080006c1 | `TIM2_IRQHandler`（0x080006c0） |
| 0xD4（IRQ 37，USART1） | 0x080000d4 | 0x080006d5 | `USART1_IRQHandler`（0x080006d4） |
| 0xD8（IRQ 38，USART2） | 0x080000d8 | 0x080006e5 | `USART2_IRQHandler`（0x080006e4） |
| 0xE0（IRQ 40，EXTI15_10） | 0x080000e0 | 0x080006f5 | `EXTI15_10_IRQHandler`（0x080006f4） |

`app_tick_isr()`（0x0800353c）由 `TIM2_IRQHandler` 的 `USER CODE BEGIN TIM2_IRQn 1` 段调用。`MX_TIM2_Init()` 中 `Prescaler = 71`、`Period = 9999`，即 72 MHz / 72 / 10000 = 10 ms。

### USART NVIC

`.ioc` 中新增 `NVIC.USART1_IRQn` 与 `NVIC.USART2_IRQn`，抢占优先级 2，同样是第 7、8 字段必须为 `true:true`——与 EXTI 那次踩到的是同一条规则。按产物核对：`usart.c:118` 与 `usart.c:148` 分别生成 `HAL_NVIC_EnableIRQ(USART1_IRQn)` 和 `HAL_NVIC_EnableIRQ(USART2_IRQn)`。

### 本轮的设计核对

这几处不是「编译通过」能证明的，因此单独记录判断依据：

- **阻塞发送会把自己打成 FAULT。** 14 条历史记录约 750 字节，HC-05 在 9600 baud 下需要 780 ms；采样器的故障超时是 3 个采样周期，100 ms 周期下只有 300 ms。若在 `HISTORY?` 的应答里就地阻塞发送，「查询历史」这个动作本身就会触发采样超时、进入 FAULT 并关阀。核对方式：发送改为中断驱动，且 `pump_replies()` 只在两路都空闲时取一行，因此单次主循环的最坏占用与一次转换同量级，而非与整段应答同量级。
- **远程写入不得造出读不回来的配置。** `SET` 走 `gas_monitor_set_threshold()` / `gas_monitor_set_period()`，两者都对整份候选配置调用 `gas_config_valid()`。核对方式见 `tests/test_protocol.c`：`SET PERIOD 255` 被拒（不是 10 ms 的整数倍），而 `SET PERIOD 250` 被接受（是整数倍且在范围内）——按键步进选不到 250，但合法值不因按键选不到就该被拒绝。
- **没有远程开阀。** `VALVE OPEN` 恒回 `ERR ONLY CLOSE`；`VALVE CLOSE` 之后仍须三路低于危险阈值 70% 并持续 3 秒，再由 KEY4 开阀。核对方式见 `tests/test_protocol.c` 的 `test_valve()`，其中先远程关阀、再走完整的低浓度安全窗口，最后确认只有 KEY4 能开阀。
- **报警记录只写一次。** 「首次进入 ALARM」由 `app_run()` 里的状态边沿判断决定，与串口报警通知共用同一处边沿。核对方式：该判断在 `app_run()` 中只出现一次，且 `last_state` 在每个主循环末尾无条件更新。

## 上一次验证（按键 EXTI 与 TIM2 采样节拍）

按键改为 EXTI 下降沿中断，采样节拍改由 TIM2 的 10 ms 更新中断驱动。

| 检查 | 结果 |
| --- | --- |
| CubeMX 实际生成 | PASS：6.18.0，STM32CubeF1 V1.8.7，生成 `tim.c`、`stm32f1xx_hal_tim.c` 等 |
| CubeMX 重新生成 | PASS：main.c 的应用初始化/轮询、异常处理关阀、TIM2 中断钩子等 USER CODE 均保留 |
| ARM 编译和链接 | PASS：GNU Arm 14.3.rel1，Cortex-M3，零 warning/error |
| 主机 C 单元测试 | PASS：MSVC 19.38，C11，/W4 /WX，`tests/test_gas_monitor.c` 与 `tests/test_settings.c` 全部通过 |
| 中断向量表指向 | PASS：`.isr_vector` 中 TIM2 槽位为 `0x08000665`、EXTI15_10 槽位为 `0x08000679`，与符号 `TIM2_IRQHandler`、`EXTI15_10_IRQHandler` 相符 |
| 实物 / Proteus | NOT VERIFIED |

最终 ARM 构建：text = 14632 bytes，data = 12 bytes，bss = 2108 bytes。BIN 为 14644 bytes。

BIN SHA256：`bada06ba51d976dac75d36167604e0ae48ebd2f24a1a20024c0cb3f43a0fbd44`。

`.ioc` 里的两处配置是手写后用无头 CubeMX 生成并**按产物核对**的，因为两者都会静默失败：

- `NVIC.EXTI15_10_IRQn` 与 `NVIC.TIM2_IRQn` 的 8 字段格式里，第 7、8 位必须为 `true:true`，否则 CubeMX 仍然生成中断处理函数和 `HAL_NVIC_SetPriority()`，但**不生成 `HAL_NVIC_EnableIRQ()`**，中断永不触发。修正后 `gpio.c`、`tim.c` 中均出现 `HAL_NVIC_EnableIRQ()`。
- TIM2 需要一个虚拟引脚 `VP_TIM2_VS_ClockSourceINT`（`Mode=Internal`）来满足 `Enable_Timer` 模式对 `VS_ClockSourceINT` 信号的要求。缺了它 CubeMX 在 `config load` 阶段静默丢弃整个 TIM2，`.ioc` 被 `config save` 写回后也不再有 TIM2，日志里没有任何提示。

核对项：`MX_TIM2_Init()` 中 `Prescaler = 71`、`Period = 9999`，即 72 MHz / 72 / 10000 = 100 Hz = 10 ms。

本轮不改变任何业务规则，主机测试用例未增减，全部沿用上一轮。

## 上一次验证（采样故障判定与采样周期下限修复）

| 检查 | 结果 |
| --- | --- |
| ARM 编译和链接 | PASS：GNU Arm 14.3.rel1，Cortex-M3，零 warning/error |
| 主机 C 单元测试 | PASS：MSVC 19.38，C11，/W4 /WX，`tests/test_gas_monitor.c` 与 `tests/test_settings.c` 全部通过 |
| 实物 / Proteus | NOT VERIFIED |

最终 ARM 构建：text = 12988 bytes，data = 12 bytes，bss = 2028 bytes。BIN 为 13000 bytes。

BIN SHA256：`d56174adabeaea4aa8e5ee6a56f7cb4fe69515d5d77007d1a04c929f40a5f8b7`。

本轮改三处：第一次采样尝试就失败时进入 FAULT 并锁存（原先要成功采样过一次之后失败才算，开机即坏的 ADC 会一直停在预热）；`GAS_PERIOD_MIN_MS` 由 50 改为 100，与按键可选列表首项一致；README 更正 `app/` 的 HAL 依赖描述。另有两处安全收尾：`NMI_Handler` 补上 `alarm_output_force_safe()`，`alarm_output.h` 里“可在 GPIO 配置前安全调用”的说法改为说明它只写 ODR、复位窗口靠硬件下拉兜底。

新增回归用例 `test_failed_attempt_is_a_fault`。该用例在旧语义下确认失败（`tests/test_gas_monitor.c` 第 94 行 `m.state == GAS_FAULT && m.latched` 不成立），在当前实现上通过。

## 上一次验证（重构后，提交 1d8ffb8）

目录重构与状态机修复后重新验证，先 `make clean` 再从无缓存状态完整构建。

| 检查 | 结果 |
| --- | --- |
| ARM 编译和链接 | PASS：GNU Arm 14.3.rel1，Cortex-M3，零 warning/error |
| 主机 C 单元测试 | PASS：MSVC 19.38，C11，/W4 /WX，`tests/test_gas_monitor.c` 与 `tests/test_settings.c` 全部通过 |
| 实物 / Proteus | NOT VERIFIED |

最终 ARM 构建：text = 12980 bytes，data = 12 bytes，bss = 2028 bytes。BIN 为 12992 bytes。

BIN SHA256：`ac29369653cb6d382ace7ad0185eabb6386b38b44e53db3a0a118040fa13dd01`。

交叉工具链无需单独安装，STM32CubeIDE 自带：`<CubeIDE>/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_*/tools/bin` 和同级的 `com.st.stm32cube.ide.mcu.externaltools.make.win32_*/tools/bin`，两者都加入 PATH 后即可执行上面的 make 命令。

重构本身是纯搬移：`gas_board.c` 拆成 `bsp/mq_sensor`、`bsp/key`、`bsp/alarm_output`、`bsp/at24c02`，`app/runtime` 改为 `app/app.c`，`app/monitor` 改为 `app/gas`，`app/parameters` 改为 `app/settings`。行为改动只有状态机部分，见下。

## 更早的验证（提交 ac803a9）

目录迁移验证：CubeMX 相关文件统一迁入 `stm32f103/cubemx/` 后，已通过新路径重新生成 HAL，并从无旧缓存的状态完整构建。

| 检查 | 结果 |
| --- | --- |
| CubeMX 实际生成 | PASS：6.18.0，STM32CubeF1 V1.8.7，生成 Core、Drivers、启动文件、链接脚本、Makefile |
| CubeMX 重新生成 | PASS：main.c 的应用初始化/轮询及 Error_Handler、异常处理中的关阀 USER CODE 保留 |
| ARM 编译和链接 | PASS：GNU Arm 14.3.rel1，Cortex-M3，生成 ELF/HEX/BIN，无编译 warning/error |
| 主机 C 单元测试 | PASS：MSVC 19.38，C11，/W4 /WX |
| 实物 / Proteus | NOT VERIFIED |

最终 ARM 构建：text = 12644 bytes，data = 12 bytes，bss = 2028 bytes。BIN 为 12656 bytes；链接脚本预留 heap/stack，目标容量为 Flash 64 KB、RAM 20 KB。

BIN SHA256：`e446a48b1489a36a74975995291668a140d0f377a3bd3310f6a56440721f711d`。

该哈希对应 ac803a9 的构建产物，其后的两次改动已不再适用。

## 测试覆盖

主机测试覆盖：三路分别触发预警/报警的边界、危险时禁止人工开阀、恢复后维持锁存、连续安全 3 秒后确认、采样失败/越界/超时、恢复采样不能掩盖停顿、32 位时间回绕、阈值上下限、采样周期设置与超时联动、延时保存、首次采样前不误判故障、首次采样失败判为故障、上电自动开阀、低阈值下仍可恢复。

存储测试覆盖：空 EEPROM、CRC 损坏回退、版本不匹配回退、越界值拒绝，以及双副本写入各字节处中断后保留旧配置。

历史测试覆盖：区域边界（0x20～0xFF 恰好 14 条）、空存储、写入后按倒序读回、重启后顺序延续、环形回绕覆盖最旧记录、写入中断留下的残槽被判为无效、CRC 损坏的记录被丢弃。

协议测试覆盖：状态/配置/历史查询的逐字节应答、大小写不敏感、空行不回话、远程设置的名称/数值/参数个数/范围四类拒绝、`VALVE CLOSE` 后仍须 KEY4、`VALVE OPEN` 恒回 `ERR ONLY CLOSE`、历史倒序与序号、报警主动通知只发一次、超长行不越界且之后协议仍可用。

两个关键回归用例都在旧实现上确认失败、在当前实现上通过：

- **低阈值恢复**：把阈值降到下限 200 后，旧实现要求读数低于 60 才允许恢复，实际不可能满足，阀门会被永久锁死。
- **首次采样失败**：旧实现只把“成功采样过之后再失败”判为故障，开机时就坏的 ADC 会一直停在预热而不报故障。

## 待验证

上述软件结果不代表传感器标定或硬件功能通过。待验证：8 MHz 晶振、ADC 电压和分压接线、MQ 模块预热/响应、继电器接点与有效电平、复位期间 PA8 的默认状态、EEPROM 地址/写周期、按键实物消抖、EXTI 下降沿在实物按键抖动下的触发次数、TIM2 10 ms 节拍的实测周期与长时间漂移，以及本轮新增的三项：SSD1306 的实际地址与 400 kHz 下的波形、HC-05 在 9600 baud 下连续收发历史记录的完整性、AT24C02 在 0x20～0xFF 段反复写入的耐久性与掉电时的记录完整性。

主机测试不覆盖 BSP 与组合层：`bsp/ssd1306`、`bsp/serial`、`app/display` 和 `app/app.c` 的引脚绑定、I2C 时序、中断收发与主循环调度都只经过编译和静态核对，需要 Proteus 或实物确认。
