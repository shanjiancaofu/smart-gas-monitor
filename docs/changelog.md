# 变更记录

按时间倒序记录固件的功能性改动。每条附提交短哈希，完整差异用 `git show <哈希>` 查看。构建与测试结果见[验证记录](verification.md)，分层与业务规则见[固件说明](firmware.md)。

日期：本次新增的各条为 2026-09-16；顶部目录重构一条为 2026-09-15，其余各条为 2026-09-14。

## `841bcc3` — 蜂鸣器改用低电平触发

- 实物用的是**低电平触发**的蜂鸣器模块，而 `BUZZER_ON_LEVEL` 默认是 `GPIO_PIN_SET`。于是固件以为「关」的时候（输出低）正好在让它响，待机时一直叫。默认值改为 `GPIO_PIN_RESET`——驱动本身早就把极性参数化了（`bsp/bsp_buzzer.h` 里一个 `#ifndef`），改的只是默认值。
- 附带修掉上电瞬间的响声：`MX_GPIO_Init()` 会把 PA7 也初始化成低电平，对低电平触发的模块来说那一下就开始响，一直响到 `alarm_init()` 才停。在 `main.c` 的 USER CODE 段里补了一句 `bsp_buzzer_set(false)` 提前摆回空闲电平；放在 USER CODE 里，CubeMX 重新生成不会覆盖。
- **Proteus 原理图里的蜂鸣器是 NPN 三极管驱动，也就是高电平触发，与实物相反。** 同一份固件不可能同时满足两边，仿真里会表现为「待机一直响、报警反而不响」。要么把仿真里的 Q2 换成 PNP，要么给实物加一级 NPN 并把 `BUZZER_ON_LEVEL` 改回去——**这一条还没定**。

## `12eeaf8` — I2C 引脚、总线速度和 UART 上拉

- **I2C1 从重映射的 PB8/PB9 移到默认的 PB6/PB7，并关掉 AFIO 重映射。** 动因是仿真：Proteus 的 STM32 模型不认 I2C1 的引脚重映射，开了之后仿真里 I2C1 仍停在默认引脚上，挂在 PB8/PB9 的 OLED 一个字节都收不到。器件不应答时 `bsp_oled_init()` 失败、`oled.ready` 保持 false、`display_update()` 直接返回，一帧都不画。实物上两种接法都能用，统一走默认引脚就不必维护两套。**实物接线要相应改到 PB6/PB7。**
- **两路 I2C 都回到 100 kHz。** I2C2 之前在 `b2018ad` 里提到 400 kHz 是为了加快历史区扫描，但那会让 Proteus 的存储模型报起始保持时间违规；I2C1 从 400 kHz 退回则是为了让仿真模型跟得上初始化序列。代价是整屏 1024 字节刷新由约 23 ms 变成约 90 ms。
- **两个 UART 的 RX（PA10、PA3）由无上拉改为上拉。** 原先配的是 `GPIO_NOPULL`，链路另一端没驱动时引脚悬空、捡噪声成字节——实测 USART2 的接收环里进来过 58 个噪声字节，其中只要凑巧有一个 `0x0A` 就会被当成一整行交给协议解析，回一条 `ERR UNKNOWN` 出去。

## `636bed1` — 参数页改用小号字体

- 参数页此前只循环 `GAS_COUNT` 画三路通道，而 `gas_item_t` 有 5 项。按 KEY5 切到采样周期或蜂鸣器档位时屏幕上没有任何标记：那两项只能盲调，改完要重新上电或在串口上发 `CONFIG?` 才知道改成了什么。
- 根因是行数放不下。大号字体一行占两个 page，8 个 page 只有 4 行，标题加 5 个可调项需要 6 行。改用小号字体（6x8），一行一个 page，6 行富余——`bsp_oled_text` 本来就带 `large` 参数、小号字模也一直在驱动里，只是 display 层一律传了 `true`。
- 行号直接由 `gas_item_t` 推出（标题占 0，之后每项一行），不再另外写死，免得以后加可调项时又漏画。
- `tests/test_display.c` 补了回归断言。原来的用例只断言 `gas.item` 变了，**从没真正画过参数页**，所以这个 bug 在七套主机测试全绿的情况下一直存在。新断言在旧 `draw_settings` 上确认会失败。
- 同一个文件里还有一条更早的问题：报警页断言出现 `"KEY4"`，那是报警页上还没有通道列表时的写法，KEY4 现在走 SAFE_WAIT 那条路径提示。这条断言让整个测试套在它那里中断，后面的用例一个都跑不到——`main` 分支上 `test_display` 一直是红的。一并改成断言实际画出来的内容。

