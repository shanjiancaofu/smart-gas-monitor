# 硬件接口

引脚以 `stm32f103/cubemx/smart_gas_monitor.ioc` 为准。

| 引脚 | 标签 | 连接 | 说明 |
| --- | --- | --- | --- |
| PA0 | MQ4_AO | MQ-4 | ADC1_IN0，10k/18k 分压 |
| PA1 | MQ6_AO | MQ-6 | ADC1_IN1，10k/18k 分压 |
| PA4 | MQ7_AO | MQ-7 | ADC1_IN4，10k/18k 分压 |
| PA8 | RELAY | 继电器 | 高电平开阀，建议 10k 下拉 |
| PA5 | VALVE_LED | 阀门指示灯 | 软件开阀指示 |
| PA6 | LED_RED | 红灯 | ALARM/FAULT |
| PA7 | BUZZER | 蜂鸣器 | 报警节奏输出 |
| PB8 | LED_GREEN | 绿灯 | NORMAL |
| PB9 | LED_YELLOW | 黄灯 | WARNING/SAFE_WAIT |
| PB12 | KEY1 | 页面切换 | EXTI 下降沿 |
| PB13 | KEY2 | 增加 | EXTI 下降沿 |
| PB14 | KEY3 | 减少 | EXTI 下降沿 |
| PB15 | KEY4 | 安全解除 | EXTI 下降沿 |
| PB5 | KEY5 | 设置项切换 | 上拉轮询 |
| PB6/PB7 | I2C1 | SSD1306 | 0x3C，400 kHz |
| PB10/PB11 | I2C2 | AT24C64 | 0x50，100 kHz |
| PA2/PA3 | USART2 | HC-05 | 9600 baud |
| PA9/PA10 | USART1 | USB-TTL | 115200 baud |
| PA13/PA14 | SWD | ST-Link | SWDIO/SWCLK |

ADC 输入必须限制在 0～3.3 V。I2C 上拉到 3.3 V。AT24C64 使用 16 位存储地址和 32 字节页；编译时设置 `EEPROM_MODEL=2` 可切换为 256 字节、8 位地址、8 字节页的 AT24C02。
