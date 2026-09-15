# 智能燃气监测与自动防护系统 课设版

基于 STM32F103C8T6 的简化课设工程，使用 STM32 HAL 和 Keil5 编译下载。

```text
smart-gas-monitor/
├── stm32f103/
│   ├── MDK-ARM/smart_gas_monitor.uvprojx  # Keil5 工程
│   ├── Core/                              # CubeMX 生成初始化代码
│   ├── Drivers/                           # STM32F1 HAL/CMSIS
│   ├── app/                               # 应用业务
│   ├── bsp/                               # 硬件驱动
│   ├── smart_gas_monitor.ioc              # CubeMX 配置
│   └── STM32F103xx_FLASH.ld
├── docs/
├── tests/
└── tools/
```

当前功能包括 MQ4/MQ6/MQ7 三路采样、五键交互、OLED 页面、TIM2 节拍、AT24C64 参数和报警历史、lockout、多传感器蜂鸣器节奏以及 USART1/HC-05 文本协议。

## Keil5 使用

用 Keil5 打开：

```text
stm32f103/MDK-ARM/smart_gas_monitor.uvprojx
```

选择 `smart_gas_monitor` Target 后点击 Build。工程已经加入 CubeMX HAL、`app/` 和 `bsp/` 源文件，头文件路径也已配置。下载使用 ST-Link，芯片选择 STM32F103C8T6。

Keil5 生成的目标文件默认位于：

```text
stm32f103/MDK-ARM/smart_gas_monitor/
```

## 主机测试

```powershell
python tools/run_host_tests.py --cc cl
python tools/run_host_tests.py --cc cl --eeprom 2
```

## 可选 CMake 构建

仓库保留 CMake/Ninja 入口用于命令行构建：

```powershell
cmake --preset arm-debug
cmake --build --preset arm-debug

cmake --preset arm-release
cmake --build --preset arm-release
```

课设交付以 Keil5 工程为准。

## 文档

- [固件说明](docs/firmware.md)
- [硬件接口](docs/hardware_interface.md)
- [串口协议](docs/uart_protocol.md)
- [演示流程](docs/demo_flow.md)
- [验证记录](docs/verification.md)
