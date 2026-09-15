# 智能燃气监测与自动防护系统

基于 STM32F103C8T6 的裸机燃气监测系统。CubeMX 负责初始化和 HAL，手写代码按照 `main → app → bsp → HAL` 分层。

## 目录结构

```text
stm32f103/
├── app/
│   ├── app.c/.h                 # 应用入口和任务调度
│   ├── gas/gas.c/.h             # MQ4/MQ6/MQ7 映射、八次平均、状态机
│   ├── alarm/alarm.c/.h         # LED、蜂鸣器、继电器业务策略
│   ├── config/config.c/.h       # 参数校验、CRC、双副本保存
│   ├── history/history.c/.h     # 报警事件记录
│   ├── protocol/protocol.c/.h   # 两路串口共用协议
│   └── display/display.c/.h     # OLED 页面
├── bsp/
│   ├── bsp_adc.c/.h             # ADC 硬件读取
│   ├── bsp_uart.c/.h            # UART 中断收发
│   ├── bsp_i2c.c/.h             # I2C 传输封装
│   ├── bsp_oled.c/.h            # SSD1306 驱动
│   ├── bsp_key.c/.h             # 五键输入和消抖
│   ├── bsp_led.c/.h             # LED 输出
│   ├── bsp_buzzer.c/.h          # 蜂鸣器开关
│   ├── bsp_relay.c/.h           # 继电器开关
│   └── bsp_eeprom.c/.h          # AT24C64 分页访问
└── cubemx/                      # CubeMX 工程，包含 Core、Drivers、IOC 和 Makefile
```

`main.c` 只调用 `app_init()` 和 `app_update()`。应用层负责业务判断，BSP 只负责硬件访问。ALARM、FAULT 和远程关阀会设置 lockout，断电重启后仍保持关阀，现场 KEY4 才能解除。

主要功能：三路 ADC 每路八次平均、五键输入、OLED 页面、TIM2 10 ms 节拍、AT24C64 参数和报警历史、USART1/HC-05 文本协议、多传感器蜂鸣器节奏。

## 构建和测试

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -MakeArgs -j4
python tools/run_host_tests.py --cc cl
```

最终固件产物同步到仓库根目录 `build/`；CubeMX 中间文件保留在 `stm32f103/cubemx/build/`。当前已完成软件测试和 ARM 构建，硬件、Proteus、PCB 和蓝牙 APP 尚待后续联调。

## 文档

- [固件说明](docs/firmware.md)
- [硬件接口](docs/hardware_interface.md)
- [串口协议](docs/uart_protocol.md)
- [验证记录](docs/verification.md)
- [硬件设计与接线说明](docs/智能燃气监测与自动防护系统_硬件设计与接线说明.docx)
