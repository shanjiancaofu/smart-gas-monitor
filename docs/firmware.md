# 课设版固件说明

本分支使用 STM32CubeMX 生成 HAL 初始化代码，使用 Keil5 编译和下载。主工程文件为：

```text
stm32f103/MDK-ARM/smart_gas_monitor.uvprojx
```

打开 Keil5 后选择 `smart_gas_monitor` Target，点击 Build。工程已经包含 `Core/`、`Drivers/`、`app/` 和 `bsp/` 源文件，头文件路径也已经配置完成。下载使用 ST-Link，目标芯片为 STM32F103C8T6。

代码调用关系为：

```text
main → app → bsp → HAL
```

`app/` 负责 MQ4/MQ6/MQ7 状态机、报警、配置、历史、协议和 OLED；`bsp/` 负责 ADC、UART、I2C、OLED、五键、LED、蜂鸣器、继电器和 EEPROM。AT24C64 默认使用 16 位地址和 32 字节页写入。

按键定义：KEY1 页面切换，KEY2 增加，KEY3 减少，KEY4 安全解除，KEY5 设置项切换。报警时 OLED 覆盖显示报警页；报警和故障会设置 lockout，环境安全后仍需 KEY4 解除。

历史区只保存首次进入 ALARM 的报警事件，不保存实时曲线。报警蜂鸣器根据报警传感器数量输出单滴、双滴或三滴模式，全部由定时节拍驱动。

CMake 仍保留为可选命令行构建入口，但课设交付和调试以 Keil5 工程为准。