## `a65b453` — 演示用阈值与预热时长

- 三路默认阈值统一为 2400。此前 MQ6 是 2000，而板上不接 MQ 时 ADC 引脚浮空、停在 3.3 V 中点约 2048，空载必然报警。
- 预热由 60 秒缩短到 3 秒。真实 MQ 传感器要热机几分钟以上才稳定，那个时长对课设演示没有意义。`GAS_WARMUP_MS` 与同为 3000 的 `GAS_SAFE_HOLD_MS` 数值相同但含义无关：一个是开机等待，一个是恢复判定要求的连续安全时长。
- 测试里 99 处绝对时刻原先是照着 60 秒预热写死的（63000 那一族）。预热一改，这些时刻就落到采样超时窗口之外，被 `gas_sample()` 读成采样中断判 FAULT，而不是用例要看的状态变化。改为从 `GAS_WARMUP_MS + GAS_SAFE_HOLD_MS` 推出的 `T0`，此后改预热时长测试跟着走；回绕用例的起点同样重选，使回绕落在预热结束之后、这一段跑完之前，不再名不副实。

## `b2018ad` — 缺 EEPROM 时不再把启动卡住

- 只接 OLED 时上电到屏幕点亮要 13.3 秒。`history_init` 要扫 510 个历史槽位，EEPROM 不在时每次读都等满 HAL 超时（约 26 ms），510 × 26 ≈ 13.2 秒，而 `bsp_oled_init` 排在它之后。
- 扫描循环把「器件不应答」和「槽位 CRC 不过」合并在同一条 `continue` 里。后者只是这个槽位没内容，该继续看下一个；前者说明 EEPROM 不在或接触不良，后面五百多个槽位只会把同样的超时再等一遍，改为直接收工。**传输超时值未改**，改的是失败之后还要不要再问下一个槽位。
- 面板初始化提到 EEPROM 操作之前，屏幕不再跟着一起等；I2C2 由 100 kHz 提到 400 kHz（AT24C64 支持），历史区扫描随之加快。
- 实测（ST-Link 在目标板上读 `uwTick`）：`bsp_oled_init` 由 t=13271 ms 变为 t=0 开始、26 ms 画完——那 26 ms 是整屏 1024 字节的首次刷屏，400 kHz 下约 23 ms；主循环由 13296 ms 变为 48 ms。

## `1f8fcc0` — 换掉损坏的 OLED 字模

- `bsp_oled.c` 里的 `font6x8`/`font8x16` 是噪点而不是字形：8x16 的 'A' 解码出来是散落的点，'0' 同样。display 层的文字全部走大号字体，因此整屏都是花的。
- 改用参考工程标准库的 `asc2_1608`/`asc2_0806`，按本工程布局重排：索引 `ch-32`，8x16 每字形 16 字节（先上半页 8 列、再下半页 8 列），bit 0 是该页最上面的像素。生成脚本 `tools/gen_oled_font.py` 一并提交，字模要改就重跑它。
- 验证办法是在目标板上把 SSD1306 的 1024 字节帧缓冲读回来、逐格与字模表比对，实时页还原出 `MQ4 2105/2400` 等，每格位差为 0。同一套办法后来用于确认参数页，脚本为 `tools/render_oled.py`。

## `d8b819d` — 面板对象移交组合层

