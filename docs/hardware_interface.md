# 硬件接口

引脚以 `stm32f103/smart_gas_monitor.ioc` 为准。

| 引脚 | 标签 | 连接 | 说明 |
| --- | --- | --- | --- |
| PA0 | MQ4_AO | MQ-4 | ADC1_IN0，10k/18k 分压 |
| PA1 | MQ6_AO | MQ-6 | ADC1_IN1，10k/18k 分压 |
| PA4 | MQ7_AO | MQ-7 | ADC1_IN4，10k/18k 分压 |
| PA8 | RELAY | 继电器 | 高电平开阀，建议 10k 下拉 |
| PA5 | VALVE_LED | 阀门指示灯 | 软件开阀指示 |
| PA6 | LED_RED | 红灯 | ALARM/FAULT |
| PA7 | BUZZER | 蜂鸣器 | 报警节奏输出 |
| PB0 | LED_GREEN | 绿灯 | NORMAL |
| PB1 | LED_YELLOW | 黄灯 | WARNING/SAFE_WAIT |
| PB12 | KEY1 | 页面切换 | EXTI 下降沿 |
| PB13 | KEY2 | 增加 | EXTI 下降沿 |
| PB14 | KEY3 | 减少 | EXTI 下降沿 |
| PB15 | KEY4 | 安全解除 | EXTI 下降沿 |
| PB5 | KEY5 | 设置项切换 | 上拉轮询 |
| PB6/PB7 | I2C1 | SSD1306 | 0x3C，100 kHz |
| PB10/PB11 | I2C2 | AT24C64 | 0x50，100 kHz |
| PA2/PA3 | USART2 | HC-05 | 9600 baud |
| PA9/PA10 | USART1 | USB-TTL | 115200 baud |
| PA13/PA14 | SWD | ST-Link | SWDIO/SWCLK |

## 供电

**只需要一路 5 V 电源。** 接到最小系统板的 `5V` 脚，板上的稳压器（AMS1117-3.3）产生 3.3 V，再从板的 `3V3` 脚引出来供给 MCU、OLED 和 AT24C64。这样不必额外准备 3.3 V 电源。

**绝对不能把 5 V 直接接到 MCU 的 VDD。** STM32F103 的 VDD 绝对最大额定值是 **4.0 V**，推荐范围 2.0～3.6 V，接 5 V 会烧芯片。最小系统板之所以能接 5 V，是因为 5 V 只进了稳压器，MCU 自己仍然是 3.3 V。

MQ 加热器、继电器模块线圈、HC-05 这几样直接吃 5 V。**所有器件必须共地**，包括 5 V 侧和 3.3 V 侧。

ADC 输入必须限制在 0～3.3 V（MQ AO 经 10 kΩ/18 kΩ 分压后约 3.21 V，留有余量）。

**I2C 上拉必须上拉到 3.3 V。** OLED 和 AT24C64 由板的 `3V3` 脚供电，它们模块上自带的上拉电阻跟着 VCC 走，就自然是 3.3 V，跟 MCU 电平对得上。如果给这两个器件供 5 V，上拉就会把 I2C 总线拉到 5 V，超出 MCU 的输入范围——轻则不通信，重则烧引脚。

AT24C64 使用 16 位存储地址和 32 字节页；编译时设置 `EEPROM_MODEL=2` 可切换为 256 字节、8 位地址、8 字节页的 AT24C02。
