# 固件说明

本分支使用 STM32CubeMX 生成 HAL 初始化代码，使用 Keil5 编译和下载。工程文件为 stm32f103/MDK-ARM/smart_gas_monitor.uvprojx。

Keil5 输出目录为 build/keil5/。工程代码按 main → app → bsp → HAL 组织：app/ 负责燃气状态机、报警、配置、历史、协议和 OLED 页面；bsp/ 负责硬件访问。

当前硬件方案为 MQ4/MQ6/MQ7、五按键、SSD1306 OLED、AT24C64 和 HC-05。报警或故障会设置 lockout，环境恢复后仍需现场按键解除。
