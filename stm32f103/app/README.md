# 应用业务层

| 文件 | 职责 |
| --- | --- |
| app.c/.h | `app_init()` 初始化，`app_update()` 调度任务；应用实例与外设句柄不对 main 暴露 |
| gas/gas.c/.h | MQ 通道映射、8 次平均、阈值判断、状态机和安全锁存；不操作 GPIO |
| alarm/alarm.c/.h | 按状态决定 LED、蜂鸣器和继电器；按 TIM2 tick 控制响铃窗口 |
| config/config.c/.h | 配置结构、默认值、范围校验、蜂鸣器换算、EEPROM 双副本编码 |
| history/history.c/.h | 历史记录添加、读取、清除；不改配置区 |
| protocol/protocol.c/.h | 解析文本命令，调用业务接口；不直接操作继电器 |
| display/display.c/.h | 实时、设置、历史页面；使用 BSP OLED 绘制 |

依赖方向：main → app → 业务模块 → BSP → HAL。gas/config/history/protocol 保持不依赖 HAL；alarm 调用 BSP 输出，display 调用 BSP OLED。配置以 `gas_config_t` 保存在运行状态中，config 模块负责类型与存储，不复制第二份运行配置。
