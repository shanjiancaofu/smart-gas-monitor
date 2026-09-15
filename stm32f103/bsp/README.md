# BSP 硬件层

BSP 只负责硬件访问，不判断业务原因、不解析协议。

- `bsp_adc.*`：读取 ADC 硬件通道。
- `bsp_uart.*`：两路 UART 中断收发和行缓冲。
- `bsp_i2c.*`：带超时的 I2C 读写和设备探测。
- `bsp_oled.*`：SSD1306 初始化、字模、页缓冲和刷新。
- `bsp_key.*`：KEY1～KEY4 EXTI、KEY5 轮询、消抖和事件位图。
- `bsp_led.*`：三色状态灯和阀门指示灯。
- `bsp_buzzer.*`：蜂鸣器 GPIO 开关，不负责计时。
- `bsp_relay.*`：继电器 GPIO 开关，不决定开关原因。
- `bsp_eeprom.*`：AT24C64 32 字节分页读写，支持 16 位存储地址。

中断里只记录事件、累加 TIM2 节拍或搬运串口数据，业务处理留在主循环。