- `display` 原先内嵌 `bsp_oled_t` 并替它调 `bsp_oled_init()`，接口上收一个 `I2C_HandleTypeDef *`——display 模块里唯一具名的 HAL 类型。现在面板对象由 `app_t` 持有（和 sensor、eeprom、keys、两个 uart 一致），`display_init()` 收对象不收句柄，自己不再初始化硬件。
- `bsp_oled_t` 增加 `ready` 字段，由 `bsp_oled_is_ready()` 查询；`display_t` 不再留第二份副本，`display_update()` 直接问 BSP。
- **这不等于 display 可以主机测试。** HAL 头仍经 `bsp_oled.h` 传递包含（`bsp_oled_t` 里存着 I2C 句柄），改动去掉的是 display 接口上的 HAL 类型，不是 HAL 依赖本身。
- 改完之后全 app 层只剩 `app.c` 具名 HAL 类型，即取六个 CubeMX 句柄的那几行。

## `079fbaa` — 按业务和硬件职责整理

- app 按 gas、alarm、config、history、protocol、display 分工；main 改为无参数 app_init/app_update，应用对象在 app.c 内部。
- BSP 使用 bsp_ 前缀。ADC 只读硬件通道，MQ 映射及八次平均归 gas；蜂鸣器计时归 alarm，LED/蜂鸣器/继电器各有独立 GPIO 驱动；OLED 和 EEPROM 共用 bsp_i2c。
- gas/config/BSP 内部 API 随模块名统一；历史接口为 history_add/get/clear。串口命令、引脚、EEPROM v5 布局保持原有格式。
- app_update 按顺序调用调度函数；关阀输出先于持久化写入。新增历史清除仅为 C 接口，未增加远程命令。
- 改名对照：`app/gas/gas_monitor.*` → `app/gas/gas.*`，`app/settings/*` → `app/config/*`，`app/communication/*` → `app/protocol/*`，`bsp/*` → `bsp/bsp_*`。测试同步改名 `test_gas_monitor.c` → `test_gas.c`、`test_settings.c` → `test_config.c`，新增 `tests/test_alarm.c`。查更早的提交时按这张表换算旧名。
- 三处不是搬移、需要单独核对的行为：八次平均由 BSP 移到 gas（`bsp_adc_read()` 单路读取）；蜂鸣器计时窗口由 BSP 移到 `app/alarm/`；`app.c` 改为持有唯一实例的单例，但其下各模块仍是显式传参，主机测试因此一行断言未改。核对记录见[验证记录](verification.md)。
- 旧的两条跨层 `_Static_assert` 随重复定义一起消失：`APP_TICK_MS` 直接定义为 `GAS_TICK_MS`，蜂鸣器哨兵值只剩 `config/config.h` 一处。`GAS_BUZZER_MAX_S * 1000u < GAS_BUZZER_FOREVER_MS` 保留。
- 统一中文注释、四空格和花括号，补充 ADC 平均、转换失败及报警窗口的主机测试。CubeMX 初始化和供应商文件不变，仅调整 USER CODE 入口及手写 GNUmakefile。

## `0d31c62` — 报警蜂鸣器时长可设置

**这一条推翻了 `fe153fd` 的判断。** 那一版把蜂鸣器改成响固定 5 秒后自动停，并写明「这个时间不做成可配参数：它的职责是让人注意到，不是给人调的」。现在的结论是：让人注意到是必须的，响多久是使用者的选择，两件事不必是同一件事。

### 变更

