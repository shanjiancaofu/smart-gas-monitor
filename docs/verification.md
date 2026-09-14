# 软件验证记录

日期：2026-09-14。

## 报警蜂鸣器时长可设置（`f8432ed` + `0d31c62`）

配置升到 v5 并新增蜂鸣器时长之后，先 `make clean` 再从无缓存状态完整构建，重新运行四套 Host Test。两个提交分别单独构建过。

| 检查 | 结果 |
| --- | --- |
| ARM 编译和链接 | PASS：GNU Arm 14.3.rel1，Cortex-M3，零 warning/error |
| 主机 C 单元测试 | PASS：MSVC 19.38，C11，/W4 /WX，`test_gas_monitor`、`test_settings`、`test_history`、`test_protocol` 四个全部通过 |
| 实物 / Proteus | NOT VERIFIED |

最终 ARM 构建：text = 28260 bytes，data = 92 bytes，bss = 4148 bytes。BIN 为 28352 bytes，占 Flash 64 KB 的 43%，RAM 20 KB 的 20%。文本段比上一轮增加 612 字节：多了一个配置字段、一组设置界面行、一条协议命令，以及跨层的两个 `_Static_assert`。

BIN SHA256：`488b422405d9a07724138527e6c4ff2b9f1108e86bc75189168d1ca60a6e7f6e`。

只含 `f8432ed` 的构建（即上一轮代码加故障入口静默蜂鸣器）为 text = 27644 bytes，BIN SHA256 `00f0921e22cf512ad676ac40e6a1a296d4bc60d0b8d5566009ac7536e4263b69`。这一轮的两个产物哈希都与上一轮的 `49d78ed3…` 不同，符合预期：两者都是真正的行为改动，不是注释或格式调整。

### 本轮的设计核对

- **16 个字节一个都不能多。** 配置区是两个 16 字节槽，历史区 0x20～0xFF 是 14 条 16 字节记录，加起来正好 256 字节，整片 AT24C02 没有空余。核对方式：`history.c` 与 `settings.c` 的区间常量相加等于 256，且新增字段占用的是压缩 sequence 腾出的字节，槽大小与历史区起点都没动。
- **v4 记录必须被挡住，而不是「几乎读对」。** 两个布局只差字节 2 和 3：v4 的 byte 3 是序号高字节，从 0 开始时为 0，读成蜂鸣器时长就是「不响」，且 CRC 与其余字段全部通过。核对方式见 `tests/test_settings.c` 的 `test_version_mismatch_falls_back()`：写入一份有效记录后把 `f.data[1]` 改成 4，`settings_load()` 必须失败。
- **越界的时长不能回绕成短时长。** `gas_buzzer_duration_ms()` 对 `>= GAS_BUZZER_ALWAYS` 的所有取值返回哨兵，只有 1～60 才做乘法；若先乘再截断，66 会变成 464 毫秒。核对方式见 `tests/test_gas_monitor.c`：66 和 255 都断言等于 `GAS_BUZZER_FOREVER_MS`。另有一条 `_Static_assert` 保证配置上限换算后仍小于哨兵。
- **`SET BUZZER 256` 不能被收窄成静默。** setter 接受 `uint16_t` 而不是字段的 `uint8_t`，先把上界判掉再转换。核对方式见 `tests/test_protocol.c`：`SET BUZZER 256` 回 `ERR RANGE` 且档位停在上一次的 `ALWAYS`。
- **`OFF` 和 `ALWAYS` 必须走自己的解析路径。** 它们排在 `SET` 分支里「先把第二个 token 解析成数字」那一步之前；顺序反了，两者都会被判成 `ERR VALUE` 就提前返回。核对方式：同一组用例里两者的成功路径与 `SET BUZZER FOO → ERR VALUE` 并存。
- **改蜂鸣器时长不重启安全窗口。** `config_changed()` 会清 `safe_timing`，`mark_dirty()` 不会；蜂鸣器时长只走后者。核对方式见 `tests/test_gas_monitor.c` 的 `test_buzzer_set()`：在 `SAFE_WAIT` 且 `reset_ready` 的监视器上调蜂鸣器，`reset_ready` 必须保持；紧接着调阈值，必须清零。

## 软件冻结候选（基于 `33c6189`）

完成 v4 lockout 安全收尾后，先 `make clean` 再从无缓存状态完整构建，并重新运行四套 Host Test。

| 检查 | 结果 |
| --- | --- |
| CubeMX 重新生成 | PASS：main.c 的应用初始化/轮询、异常处理关阀、TIM2 中断钩子等 USER CODE 均保留；GPIO User Labels 生成 `*_Pin` / `*_GPIO_Port` 宏 |
| ARM 编译和链接 | PASS：GNU Arm 14.3.rel1，Cortex-M3，零 warning/error |
| 主机 C 单元测试 | PASS：MSVC 19.38，C11，/W4 /WX，`test_gas_monitor`、`test_settings`、`test_history`、`test_protocol` 四个全部通过 |
| 中断向量表指向 | PASS：TIM2、USART1、USART2、EXTI15_10 四个槽位 |
| 实物 / Proteus | NOT VERIFIED |

