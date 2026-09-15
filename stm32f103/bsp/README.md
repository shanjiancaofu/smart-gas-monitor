# 板级硬件层

| 文件 | 职责 |
| --- | --- |
| bsp_adc.* | 单次读取硬件通道 0～15；用 bool 返回转换是否成功 |
| bsp_uart.* | 双 UART 中断收发与行缓冲 |
| bsp_i2c.* | 带超时的一字节寄存器地址读写、ACK 轮询 |
| bsp_oled.* | SSD1306 缓冲、字模、初始化与刷新 |
| bsp_key.* | EXTI 事件记录与主循环消抖 |
| bsp_led.* | 三色灯和阀门状态指示的 GPIO 输出 |
| bsp_buzzer.* | 蜂鸣器 GPIO 开关；不计时 |
| bsp_relay.* | 继电器 GPIO 开关；不判断开关原因 |
| bsp_at24c02.* | EEPROM 边界检查、8 字节分页写入，使用 bsp_i2c |

BSP 只依赖 HAL 和 CubeMX 引脚宏，不包含 app 头文件。中断中只累加 tick 或搬运字节/事件；业务、ADC 平均、报警持续时间和 EEPROM 数据含义由 app 决定。
