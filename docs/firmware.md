# 固件分层与开发说明

## 工程来源

本工程由本机 STM32CubeMX 6.18.0（安装日志标识 RC3）实际生成，固件包为 STM32CubeF1 V1.8.7，目标 STM32F103C8T6。参考课设 MDK-GPIO 标准库例程及 chassis-controller 的应用/驱动边界、故障锁存和主机测试方式，未移植底盘的电机、RTOS、OTA 或 G474 配置。

## 分层

| 目录 | 职责 | 依赖 |
| --- | --- | --- |
| cubemx/Core | CubeMX 初始化、启动入口、中断 | HAL；USER CODE 调用 app |
| cubemx/Drivers | ST HAL、CMSIS | 供应商代码 |
| bsp/mq_sensor | PA0/PA1/PA4 三路 ADC 顺序采集、8 次平均与校准 | HAL ADC |
| bsp/key | PB12～PB15 的 EXTI 边沿锁存、扫描与 30 ms 消抖 | HAL GPIO |
| bsp/alarm_output | 继电器、阀门指示、LED、蜂鸣器计时 | HAL GPIO |
| bsp/at24c02 | I2C2 分页读写与 ACK polling | HAL I2C |
| bsp/ssd1306 | 128×64 面板的缓冲、字模与分页刷新 | HAL I2C |
| bsp/serial | 中断收发的行缓冲，收发都不阻塞主循环 | HAL UART |
| app/gas | 阈值、状态机、锁存和安全确认 | 标准 C，不依赖 HAL |
| app/settings | 配置序列化、CRC16、双副本提交 | 配置类型与读写回调，不依赖 HAL |
| app/history | 报警记录的环形存储、序号与 CRC | 读写回调，不依赖 HAL |
| app/display | 实时、参数、历史三个界面 | ssd1306、gas、history |
| app/communication | 文本协议解析与逐行应答 | gas、history，不依赖 HAL |
| app/app.c | 初始化装配、采样调度、按键分发、输出、延时保存与协议节流 | 全部下层 |

main.c 的 USER CODE 只保存应用对象并调用 `app_init`、`app_run`，把 `&hadc1`、`&hi2c2`、`&hi2c1`、`&htim2`、`&huart1`、`&huart2` 六个句柄交给应用层。GPIO 初始化由 CubeMX 负责。`app.c` 把业务状态转换成 BSP 的布尔量，BSP 不接收业务状态机对象。

`mq_sensor_read()` 按 MQ4、MQ7、MQ8 顺序填充数组，`gas_channel_t` 用同样的顺序编号；`app.c` 里的 `_Static_assert` 保证两者一致，否则阈值会被悄悄换到别的传感器上。键盘同理：`key_bits()` 一次读整个 PB12～PB15 半字节，`key.c` 用 `_Static_assert` 钉住这个对应关系。

`app/gas`、`app/settings`、`app/history`、`app/communication` 四个模块不包含 HAL 头文件，因此可以在主机上直接编译测试。

## 外设与时序

| 外设 | 配置 |
| --- | --- |
| 时钟 | 8 MHz HSE → PLL 72 MHz；APB1 36 MHz；ADC 12 MHz |
| ADC1 | PA0/PA1/PA4，单次软件触发，239.5 cycles；每路连续转换 8 次取平均 |
| TIM2 | 72 MHz 定时器时钟，预分频 71、周期 9999，10 ms 更新中断，中断里只累加节拍 |
| I2C1 | PB6/PB7，400 kHz，SSD1306（7 位地址 0x3C） |
| I2C2 | PB10/PB11，100 kHz，AT24C02 默认 7 位地址 0x50 |
| USART1 | PA9/PA10，115200，8-N-1，接 USB-TTL |
| USART2 | PA2/PA3，9600，8-N-1，接 HC-05 |
| 按键 | PB12～PB15，上拉输入，EXTI 下降沿中断，主循环 30 ms 消抖 |
| 输出 | PB8 绿、PB9 黄、PA6 红、PA7 蜂鸣器、PA8 继电器、PA5 阀门指示 |
| 调试 | PA13/PA14 SWD，关闭 JTAG |

按键走 EXTI 下降沿中断（`EXTI15_10_IRQn`，抢占优先级 0），HAL 生成的处理函数逐个调用 `HAL_GPIO_EXTI_Callback()`，由 `bsp/key.c` 实现：只把「哪一路动了」记进待处理位，消抖和分发仍留在主循环。中断里不碰 ADC、I2C、串口和 EEPROM。电平仍由主循环采样——消抖判断的就是电平，边沿只用来给这个窗口打时间戳，所以一次在两次轮询之间按下又松开的抖动不会变成一次按键。主循环被 EEPROM 写阻塞期间按下的键不会丢：边沿已经锁存，写完之后照常进入消抖。上电已按住的按键必须释放后重按才能产生事件。