- 蜂鸣器时长成为配置项，三个取值排在同一条数轴上：0 为「不响」、1～60 为秒数、61（`GAS_BUZZER_ALWAYS`）为「一直响到报警解除」。默认 5 秒，与这项设置存在之前的行为完全一致，升级不会让现场表现发生变化。
- KEY1 的循环里增加「蜂鸣器时长」一页，排在采样周期之后、历史记录之前；KEY2/3 每档 1 秒，两端夹取不环绕——「加」永远意味着「响得更久」，撞到顶就停在 `ALWAYS`，不会绕回 `OFF`。
- 串口新增 `SET BUZZER <OFF|ALWAYS|1-60>`，`CONFIG?` 追加 `BUZZ=`。两个特殊档位是词不是数：接口上因此不留下 61 这个魔法数。秒数后的 `S` 可有可无，`CONFIG?` 打印出来的值可以原样敲回去；回显的也是名字而不是数字。
- `gas_buzzer_duration_ms()` 负责「配置值 → BSP 要的毫秒数」，放在 `gas_monitor.c` 而不是 `app.c`：换算本身是安全相关的一环，而 `app.c` 和 `alarm_output.c` 都不在主机测试里。越界的值一律读作 `ALWAYS`，绝不回绕成一个很短的时长——报警响了半秒就安静，是这条路径上最不该出现的失败方式。
- `alarm_output_apply()` 的蜂鸣器时长由编译期常量改为运行期参数，`BUZZER_ALARM_TICKS` 换成 `BUZZER_FOREVER` 哨兵和 `BUZZER_TICK_MS`。跨层的哨兵值由 `app.c` 里的 `_Static_assert` 钉住（BSP 不能反向依赖 app），另有 `_Static_assert(GAS_BUZZER_MAX_S * 1000u < GAS_BUZZER_FOREVER_MS)` 保证日后调大上限时先编译不过，而不是让「70 秒」和「一直响」两个含义悄悄重合。
- 配置格式升级到 v5：sequence 由 2 字节压成 1 字节，腾出的 byte 3 给蜂鸣器时长，其余偏移与 CRC 覆盖长度都与 v4 一致。加字段是必须的——两个 16 字节槽都已排满，历史区 0x20～0xFF 又被 14 条记录正好占满，整片 AT24C64 没有空余字节。sequence 只用于判断两槽新旧、逐次 +1 递增、按模比较，8 位与 16 位结果相同。**v4 及更早的记录会被判为无效并回退默认配置**，阈值和采样周期也一并回落——这是有意为之，见下。
- 改蜂鸣器时长只置 dirty，不重启「低于危险阈值多久算恢复」那个 3 秒窗口；阈值和采样周期照旧重启。`config_changed()` 因此拆成 `mark_dirty()` 与 `config_changed()` 两个函数：那个窗口拦的是 KEY4，不该让一个不参与限值判断的字段拖住站在面板前的人。
- 参数页重排：`STORE` 与 `ALARMS` 并成一行，腾出的第八行给蜂鸣器档位。八行是面板的物理上限，没有页内翻页可用。

### 为什么必须挡住 v4

两个布局只差字节 2 和 3，其余偏移全都对得上，少了版本检查，v4 的记录会「几乎读对」：byte 3 在 v4 里是序号的高字节，从 0 开始的序号高位是 0，读成蜂鸣器时长恰好是 0——也就是把报警蜂鸣器静默关掉，而 CRC 和其余字段全部通过校验。静默失效的报警输出比读不出来的配置危险得多，所以这一版宁可让旧记录整体作废。

### 测试

- `test_gas_monitor.c` 新增三组：取值到毫秒与名字的换算（含越界值读作 `ALWAYS`、缓冲区截断不越界）、按键逐档与两端夹取、setter 的范围拒绝与安全窗口的取舍。最后一组把两个方向都断言了——改蜂鸣器时长后 `reset_ready` 保持，改阈值后必须清零。
- `test_settings.c` 的往返现在带着蜂鸣器一起比对，并新增两处字节级校验：蜂鸣器字节损坏使该槽作废（回退到仍是默认 5 秒的那份），v4 版本的记录被拒绝。逐字节掉电矩阵不变，仍是 17 个中断点全部还原旧配置。
- `test_protocol.c` 新增 `SET BUZZER` 的成功与四类拒绝，以及 `CONFIG?` 的逐字节新格式。

### 文档

- `firmware.md` 记录三个档位的编码、换算函数、安全窗口的取舍、v5 布局与「为什么是压缩序号而不是扩大槽」，串口协议表补上 `SET BUZZER`。
- `verification.md` 记录本轮构建与测试结果。
- Proteus 演示脚本新增第 12 步（改蜂鸣器档位），并把「蜂鸣器响 5 秒后自动停」的三处说法改掉——其中第八节那条排查建议在 `ALWAYS` 档下会导出错误结论。

