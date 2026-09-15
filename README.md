# 智能燃气监测与自动防护系统

基于 STM32F103C8T6 的裸机燃气监测系统，代码按 `main → app → bsp → HAL` 分层。

```text
smart-gas-monitor/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
│   └── arm-none-eabi-gcc.cmake
├── stm32f103/
│   ├── app/
│   ├── bsp/
│   └── cubemx/
├── tests/
├── tools/
└── build/                    # 生成目录，不提交 Git
    ├── arm-debug/
    └── arm-release/
```

当前功能包括 MQ4/MQ6/MQ7 三路采样、五键交互、OLED 页面、TIM2 节拍、AT24C64 参数与报警历史、lockout、多传感器蜂鸣器节奏以及 USART1/HC-05 文本协议。

## 工具要求

- CMake 3.22 或更高版本
- Ninja
- GNU Arm Embedded Toolchain，包含 `arm-none-eabi-gcc`

将工具加入 `PATH`。也可以设置 `ARM_GNU_TOOLCHAIN_ROOT`，其目录下应包含 `bin/arm-none-eabi-gcc`。

## Debug 构建

```sh
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel 4
```

## Release 构建

```sh
cmake --preset arm-release
cmake --build --preset arm-release --parallel 4
```

产物分别位于 `build/arm-debug/` 和 `build/arm-release/`，文件名包含本次配置时间：

```text
smart_gas_monitor_YYYYMMDD_HHMMSS.elf
smart_gas_monitor_YYYYMMDD_HHMMSS.hex
smart_gas_monitor_YYYYMMDD_HHMMSS.bin
smart_gas_monitor_YYYYMMDD_HHMMSS.map
```

每次重新执行 `cmake --preset ...` 会更新文件时间戳，并清理该配置目录中的上一组固件文件。

## 主机测试

```sh
python tools/run_host_tests.py --cc gcc
python tools/run_host_tests.py --cc gcc --eeprom 2
```

Windows 的 Visual Studio Native Tools 环境也可以使用 `--cc cl`。

## 文档

- [固件说明](docs/firmware.md)
- [硬件接口](docs/hardware_interface.md)
- [串口协议](docs/uart_protocol.md)
- [演示流程](docs/demo_flow.md)
- [验证记录](docs/verification.md)
