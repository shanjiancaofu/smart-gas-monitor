# 固件分层与开发说明

## 工程来源

本工程由本机 STM32CubeMX 6.18.0（安装日志标识 RC3）实际生成，固件包为 STM32CubeF1 V1.8.7，目标 STM32F103C8T6。参考课设 MDK-GPIO 标准库例程及 chassis-controller 的应用/驱动边界、故障锁存和主机测试方式，未移植底盘的电机、RTOS、OTA 或 G474 配置。

## 分层

| 目录 | 职责 | 依赖 |
| --- | --- | --- |
| cubemx/Core | CubeMX 初始化、启动入口、中断 | HAL；USER CODE 调用 app |
| cubemx/Drivers | ST HAL、CMSIS | 供应商代码 |
| bsp/mq_sensor | PA0/PA1/PA4 三路 ADC 顺序采集与校准 | HAL ADC |
| bsp/key | PB12～PB15 扫描与 30 ms 消抖 | HAL GPIO |
| bsp/alarm_output | 继电器、阀门指示、LED、蜂鸣器 | HAL GPIO |
| bsp/at24c02 | I2C2 分页读写与 ACK polling | HAL I2C |
| app/gas | 阈值、状态机、锁存和安全确认 | 标准 C，不依赖 HAL |
| app/settings | 配置序列化、CRC16、双副本提交 | 配置类型与读写回调，不依赖 HAL |
| app/app.c | 初始化装配、采样调度、按键分发、输出及延时保存 | gas、settings、bsp |

main.c 的 USER CODE 只保存应用对象并调用 `app_init`、`app_run`。GPIO 初始化由 CubeMX 负责。`app.c` 把业务状态转换成 BSP 的布尔量，BSP 不接收业务状态机对象。

`mq_sensor_read()` 按 MQ4、MQ7、MQ8 顺序填充数组，`gas_channel_t` 用同样的顺序编号；`app.c` 里的 `_Static_assert` 保证两者一致，否则阈值会被悄悄换到别的传感器上。

## 外设与时序

| 外设 | 配置 |
| --- | --- |
| 时钟 | 8 MHz HSE → PLL 72 MHz；APB1 36 MHz；ADC 12 MHz |
| ADC1 | PA0/PA1/PA4，单次软件触发，239.5 cycles；按配置周期顺序采集完整三路 |
| I2C1 | PB6/PB7，100 kHz，预留 SSD1306 |
| I2C2 | PB10/PB11，100 kHz，AT24C02 默认 7 位地址 0x50 |
| USART1 | PA9/PA10，115200，8-N-1 |
| USART2 | PA2/PA3，9600，8-N-1 |
| 按键 | PB12～PB15，上拉输入，主循环轮询，30 ms 消抖 |
| 输出 | PB8 绿、PB9 黄、PA6 红、PA7 蜂鸣器、PA8 继电器、PA5 阀门指示 |
| 调试 | PA13/PA14 SWD，关闭 JTAG |

本版按键使用轮询而非课设要求的外部中断，保持原引脚和功能。迁移到 EXTI 时只需改 `bsp/key.c`：中断里记录待处理位，消抖和分发仍留在主循环。上电已按住的按键必须释放后重按才能产生事件。

`alarm_output.h` 集中定义继电器和蜂鸣器的有效电平。继电器默认 PA8 高电平表示允许开阀，低电平关闭。更换极性时，必须同时通过 CubeMX 调整 GPIO 上电默认输出，保证初始化时关闭阀门；复位后到 `MX_GPIO_Init` 之间 PA8 是浮空输入，硬件需要有下拉，否则这段窗口的继电器状态不可控。接点和电平需按实物确认；PA5 仅显示软件开阀命令，没有物理阀位反馈。

## 业务规则

- 三路演示危险阈值为 2400、2000、2400 ADC counts，分别对应 MQ-4、MQ-7、MQ-8；预警阈值为危险阈值的 80%。等于危险阈值即报警，任一路即可关阀。
- 上电默认预热 60 秒并保持关阀；该时间是课设演示设置，不能替代 MQ 模块实际预热/标定要求。预热期间不做报警抑制，高浓度同样进入 ALARM 并锁存——预热期阀门本来就是关闭的，这里的取舍是宁可多报。
- 正常上电且确认安全后自动开阀，不需要人工确认。锁存只由两种事件产生：进入 ALARM，或已取得有效采样后采样中断。此后必须三路均低于危险阈值的 70% 并持续 3 秒，KEY4 才能重新开阀。上电时没有 KEY4 可按的场合不会因此卡住。
- ADC 调用失败、数值越界或采样间隔达到 3 个采样周期，进入 FAULT 并锁存关阀。区分“还没采过”和“采了但失败”：第一次采样尝试之前处于预热，不锁存；此后任何一次失败都算故障。新样本不能掩盖之前的采样中断。
- KEY1 在实时界面、MQ-4、MQ-7、MQ-8、采样周期之间循环；KEY2/3 调整选中项，阈值步长 50、范围 200～4000，采样周期在 100/200/500/1000/2000/5000 ms 之间切换。KEY4 只用于安全恢复确认。采样周期的下限与列表首项一致，不存在按键选不到、只有从 EEPROM 载入才生效的取值。
- 修改停止 2 秒后保存；失败保留 dirty 标志，每 2 秒重试。运行对象 storage_ok 表示最近加载/保存是否成功。空 EEPROM 首次采用默认值，用户修改后保存。
- 报警输出优先于 EEPROM 操作。EEPROM 驱动使用有超时的阻塞事务。NMI、HardFault、MemManage、BusFault、UsageFault 五个异常处理入口都会调用 `alarm_output_force_safe()`，但它写的是 ODR，引脚仍是输入时驱动不了，这一窗口由硬件下拉兜底。当前尚未加入独立看门狗，主循环挂死不能单靠采样超时逻辑关阀。

## 参数存储

0x00～0x0F 和 0x10～0x1F 为两个 16 字节配置槽：magic、版本、16 位序号、三个小端阈值、采样周期、CRC16-CCITT 和提交标志。先清除另一槽提交标志，再写正文，最后提交并读回校验。底层按 8 字节页边界拆分并 ACK polling。

版本号当前为 2；版本 1 的记录没有采样周期字段，会被判为无效并回退到默认值，不会误读。加载时验证 CRC、版本、阈值范围和采样周期范围，选择有效且较新的副本。0x20～0xFF 尚未使用，预留给报警历史。

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

OLED 显示、PC/蓝牙命令、历史报警持久化尚未实现；两组 I2C 和两路 UART 已完成 HAL 初始化。下一步按 OLED → 串口协议 → 报警历史 → Proteus/实物验证的顺序补齐，同时把按键轮询换成 EXTI、引入 TIM2 作为采样节拍。
