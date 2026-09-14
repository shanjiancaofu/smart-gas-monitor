# 变更记录

按时间倒序记录固件的功能性改动。每条附提交短哈希，完整差异用 `git show <哈希>` 查看。构建与测试结果见[验证记录](verification.md)，分层与业务规则见[固件说明](firmware.md)。

日期均为 2026-09-14。

## `fe84791` — 按键 EXTI 与 TIM2 采样节拍

### 修复

- `.ioc` 中 `NVIC.EXTI15_10_IRQn` 的第 7、8 字段由 `false:false` 改为 `true:true`。原写法下 CubeMX 照样生成中断处理函数和 `HAL_NVIC_SetPriority()`，但**不生成 `HAL_NVIC_EnableIRQ()`**，中断永远不会触发。
- `.ioc` 补上虚拟引脚 `VP_TIM2_VS_ClockSourceINT`。`Enable_Timer` 模式要求 `VS_ClockSourceINT` 信号，缺了它 CubeMX 在 `config load` 阶段静默丢弃整个 TIM2，日志里没有任何提示。

### 变更

- `app_run()` 的采样调度由「和 `HAL_GetTick()` 比差值」改为按 `sample_period_ms / 10` 个 TIM2 节拍触发，并按整周期推进游标，周期不再随主循环抖动漂移；主循环停顿超过一个整周期时重新对齐。`HAL_GetTick()` 仍是状态机的时间基准，超时、运行时长和消抖都用它。
- `app_t` 的 `last_sample` 换成 `last_tick`。
- `key_poll()` 把 EXTI 锁存的边沿并入「电平变化」判定。电平仍是消抖的依据，边沿只用来给消抖窗口打时间戳，因此主循环被 EEPROM 写阻塞期间按下的键不会丢，而两次轮询之间按下又松开的抖动也不会变成一次按键。

### 新增

- 按键改走 EXTI：PB12～PB15 配成 `GPIO_MODE_IT_FALLING` 加上拉，使能 `EXTI15_10_IRQn`（抢占优先级 0）。HAL 生成的处理函数逐个调用 `HAL_GPIO_EXTI_Callback()`，实现放在 `bsp/key.c`，只把「哪一路出现下降沿」记进一个待处理位；消抖和分发仍留在主循环。
- TIM2 作为采样节拍：预分频 71、周期 9999，即 72 MHz / 72 / 10000 = 100 Hz = 10 ms 更新中断，使能 `TIM2_IRQn`（抢占优先级 1）。中断只对 `app_ticks` 加一，转换、显示和 EEPROM 全部留在主循环。
- `app_tick_isr()`，由 `TIM2_IRQHandler` 的 USER CODE 调用；`app_init()` 增加 `TIM_HandleTypeDef *tick` 参数并在其中 `HAL_TIM_Base_Start_IT()`。

### 测试

- 无新增用例，本轮不改变业务规则；`tests/test_gas_monitor.c` 与 `tests/test_settings.c` 全部沿用并通过。按键与节拍的改动在 BSP 与组合层，主机测试不覆盖这两层，因此另以中断向量表指向和 `MX_TIM2_Init()` 的参数作为核对依据，见[验证记录](verification.md)。

### 文档

- `docs/firmware.md` 外设表补 TIM2，按键一行改为 EXTI；删去「本版按键使用轮询」一节，改写为 EXTI 边沿锁存加 TIM2 节拍的实现说明。README 与外设相关的描述同步。

## `679b859` — 采样故障判定与采样周期下限修复

### 修复

- 第一次采样尝试就失败时不再停留在预热。`sample_seen` 只在采样成功时置位，所以开机起 ADC 就一直失败的系统会永远停在 WARMUP，既不开阀也不报故障——阀门是关的，不构成危险开阀，但故障语义不对。改为 `sample_attempted`：任何一次转换尝试都置位，于是“还没采过”是预热，“采过但失败”是 FAULT 并锁存。
- `GAS_PERIOD_MIN_MS` 由 50 改为 100，与按键可选列表首项一致。原先 50 能被 `gas_config_valid()` 接受，但按键选不到，只有从 EEPROM 载入记录才会生效。
- `alarm_output.h` 里“可在 GPIO 配置前安全调用”的说法不成立：`alarm_output_force_safe()` 写的是 ODR，引脚还是输入时驱动不了。改为明确说明复位到 `MX_GPIO_Init()` 之间靠硬件下拉兜底。

### 新增

- `NMI_Handler` 调用 `alarm_output_force_safe()`，与另外四个异常处理入口保持一致。

### 测试

- 新增 `test_failed_attempt_is_a_fault`，覆盖“开机第一次采样就失败”，并验证它走正常的 KEY4 恢复路径。该用例在旧语义下确认失败。

### 文档

- README 更正 `app/` 的依赖描述：`app/gas` 与 `app/settings` 才是 HAL-independent 的部分，`app.c` 作为组合层持有 BSP 对象并调用 `HAL_GetTick()`，原文“整个 `app/` 只依赖标准 C”不准确。

## `1d8ffb8` — 目录重构与状态机修复

### 修复

