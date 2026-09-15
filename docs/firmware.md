# 固件说明

## 工程结构

```text
stm32f103/
├── app/      # 业务层
├── bsp/      # 硬件层
└── cubemx/   # CubeMX 工程和 HAL
```

调用关系为 `main → app → bsp → HAL`。`main.c` 只调用 `app_init()` 和 `app_update()`；CubeMX 生成文件保留在 `cubemx/`。

## app 模块

- `app/app.c`：保存应用实例，按顺序调度采样、按键、串口、报警、历史、显示和参数保存。
- `app/gas/`：将 ADC 通道 0、1、4 映射到 MQ4、MQ6、MQ7，每路读取 8 次并求平均，执行状态机和 lockout。
- `app/alarm/`：根据气体状态控制输出策略，并按报警通道数量生成非阻塞蜂鸣器节奏。
- `app/config/`：参数默认值、范围校验、蜂鸣器设置和 EEPROM 双副本。
- `app/history/`：报警事件添加、读取和清除，只保存首次进入 ALARM 的记录。
- `app/protocol/`：解析 USART1 和 USART2 的共用文本命令。
- `app/display/`：实时、设置、历史三个页面，报警时覆盖显示报警页。

## bsp 模块

- `bsp_adc`：读取一个 ADC 硬件通道；MQ 通道映射和八次平均在 gas 模块。
- `bsp_uart`：双 UART 中断收发和行缓冲。
- `bsp_i2c`：带超时的 I2C 读写和设备探测，按调用方选择 8 位或 16 位存储地址。
- `bsp_oled`：SSD1306 初始化、字模、页缓冲和刷新。
- `bsp_key`：KEY1～KEY4 使用 EXTI，KEY5 使用轮询，统一输出事件位图。
- `bsp_led`、`bsp_buzzer`、`bsp_relay`：只控制对应 GPIO，不判断业务原因。
- `bsp_eeprom`：默认适配 AT24C64，支持 16 位地址、32 字节分页写；编译时可切换 AT24C02。

## 按键和页面

| 按键 | 功能 |
| --- | --- |
| KEY1 | 页面切换：实时 → 设置 → 历史 |
| KEY2 | 设置页增加；历史页向新记录翻阅 |
| KEY3 | 设置页减少；历史页向旧记录翻阅 |
| KEY4 | 安全解除：安全等待满足后清除 lockout |
| KEY5 | 设置项切换；长按约 1.5 秒进入测试模式 |

报警状态会覆盖 OLED 页面，解除后返回此前选择的页面。参数修改自动延时保存，不需要单独的保存键。

## 外设参数

- ADC1：PA0、PA1、PA4，分别对应 MQ4、MQ6、MQ7；239.5 cycles。
- I2C1：PB6/PB7，SSD1306，400 kHz，默认地址 0x3C。
- I2C2：PB10/PB11，AT24C64，100 kHz，默认地址 0x50。
- USART1：PA9/PA10，USB-TTL，115200 baud。
- USART2：PA2/PA3，HC-05，9600 baud。
- TIM2：预分频 71、周期 9999，10 ms 更新中断；中断只增加节拍计数。

## 状态和报警

正常状态开阀；预警状态保持开阀并亮黄灯；ALARM 或 FAULT 关闭继电器、亮红灯并设置 lockout。报警 mask 中有 1、2、3 路时，蜂鸣器分别输出单滴、双滴、三滴循环；所有节奏都在 alarm 模块中非阻塞执行。

报警和故障的 lockout 写入 AT24C64。重启后恢复 lockout，环境连续安全 3 秒后仍需 KEY4 才能开阀。远程协议只允许 `VALVE CLOSE`，不允许远程开阀。

## 存储布局

配置区为 0x00～0x1F 的两个 16 字节槽，v6 布局保存 magic、版本、序号、蜂鸣器设置、三路阈值、采样周期、lockout、CRC16 和提交标志。历史区从 0x20 开始，每条 16 字节；AT24C64 可循环保存 510 条报警事件。记录含序号、上电秒数、三路 ADC 和报警通道 mask，没有 RTC 日期时间。

## 构建和测试

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -MakeArgs -j4
python tools/run_host_tests.py --cc cl
python tools/run_host_tests.py --cc cl --eeprom 2
```

Host Test 覆盖报警、气体状态、配置存储、历史记录和串口协议；两种 EEPROM 模式均通过。当前只完成软件验证，实物、Proteus、原理图和 PCB 仍待联调。