## `f8432ed` — 故障处理入口让蜂鸣器静默

`alarm_output_force_safe()` 只驱动继电器和阀门指示灯，不碰蜂鸣器。它被 `Error_Handler` 和五个 fault handler 调用，而那些路径之后 `alarm_output_apply()` 不再运行，蜂鸣器引脚就停在最后写入的电平上——HardFault 若正落在响铃窗口里，它会一直响到复位为止。现在把蜂鸣器也收进这个函数，并删掉 `alarm_output_init()` 里那句因此变得多余的写入。

这个缺陷在改动前就存在，5 秒窗口一样会踩到；但「一直响」档把它从偶发变成常态，而「引脚停在某个电平上」正是这个函数要消灭的东西。代价要说清楚：它让硬件故障时的现场变安静，而蜂鸣器恰恰是「让人注意到」的那个输出。这是取舍，所以单独一个提交，单独可以否决。

## `1a2a923` — 持久化阀门锁存与软件冻结收尾

- 修复 SSD1306 小字体行缓冲越界；`app_init()` 检查 TIM2 启动结果；历史写入失败反馈到 `storage_ok`；蜂鸣器 5 秒窗口改用 TIM2 的 500 个 10 ms 节拍。
- 配置格式升级到 v4：byte 12 保存 lockout，byte 13～14 保存覆盖 byte 0～12 的 CRC16，commit 仍在 byte 15。lockout 损坏会使该槽失效并回退另一有效槽。
- ALARM、FAULT、远程 `VALVE CLOSE` 共用锁存路径，KEY4 共用清除路径。只有 lockout 边沿会置 dirty；组合层在边沿立即保存，失败后走原有 2 秒重试，持续 ALARM/FAULT 不重复磨损 EEPROM。
- 历史记录 CRC 失败时清空 OLED 剩余行，避免残留上一条记录。
- 新增回归覆盖 lockout CRC、掉电恢复、持续 ALARM/FAULT 不重复 dirty、重复远程 CLOSE 不重复 dirty。四套 Host Test 和无缓存 ARM 构建通过。

## `8fdcf1f` — 双串口文本协议

### 变更

- 配置写入由按键路径独占改为共用一套记账逻辑。`gas_monitor_set_threshold()` 与 `gas_monitor_set_period()` 先克隆当前配置、只改目标字段、再用 `gas_config_valid()` 整体校验，最后走与按键相同的 `config_changed()`。只校验新字段的话，两次分别合法的远程写入可以叠加出一份开机能存但读不回来的配置。
- `gas_config_valid()` 增加「采样周期必须是 `GAS_TICK_MS` 的整数倍」。255 ms 能被调度成 250 ms，而面板和串口都还在报 255。
- 串口报警通知复用 `app_update()` 里已有的报警边沿判断，与写历史记录共用同一处 `last_state` 比较，不新增第二份「什么是新报警」的判据。
- `display.c` 里的 `state_name[]`、`channel_name[]` 两张表和 `mask_name()` 上移到 `gas_monitor.c`，成为 `gas_channel_name()`、`gas_state_name()`、`gas_alarm_mask_name()`。两个消费者（面板和串口）本来各要一份同样的名字，放在状态机旁边就只有一份。

### 新增

- `app/communication/protocol.*`：状态机式的逐行应答生成器，`protocol_next()` 每次取一行，取完返回 NULL。不包含 HAL 头文件，可在主机上测试。
- `bsp/serial.*`：两路中断收发的行缓冲。TX/RX 环形缓冲长度都是 2 的幂，用掩码代替取模；`serial_write()` 要么整行入队要么拒绝，不做部分写入。
- `HAL_UART_RxCpltCallback()`、`HAL_UART_TxCpltCallback()`、`HAL_UART_ErrorCallback()`，按 `uart->Instance` 在两张口的注册表里分发。错误回调清 ORE/FE/NE/PE 后重新武装接收——无线链路的一个坏字节不该让这个口从此沉默。
- 命令集：`STATUS?`、`CONFIG?`、`HISTORY?`、`SET MQ4|MQ6|MQ6 <值>`、`SET PERIOD <毫秒>`、`VALVE CLOSE`，行以 `\r\n` 结束，大小写不敏感。报警开始时在两路主动推一行 `ALARM`。
- `.ioc` 中使能 `USART1_IRQn` 与 `USART2_IRQn`（抢占优先级 2），同样是第 7、8 字段必须为 `true:true`。

