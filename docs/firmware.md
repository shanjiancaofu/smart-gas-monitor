# 固件说明

工程按 `main → app → bsp → HAL` 分层。`main.c` 只调用 `app_init()` 和 `app_update()`，CubeMX 生成文件保留在 `stm32f103/cubemx/`。

`app/` 负责 MQ4/MQ6/MQ7 状态机、报警、配置、历史、协议和 OLED 页面。`bsp/` 负责 ADC、UART、I2C、SSD1306、五键、LED、蜂鸣器、继电器和 AT24C64。报警判断和响铃节奏不放进蜂鸣器 BSP，EEPROM 的地址宽度和分页也由 BSP 配置处理。

按键定义：KEY1 页面切换，KEY2 增加，KEY3 减少，KEY4 安全解除，KEY5 设置项切换。页面和设置项分别保存在 display 与 gas 对象中，KEY4 不参与普通设置。

AT24C64 默认使用 8192 字节、16 位地址和 32 字节页。配置占用 0x00～0x1F，历史从 0x20 开始，每条 16 字节，最多 510 条循环记录。编译时使用 `-DEEPROM_MODEL=2` 可切换 AT24C02 兼容模式。

ALARM 和 FAULT 会设置 lockout。环境安全保持 3 秒后仍需 KEY4 才能开阀。蜂鸣器根据报警 mask 中的传感器数量输出一滴、两滴或三滴节奏，全部使用 TIM2 节拍，不调用 `HAL_Delay()`。

## 构建

```powershell
powershell -ExecutionPolicy Bypass -File .uild.ps1 -Configuration Debug -Jobs 4
powershell -ExecutionPolicy Bypass -File .uild.ps1 -Configuration Release -Jobs 4
```

Debug 使用 `-Og` 和调试信息，Release 使用 `-Os` 和调试信息。产物目录为 `build/debug/` 和 `build/release/`。