采样节拍由 TIM2 的 10 ms 中断提供（`TIM2_IRQn`，抢占优先级 1），中断里只对 `app_ticks` 加一，转换、显示和 EEPROM 全部留主循环。`app_run` 按 `sample_period_ms / 10` 个节拍触发一次采样，并按整周期推进游标，使周期不因主循环抖动而漂移；主循环停顿超过一个整周期时重新对齐。`HAL_GetTick()` 仍是状态机的时间基准，超时、运行时长和消抖都用它，节拍只负责调度。`gas_config_valid()` 因此要求采样周期是 10 ms 的整数倍：不是整数倍的周期会被按四舍五入后的节拍数调度，而面板和串口都还在报原值。

每路 ADC 连续转换 8 次取平均（`MQ_SENSOR_AVERAGES`）。单次转换的读数在相邻采样周期间能差几十个计数，足以让读数停在阈值上下反复越界，滤波放在 BSP 里，状态机只看到平均值。

两路串口都是中断收发（`USART1_IRQn`/`USART2_IRQn`，抢占优先级 2）。发送不做阻塞等待是有原因的：14 条历史记录在 9600 baud 下要 780 ms 才发得完，比短周期下采样器自己的故障超时还长，就地阻塞发送会把一次查询变成一次 FAULT 并关阀。改为逐行应答、且只在两路都空闲时才发一行，协议对主循环的开销小到看不出来。收发出错（帧错误、溢出）时在错误回调里重新武装接收：无线链路的一个坏字节不该让这个口从此沉默。

`alarm_output.h` 集中定义继电器和蜂鸣器的有效电平。继电器默认 PA8 高电平表示允许开阀，低电平关闭。更换极性时，必须同时通过 CubeMX 调整 GPIO 上电默认输出，保证初始化时关闭阀门；复位后到 `MX_GPIO_Init` 之间 PA8 是浮空输入，硬件需要有下拉，否则这段窗口的继电器状态不可控。接点和电平需按实物确认；PA5 仅显示软件开阀命令，没有物理阀位反馈。

## 业务规则

- 三路演示危险阈值为 2400、2000、2400 ADC counts，分别对应 MQ-4、MQ-7、MQ-8；预警阈值为危险阈值的 80%。等于危险阈值即报警，任一路即可关阀。
- 上电默认预热 60 秒并保持关阀；该时间是课设演示设置，不能替代 MQ 模块实际预热/标定要求。预热期间不做报警抑制，高浓度同样进入 ALARM 并锁存——预热期阀门本来就是关闭的，这里的取舍是宁可多报。
- 正常上电且确认安全后自动开阀，不需要人工确认。锁存只由两种事件产生：进入 ALARM，或已取得有效采样后采样中断。此后必须三路均低于危险阈值的 70% 并持续 3 秒，KEY4 才能重新开阀。上电时没有 KEY4 可按的场合不会因此卡住。
- ADC 调用失败、数值越界或采样间隔达到 3 个采样周期，进入 FAULT 并锁存关阀。区分“还没采过”和“采了但失败”：第一次采样尝试之前处于预热，不锁存；此后任何一次失败都算故障。新样本不能掩盖之前的采样中断。
- 报警时蜂鸣器响 5 秒后自动停（`BUZZER_ALARM_MS`），红灯和关阀保持不变。这个时间固定不可配：它的职责是让人注意到，不是给人调的。报警持续期间不会重新计时，只有新的报警才重新响。
- KEY1 在实时界面、MQ-4、MQ-7、MQ-8、采样周期、历史记录之间循环；KEY2/3 调整选中项，阈值步长 50、范围 200～4000，采样周期在 100/200/500/1000/2000/5000 ms 之间切换。KEY4 只用于安全恢复确认。历史界面没有可调项，那里的 KEY2/3 改为翻阅记录，不会传到参数处理逻辑里。
- 修改停止 2 秒后保存；失败保留 dirty 标志，每 2 秒重试。运行对象 storage_ok 表示最近加载/保存是否成功。空 EEPROM 首次采用默认值，用户修改后保存。
- 报警输出优先于 EEPROM 操作。EEPROM 驱动使用有超时的阻塞事务。NMI、HardFault、MemManage、BusFault、UsageFault 五个异常处理入口都会调用 `alarm_output_force_safe()`，但它写的是 ODR，引脚仍是输入时驱动不了，这一窗口由硬件下拉兜底。当前尚未加入独立看门狗，主循环挂死不能单靠采样超时逻辑关阀。

## 参数存储

AT24C02 共 256 字节，分两段：0x00～0x1F 是配置，0x20～0xFF 是报警历史。

配置占 0x00～0x0F 和 0x10～0x1F 两个 16 字节槽：magic、版本、16 位序号、三个小端阈值、采样周期、CRC16-CCITT 和提交标志。先清除另一槽提交标志，再写正文，最后提交并读回校验。底层按 8 字节页边界拆分并 ACK polling。