### 修复

- **一次 `HISTORY?` 会把自己打成 FAULT。** 14 条记录约 750 字节，HC-05 在 9600 baud 下要 780 ms 才发得完，比短周期下采样器自己的故障超时（3 × 100 ms）还长。就地阻塞发送的结果是：查询历史这个动作本身触发采样超时、进入 FAULT、关阀。改为中断发送，并且 `pump_replies()` 只在两路都空闲时才取一行，协议对主循环的开销小到看不出来。

### 测试

- 新增 `tests/test_protocol.c`：查询应答逐字节比对、大小写不敏感、空行不回话、远程设置的范围与类型拒绝、`VALVE CLOSE` 之后仍须 KEY4 才能开阀、`VALVE OPEN` 恒回 `ERR ONLY CLOSE`、历史记录倒序与序号、报警主动通知只发一次、超长行不越界且之后协议仍可用。

### 文档

- `docs/firmware.md` 增加串口协议一节与两路串口的中断收发说明；命令表、`SET` 的校验路径和「没有远程开阀」的规则写入业务规则。README 同步。

## `4d8c404` — OLED 三界面与 EEPROM 报警记录

### 新增

- `bsp/ssd1306.*`：128×64 面板的缓冲、字模与分页刷新。控制字节 0x00 为命令、0x40 为数据，7 位地址 0x3C 左移成 0x78。用页脏位图记录哪些页变了，刷新时只发变化的页，因此每 200 ms 一次的界面刷新不会把 I2C1 占满。字符集为 `font6x8`（21 列）与 `font8x16`（16 列）两套列优先点阵。
- `app/display/display.*`：三个界面。实时页显示三路 ADC 值/阈值、`NORMAL/WARNING/ALARM` 与 `VALVE OPEN/CLOSED`；参数页显示可调项、存储状态与累计报警次数；历史页浏览记录。每行都按满宽补齐，数值变短时能盖住上一次的残留字符。
- 历史页上的 KEY2/KEY3 改为翻阅记录，在 `app_update()` 的按键分发里截获，不进入参数处理逻辑——历史页没有可调项，否则这两个键会去改最后选中的那个参数。
- `app/history/history.*`：0x20～0xFF 的 14 条 16 字节环形记录，含 16 位序号、上电秒数、三路 ADC 值、报警通道掩码和 CRC16。序号由记录本身携带并在启动时扫描得出，重启后接着往下编。

### 变更

- `app_update()` 增加报警边沿检测：只在**首次进入 ALARM** 时写一条记录。状态机决定报警何时开始，上一次的状态用来区分「新报警」和「报警还在持续」，后者不重复写。
- `history_append()` 先取槽位再写入、成功后才推进游标，写失败的记录会在同一个槽位重试，而不是留下一个空洞。

### 测试

- 新增 `tests/test_history.c`：区域边界、空存储、写入与重启后顺序、环形回绕、写入中断留下的残槽、CRC 损坏记录被丢弃。

### 文档

- 时间来源说明：没有 RTC，记录里存的是上电秒数，不是它无从知道的日期时间。

## `fe153fd` — ADC 八次平均与蜂鸣器限时

### 变更

