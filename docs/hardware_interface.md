# 硬件接口表

本文档给硬件联调使用。引脚以 `stm32f103/cubemx/smart_gas_monitor.ioc` 为准，电平有效性以 `bsp/bsp_relay.h` 和 `bsp/bsp_buzzer.h` 为准。

| STM32 引脚 | User Label | 连接对象 | 说明 |
| --- | --- | --- | --- |
| PA0 | MQ4_AO | MQ-4 AO | ADC1_IN0，经 10 kΩ/18 kΩ 分压 |
| PA1 | MQ7_AO | MQ-7 AO | ADC1_IN1，经 10 kΩ/18 kΩ 分压 |
| PA4 | MQ8_AO | MQ-8 AO | ADC1_IN4，经 10 kΩ/18 kΩ 分压 |
| PA8 | RELAY | 继电器模块 | 高电平默认表示允许开阀；PA8 建议 10 kΩ 下拉 |
| PA5 | VALVE_LED | 阀门状态 LED | 软件开阀指示 |
| PA6 | LED_RED | 红色 LED | ALARM/FAULT |
| PA7 | BUZZER | 有源蜂鸣器 | 报警时按 TIM2 节拍控制 |
| PB8 | LED_GREEN | 绿色 LED | NORMAL |
| PB9 | LED_YELLOW | 黄色 LED | WARNING/SAFE_WAIT |
| PB12 | KEY1 | 模式键 | EXTI 下降沿，上拉输入 |
| PB13 | KEY2 | 增加键 | EXTI 下降沿，上拉输入 |
| PB14 | KEY3 | 减少键 | EXTI 下降沿，上拉输入 |
| PB15 | KEY4 | 安全确认键 | EXTI 下降沿，上拉输入 |
| PB6/PB7 | I2C1 | SSD1306 OLED | SCL/SDA，地址默认 0x3C |
| PB10/PB11 | I2C2 | AT24C02 | SCL/SDA，地址默认 0x50 |
| PA2/PA3 | USART2 | HC-05 | 9600 baud |
| PA9/PA10 | USART1 | USB-TTL | 115200 baud |
| PA13/PA14 | SWD | ST-Link | SWDIO/SWCLK |

ADC 输入范围必须保持在 0～3.3 V。MQ AO 使用 10 kΩ/18 kΩ 分压，并建议在 ADC 节点并联 100 nF 电容。I2C 两组总线均需要上拉到 3.3 V。继电器模块的输入有效电平、线圈供电和触点逻辑必须在实物联调时确认。

TIM2 预分频 71、周期 9999，产生 10 ms 中断；中断只累加软件节拍，不直接访问 OLED、EEPROM 或 ADC。