最终 ARM 构建：text = 27648 bytes，data = 92 bytes，bss = 4148 bytes。BIN 为 27740 bytes，占 Flash 64 KB 的 42%，RAM 20 KB 的 20%。本轮将配置升级到 v4，lockout 纳入 CRC；ALARM、FAULT、远程 CLOSE 和 KEY4 使用统一锁存边沿持久化；持续状态不重复置 dirty；历史坏记录界面会清除残行。

BIN SHA256：`49d78ed3244f58ec23cedccacd05a52912958ca2328bea8398d820aca306359d`。

### 中断向量表

四个槽位的值与符号相符（向量表项最低位为 Thumb 标志，故比符号地址大 1）：

| 槽位偏移 | 偏移地址 | 内容 | 符号 |
| --- | --- | --- | --- |
| 0xB0（IRQ 28，TIM2） | 0x080000b0 | 0x080006c9 | `TIM2_IRQHandler`（0x080006c8） |
| 0xD4（IRQ 37，USART1） | 0x080000d4 | 0x080006dd | `USART1_IRQHandler`（0x080006dc） |
| 0xD8（IRQ 38，USART2） | 0x080000d8 | 0x080006ed | `USART2_IRQHandler`（0x080006ec） |
| 0xE0（IRQ 40，EXTI15_10） | 0x080000e0 | 0x080006fd | `EXTI15_10_IRQHandler`（0x080006fc） |

`app_tick_isr()`（0x0800356c）由 `TIM2_IRQHandler` 的 `USER CODE BEGIN TIM2_IRQn 1` 段调用。`MX_TIM2_Init()` 中 `Prescaler = 71`、`Period = 9999`，即 72 MHz / 72 / 10000 = 10 ms。

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

主机测试覆盖：三路分别触发预警/报警的边界、危险时禁止人工开阀、恢复后维持锁存、持久化 lockout 启动恢复、连续安全 3 秒后 KEY4 清除、持续 ALARM/FAULT 不重复置 dirty、采样失败/越界/超时、恢复采样不能掩盖停顿、32 位时间回绕、阈值上下限、采样周期设置与超时联动、延时保存、首次采样前不误判故障、首次采样失败判为故障、无锁存上电自动开阀、低阈值下仍可恢复、蜂鸣器取值的换算与越界处理、蜂鸣器按键的逐档与两端夹取、蜂鸣器 setter 的范围拒绝与「不重启安全窗口」。

存储测试覆盖：空 EEPROM、CRC 损坏回退、lockout 字节损坏回退、蜂鸣器字节损坏回退、版本不匹配回退（含 v4）、越界值拒绝，以及双副本写入各字节处中断后保留旧配置。

历史测试覆盖：区域边界（0x20～0xFF 恰好 14 条）、空存储、写入后按倒序读回、重启后顺序延续、环形回绕覆盖最旧记录、写入中断留下的残槽被判为无效、CRC 损坏的记录被丢弃。

协议测试覆盖：状态/配置/历史查询的逐字节应答、大小写不敏感、空行不回话、远程设置的名称/数值/参数个数/范围四类拒绝、`SET BUZZER` 的三个档位与 `ERR VALUE`/`ERR RANGE`/`ERR NAME` 三条拒绝路径、`VALVE CLOSE` 后仍须 KEY4、`VALVE OPEN` 恒回 `ERR ONLY CLOSE`、历史倒序与序号、报警主动通知只发一次、超长行不越界且之后协议仍可用。

两个关键回归用例都在旧实现上确认失败、在当前实现上通过：

- **低阈值恢复**：把阈值降到下限 200 后，旧实现要求读数低于 60 才允许恢复，实际不可能满足，阀门会被永久锁死。
- **首次采样失败**：旧实现只把“成功采样过之后再失败”判为故障，开机时就坏的 ADC 会一直停在预热而不报故障。

## 待验证

上述软件结果不代表传感器标定或硬件功能通过。待验证：8 MHz 晶振、ADC 电压和分压接线、MQ 模块预热/响应、继电器接点与有效电平、复位期间 PA8 的默认状态、EEPROM 地址/写周期、按键实物消抖、EXTI 下降沿在实物按键抖动下的触发次数、TIM2 10 ms 节拍的实测周期与长时间漂移，以及此前新增的三项：SSD1306 的实际地址与 400 kHz 下的波形、HC-05 在 9600 baud 下连续收发历史记录的完整性、AT24C02 在 0x20～0xFF 段反复写入的耐久性与掉电时的记录完整性。

本轮另外两项只能上实物确认：**蜂鸣器实际响多久**与设定值是否相符（`BUZZER_TICK_MS` 与实际节拍的一致性由 `app.c` 的 `_Static_assert` 锁定，但「写进寄存器的值」到「听到的时长」这段没有软件可证的环节）；以及**`alarm_output_force_safe()` 之后蜂鸣器是否真的静默**——那条路径只在故障时进入，主机测试和 Proteus 都不会替它做保。

主机测试不覆盖 BSP 与组合层：`bsp/ssd1306`、`bsp/serial`、`bsp/alarm_output` 和 `app/display`、`app/app.c` 的引脚绑定、I2C 时序、中断收发与主循环调度都只经过编译和静态核对，需要 Proteus 或实物确认。蜂鸣器的计时换算（`gas_buzzer_duration_ms()`）特意放在被测得到的 `gas_monitor.c`，就是为了让这段逻辑不必只靠静态核对。