- 每路 ADC 连续转换 8 次取平均（`MQ_SENSOR_AVERAGES`）。MQ 模块的输出带市电纹波和加热丝开关噪声，单次转换不是一个稳定的读数，而且一次转换失败即判定整组无效；滤波放在 BSP 里，状态机只看到平均值。
- 报警的蜂鸣器改为响固定 5 秒后自动停（`BUZZER_ALARM_MS`），红灯和关阀保持不变。这个时间不做成可配参数：它的职责是让人注意到，不是给人调的。
- 全部 GPIO 通过 CubeMX 配置 User Labels，`main.h` 里生成 `*_Pin` / `*_GPIO_Port` 宏，BSP 不再直接写 `GPIOA`/`GPIO_PIN_8` 这类字面量。引脚改名只需改 `.ioc`。
- `key.c` 增加一条 `_Static_assert`，钉住 KEY1～KEY4 就是 PB12～PB15 这半个字节。`key_bits()` 一次读整个半字节，重排引脚会让断言失败，而不是把 KEY1 悄悄映射到别的位上。
- I2C1 由 100 kHz 提到 400 kHz。面板整屏刷新 1024 字节，100 kHz 下要 90 ms 以上，会把主循环拖长到影响采样节拍。

### 修复

- 蜂鸣器原先由报警状态直接驱动，报警持续期间一直响。改为按报警边沿计时，持续报警不重新计时，只有新的报警才重新响。

### 测试

- 无新增用例，本轮改动在 BSP 与 CubeMX 配置层，主机测试不覆盖这两层。核对项为生成后的 `MX_TIM2_Init()` 参数、GPIO User Label 宏与 I2C1 时钟频率，见[验证记录](verification.md)。

### 文档

- 无。

## `fe84791` — 按键 EXTI 与 TIM2 采样节拍

### 修复

- `.ioc` 中 `NVIC.EXTI15_10_IRQn` 的第 7、8 字段由 `false:false` 改为 `true:true`。原写法下 CubeMX 照样生成中断处理函数和 `HAL_NVIC_SetPriority()`，但**不生成 `HAL_NVIC_EnableIRQ()`**，中断永远不会触发。
- `.ioc` 补上虚拟引脚 `VP_TIM2_VS_ClockSourceINT`。`Enable_Timer` 模式要求 `VS_ClockSourceINT` 信号，缺了它 CubeMX 在 `config load` 阶段静默丢弃整个 TIM2，日志里没有任何提示。

### 变更

- `app_update()` 的采样调度由「和 `HAL_GetTick()` 比差值」改为按 `sample_period_ms / 10` 个 TIM2 节拍触发，并按整周期推进游标，周期不再随主循环抖动漂移；主循环停顿超过一个整周期时重新对齐。`HAL_GetTick()` 仍是状态机的时间基准，超时、运行时长和消抖都用它。
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
- 采样周期改为可配置，取值为 100/200/500/1000/2000/5000 ms，由 KEY1 选中、KEY2/KEY3 调整；采样超时随之改为“3 个采样周期”，不再是固定的 300 ms。`app_update` 的调度同样改用该配置，不再硬编码 100 ms。
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

- STM32F103C8T6 HAL 工程：CubeMX 6.18.0 / STM32CubeF1 V1.8.7，8 MHz HSE → PLL 72 MHz，ADC1 采 PA0/PA1/PA4，I2C2 接 AT24C64，USART1/USART2 已初始化，输出引脚见[固件说明](firmware.md)。CubeMX 相关文件统一放在 `stm32f103/`，`GNUmakefile` 独立加入外层 app/bsp 源码。
- 三路气体采集、危险/预警双阈值判断、报警锁存、KEY4 安全恢复、KEY1～KEY3 阈值调整。
- AT24C64 双副本参数存储：magic、版本、序号、阈值、CRC16-CCITT 与提交标志；写前清除另一副本的提交标志，写入按 32 字节页边界拆分并做 ACK polling。
- 主机测试框架：`tools/run_host_tests.py` 编译并运行 `tests/` 下的全部测试，`tools/generate_hal.py` 重新生成 HAL。

## `dfc3881` — 仓库初始化

`.gitignore` 与 MIT LICENSE。手写代码适用 MIT，STM32Cube HAL/CMSIS 保留其供应商许可。
