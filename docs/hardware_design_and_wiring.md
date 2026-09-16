# 智能燃气监测与自动防护系统硬件设计与接线说明

本文档是当前 `main` 分支的硬件接线基线，用于原理图、Proteus、PCB、焊接和实物联调。传感器阈值使用 12 位 ADC 原始计数，不代表标定后的 ppm。

## 1. 系统组成

| 模块 | 器件或接口 | 说明 |
| --- | --- | --- |
| 主控 | STM32F103C8T6 最小系统板 | SWD 下载调试 |
| 气体检测 | MQ-4、MQ-6、MQ-7 | 三路模拟 AO，经分压后进入 ADC |
| 显示 | SSD1306 I2C OLED | I2C1 |
| 存储 | AT24C64 | I2C2，保存配置和报警历史 |
| 通信 | USB-TTL、HC-05 | USART1 / USART2 |
| 执行与报警 | 继电器、蜂鸣器、绿黄红 LED、阀门 LED | 报警时关阀并锁存 |
| 输入 | KEY1～KEY5 | 页面、参数调整、安全确认和设置项 |

## 2. MCU 引脚分配

| 引脚 | 功能 | 连接 |
| --- | --- | --- |
| PA0 | ADC1_IN0 | MQ-4 AO 分压节点 |
| PA1 | ADC1_IN1 | MQ-6 AO 分压节点 |
| PA4 | ADC1_IN4 | MQ-7 AO 分压节点 |
| PA2 / PA3 | USART2_TX / RX | HC-05 RXD / TXD |
| PA9 / PA10 | USART1_TX / RX | USB-TTL RXD / TXD |
| PB8 / PB9 | I2C1 SCL / SDA (remapped) | SSD1306 OLED |
| PB10 / PB11 | I2C2 SCL / SDA | AT24C64 |
| PB0 | GPIO | 绿 LED，NORMAL |
| PB1 | GPIO | 黄 LED，WARNING |
| PA6 | GPIO | 红 LED，ALARM/FAULT |
| PA7 | GPIO | 有源蜂鸣器 |
| PA8 | GPIO | 继电器 IN，默认关阀 |
| PA5 | GPIO | 阀门状态 LED |
| PB12 | EXTI12 | KEY1 页面/选择 |
| PB13 | EXTI13 | KEY2 增加 |
| PB14 | EXTI14 | KEY3 减少 |
| PB15 | EXTI15 | KEY4 安全确认 |
| PB5 | GPIO 输入 | KEY5 设置项轮询 |
| PA13 / PA14 | SWDIO / SWCLK | ST-Link，保留 |

KEY1～KEY4 使用内部上拉、按下接 GND；KEY5 使用 PB5 内部上拉轮询。按键消抖和业务处理在主循环执行，不在 IRQ 中访问 OLED、I2C 或 EEPROM。

## 3. 电源和模拟输入

- MQ-4、MQ-6、MQ-7 加热器使用 5 V，三路共地；5 V 电源建议至少 1 A，并为继电器留余量。
- MCU、OLED、AT24C64 的逻辑电平使用 3.3 V。确认模块自带 I2C 上拉不会把 SDA/SCL 拉到 5 V。
- 每路 MQ AO 使用 10 kΩ / 18 kΩ 分压：`AO -> 10 kΩ -> ADC -> 18 kΩ -> GND`。5 V 输入时 ADC 约 3.21 V。
- ADC 节点可并联 100 nF 对地电容。DO 数字输出不接 MCU。

## 4. 总线和通信接线

### OLED I2C1

| OLED | STM32 |
| --- | --- |
| VCC | 3.3 V |
| GND | GND |
| SCL | PB8 |
| SDA | PB9 |

常见 SSD1306 地址为 `0x3C`，以实际模块为准。驱动使用 Page Addressing Mode，并按 dirty page 刷新。

### AT24C64 I2C2

| AT24C64 | STM32 |
| --- | --- |
| VCC | 3.3 V |
| GND | GND |
| SCL | PB10 |
| SDA | PB11 |

AT24C64 为 8192 字节、32 字节页、16 位存储地址。常见基础地址为 `0x50`，A0/A1/A2 以模块接法为准。写入必须等待内部写周期或使用 ACK polling。

### 串口

- USART1：PA9 -> USB-TTL RXD，PA10 <- USB-TTL TXD，默认 115200-8-N-1。
- USART2：PA2 -> HC-05 RXD，PA3 <- HC-05 TXD，常见透传速率 9600。
- 两个串口必须共地；USB-TTL 优先使用 3.3 V TTL 电平。
- USB 和 HC-05 在软件中使用独立协议队列，回复不会互串。

## 5. 状态和安全策略

| 状态 | 指示 | 阀门 |
| --- | --- | --- |
| WARMUP | 阀门关闭，暂不判定气体阈值 | CLOSED |
| NORMAL | 绿灯 | OPEN |
| WARNING | 黄灯 | OPEN |
| ALARM | 红灯、蜂鸣器、记录事件 | CLOSED |
| FAULT | 红灯、持续故障提示 | CLOSED |
| SAFE_WAIT | 等待 KEY4 | CLOSED |

MQ 预热 3 秒（`GAS_WARMUP_MS`）期间阀门保持关闭，ADC 或采样链路真正失败仍进入 FAULT。真实 MQ 传感器要热机几分钟以上才稳定，本项目按演示需要固定取 3 秒。ALARM 或 FAULT 解除后，只有环境恢复安全并按下 KEY4 才允许重新开阀。

## 6. 构建和联调

- Keil5 是课设主构建：打开 `stm32f103/MDK-ARM/smart_gas_monitor.uvprojx`。最终文件位于 `build/keil5/Artifacts/`，中间文件位于 `build/keil5/Listings/`。
- CMake/Ninja 是可选交叉验证：使用 `cmake --preset arm-debug` 或 `arm-release`，最终 `.elf/.hex/.bin` 位于对应 `build/arm-*` 目录。
- 上电顺序：先测 12 V / 5 V / 3.3 V，再验证 SWD、USART1、ADC、OLED、AT24C64、按键、LED/蜂鸣器、继电器，最后接 HC-05。
- 实物联调前确认继电器有效电平、OLED/EEPROM 上拉电压、MQ 分压节点电压和 HC-05 供电版本。
