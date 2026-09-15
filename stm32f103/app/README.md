# 应用层

应用层只处理业务和调度，不直接读写 GPIO、ADC、I2C 或 UART。

- `app.c/.h`：应用实例、初始化和 `app_update()` 调度。
- `gas/`：三路气体映射、八次平均、阈值判断、状态机和 lockout。
- `alarm/`：按状态和报警传感器数量决定 LED、继电器和蜂鸣器节奏。
- `config/`：参数结构、默认值、范围校验、CRC 和 EEPROM 双副本。
- `history/`：报警事件添加、读取和清除。
- `protocol/`：USB-TTL 与 HC-05 共用文本协议。
- `display/`：实时、设置、历史、系统和报警页面。

依赖方向为 `main → app → 业务模块 → bsp → HAL`。`gas`、`config`、`history`、`protocol` 保持 HAL 无关，便于主机测试。