- **阈值调到低位后阀门永久锁死。** 恢复判断原先用固定的 100 个计数做滞回（`warn - 100`），而阈值下限是 200，此时要求三路读数都低于 60 才算安全，实物上不可能达到：一旦进入 ALARM 锁存，KEY4 永远不会被接受，只能重新烧录才能恢复。改为按危险阈值的比例判断，安全线 70%、预警线 80%，全量程内安全区都可达。回归用例 `test_low_threshold_recovers` 在旧实现上确认失败、在新实现上通过。
- 首次采样到达之前不再判为采样器故障。上电到第一次转换完成之间采样本来就是空的，旧实现直接进入 FAULT 并锁存，与“预热期保持关阀、预热结束后转入正常”的原意冲突。新增 `sample_seen` 区分“尚未采样”和“采样中断”，只有后者锁存。

### 变更

- 上电并在预热期确认安全后自动开阀，不再需要人工按 KEY4 确认。锁存只由两种事件产生：进入 ALARM，或取得有效采样后采样中断。副作用是断电重启会清除锁存状态，单次运行内“异常解除后不自动开阀”的规则不变。
- 采样周期改为可配置，取值为 100/200/500/1000/2000/5000 ms，由 KEY1 选中、KEY2/KEY3 调整；采样超时随之改为“3 个采样周期”，不再是固定的 300 ms。`app_run` 的调度同样改用该配置，不再硬编码 100 ms。
- 配置记录版本号 1 → 2，启用原先空闲的字节 10～11 存放采样周期。版本 1 的旧记录会被判为无效并回退到默认值，不会误读。记录仍为 16 字节，`[0]=magic`、`[2..3]=序号`、`[4..9]=三个阈值`、`[10..11]=采样周期`、`[12..13]=CRC16`、`[15]=提交标志`，CRC 覆盖范围不变。

### 新增

- `gas_safe_threshold()`、`gas_sample_timeout_ms()`，把安全线和采样超时的换算收进模块，调用方不再各自硬编码。

### 重构

- `bsp/gas_board.c` 按硬件职责拆成四个模块：`mq_sensor`（PA0/PA1/PA4 三路 ADC 顺序采集）、`key`（PB12～PB15 扫描与 30 ms 消抖）、`alarm_output`（继电器、阀门指示、LED、蜂鸣器）、`at24c02`（I2C2 分页读写与 ACK polling）。继电器和蜂鸣器的有效电平集中定义在 `alarm_output.h`。
- `app/runtime/gas_app.*` → `app/app.*`，`app/monitor/` → `app/gas/`，`app/parameters/` → `app/settings/`；存储模块的类型和函数统一改名（`settings_io_t`、`settings_read_fn`、`settings_write_fn`、`settings_load`、`settings_save`）。迁移全部用 `git mv` 完成，文件历史保留。
- `app.c` 增加 `_Static_assert`，编译期校验 `gas_channel_t` 的编号顺序与 `mq_sensor_read()` 的填充顺序一致，防止阈值被静默换到别的传感器上。
- 异常处理路径同步改名：`stm32f1xx_it.c` 的 HardFault/MemManage/BusFault/UsageFault 处理程序由 `gas_board_close()` 改为 `alarm_output_force_safe()`。

### 测试

- `tests/test_gas.c` 拆分为 `tests/test_gas_monitor.c`（状态机：三路各自的预警/报警边界、危险时禁止人工开阀、恢复后维持锁存、连续安全 3 秒后确认、采样失败/越界/超时、新样本不掩盖采样停顿、32 位时间回绕、阈值上下限、采样周期与超时联动、延时保存、首次采样前不误判故障、上电自动开阀、低阈值恢复）和 `tests/test_settings.c`（存储：空 EEPROM、CRC 损坏回退、版本不匹配回退、越界值拒绝、双副本写入各字节处中断后保留旧配置）。

### 文档

- 本提交不含文档改动，README 与 `docs/` 的同步更新在下一条提交。

## `1065fc7` — 文档更新

纯文档提交，不含代码改动。

- `README.md`、`docs/firmware.md` 更新为重构后的目录分层，补充状态机业务规则、外设配置表、参数存储布局和后续实现顺序。
- `docs/verification.md` 记录重构后的 ARM 构建与主机测试结果，并标注尚未验证的硬件项。
- 删除 `docs/` 下一直为空、未使用的 `changelog.md`，本文件随后重建。

## `ac803a9` — 首次实现

### 新增

- STM32F103C8T6 HAL 工程：CubeMX 6.18.0 / STM32CubeF1 V1.8.7，8 MHz HSE → PLL 72 MHz，ADC1 采 PA0/PA1/PA4，I2C2 接 AT24C02，USART1/USART2 已初始化，输出引脚见[固件说明](firmware.md)。CubeMX 相关文件统一放在 `stm32f103/cubemx/`，`GNUmakefile` 独立加入外层 app/bsp 源码。
- 三路气体采集、危险/预警双阈值判断、报警锁存、KEY4 安全恢复、KEY1～KEY3 阈值调整。
- AT24C02 双副本参数存储：magic、版本、序号、阈值、CRC16-CCITT 与提交标志；写前清除另一副本的提交标志，写入按 8 字节页边界拆分并做 ACK polling。
- 主机测试框架：`tools/run_host_tests.py` 编译并运行 `tests/` 下的全部测试，`tools/generate_hal.py` 重新生成 HAL。

## `dfc3881` — 仓库初始化

`.gitignore` 与 MIT LICENSE。手写代码适用 MIT，STM32Cube HAL/CMSIS 保留其供应商许可。