版本号当前为 2；版本 1 的记录没有采样周期字段，会被判为无效并回退到默认值，不会误读。加载时验证 CRC、版本、阈值范围和采样周期范围，选择有效且较新的副本。

历史区 224 字节正好是 14 条 16 字节记录，每条含 16 位序号、上电秒数、三路 ADC 值、报警通道掩码和 CRC16。槽位按环形复用，写满后覆盖最旧的一条而不是停止记录。序号由记录本身携带并在启动时扫描得出，所以重启之后接着往下编，不会把已经报过的报警算漏。

记录只在**首次进入 ALARM** 时写一条：状态机决定报警何时开始，上一次的状态用来区分“新报警”和“报警还在持续”，后者不会重复写。没有 RTC，记录里存的是当时的上电秒数，而不是它无从知道的日期时间。CRC 不符的记录在下次启动时直接丢弃，不会用残缺的数值冒充一条记录。

## 串口协议

USART1 和 USART2 走同一套文本协议，任一路能做的事另一路也能做，不存在只有蓝牙才能用的指令。行以 `\r\n` 结束，命令大小写不敏感，空行不回话。

| 命令 | 应答 |
| --- | --- |
| `STATUS?` | `STATE=… VALVE=… ALARMS=…` 与 `MQ4=值/阈值 …` 两行 |
| `CONFIG?` | `TH MQ4=… MQ7=… MQ8=… PERIOD=…` |
| `HISTORY?` | `HISTORY 条数/14` 加每条一行，最新的在前 |
| `SET MQ4\|MQ7\|MQ8 <值>` | `OK MQ4=2500`，越界回 `ERR RANGE` |
| `SET PERIOD <毫秒>` | `OK PERIOD=500` |
| `VALVE CLOSE` | `OK VALVE=CLOSED` |

报警一旦开始，会在两路上主动推一行 `ALARM MQ4 MQ4=… MQ7=… MQ8=…`，不需要轮询。

**没有远程开阀。** `VALVE OPEN` 一律回 `ERR ONLY CLOSE`。`VALVE CLOSE` 只是把阀门锁存到关闭，解除锁存的唯一途径仍是面板上的 KEY4，且与报警后恢复的条件完全相同：三路都要低于危险阈值的 70% 并持续 3 秒。

远程 `SET` 不直接改配置结构，而是走 `gas_monitor_set_threshold()` / `gas_monitor_set_period()`，复用按键路径那套记账逻辑，并且只接受设置解码器同样会接受的值——否则远程写入可以造出一份开机能存但读不回来的配置。

## 生成与构建

在 CubeMX 打开 `stm32f103/cubemx/smart_gas_monitor.ioc`，生成器选 Makefile，保留 USER CODE。CubeMX 相关工程文件统一放在 `cubemx/`，app、bsp 保持在外层。`cubemx/Makefile` 由工具管理，`cubemx/GNUmakefile` 独立加入手写源码，直接在该目录构建。不要使用 make -f Makefile 跳过应用层。产物输出到 `stm32f103/cubemx/build/`。

```powershell
make -C stm32f103/cubemx -j4
```

`GNUmakefile` 把目标文件压平到同一个目录，因此 app 和 bsp 下的源文件不能重名。

IOC 使用默认固件包位置，在 CubeMX 中安装或选择 STM32CubeF1 V1.8.7 即可；不依赖本机绝对路径。命令行接口可参考 [ST STM32CubeMX 用户手册](https://www.st.com.cn/resource/zh/user_manual/um1718-stm32cubemx-for-stm32-configuration-and-initialization-c-code-generation-stmicroelectronics.pdf)。

仓库内的 `tools/generate_hal.py` 可直接重新生成 HAL，日志保存在 `build/cubemx-generate.log`：

```powershell
python tools/generate_hal.py --cubemx E:/STM32CubeMX --cube C:/Users/FFZNB/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7
```

构建需要 GNU Make、arm-none-eabi-gcc、objcopy 和 size。产物为 HEX/BIN/ELF，Flash 起始地址 0x08000000，容量 64 KB，RAM 20 KB。

主机测试不依赖 HAL，也不需要交叉工具链：

```powershell
python tools/run_host_tests.py --cc gcc
```

Windows 可在 Visual Studio x64 Native Tools 命令行运行 `python tools/run_host_tests.py --cc cl`。脚本自动编译并运行 `tests/` 下的每个测试文件，测试产物不纳入版本控制。

## 后续实现

软件部分已完成。下一步是 Proteus 仿真（三路电位器模拟 MQ 输入，走通正常→预警→报警→红灯/蜂鸣器/继电器关阀→串口报警→EEPROM 记录→浓度恢复但阀门保持关闭→KEY4 恢复的完整链路），随后是原理图、PCB、实物与课设报告。PCB 采用 STM32F103C8T6 最小系统板插接方案，MQ AO 保留 10k/18k 分压，PA8 需要硬件下拉以保证复位期间继电器处于安全状态。
