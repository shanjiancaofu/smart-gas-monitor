# 智能燃气监测与自动防护系统

基于 STM32F103C8T6 的裸机燃气监测系统，代码按 `main → app → bsp → HAL` 分层。

```text
stm32f103/
├── app/      # gas、alarm、config、history、protocol、display
├── bsp/      # ADC、UART、I2C、OLED、按键、LED、蜂鸣器、继电器、EEPROM
└── cubemx/   # CubeMX 工程和 HAL
```

当前功能包括 MQ4/MQ6/MQ7 三路八次平均采样、五键交互、OLED 页面、TIM2 10 ms 节拍、AT24C64 参数和报警历史、lockout、防误开阀、多传感器蜂鸣器节奏和 USART1/HC-05 文本协议。

## 构建

Debug：

```powershell
powershell -ExecutionPolicy Bypass -File .uild.ps1 -Configuration Debug -Jobs 4
```

Release：

```powershell
powershell -ExecutionPolicy Bypass -File .uild.ps1 -Configuration Release -Jobs 4
```

产物分别位于：

```text
build/debug/
build/release/
```

每个目录包含 `smart_gas_monitor.elf`、`.hex`、`.bin` 和 `.map`。CubeMX 中间文件位于 `stm32f103/cubemx/build/`，一般不作为交付目录使用。

## 主机测试

```powershell
python tools/run_host_tests.py --cc cl
python tools/run_host_tests.py --cc cl --eeprom 2
```

两种 EEPROM 模式均覆盖报警、气体状态、配置、历史、协议、页面和分页地址测试。

## 文档

- [固件说明](docs/firmware.md)
- [硬件接口](docs/hardware_interface.md)
- [串口协议](docs/uart_protocol.md)
- [演示流程](docs/demo_flow.md)
- [验证记录](docs/verification.md)
