# 硬件接口

以下为最终硬件职责（**实物**构建 `smart_gas_monitor_hw`）。TIM4 舵机配置已在固件和 `.ioc` 中实现。

> **仿真构建（`smart_gas_monitor_soft`）的舵机和 MQ6 引脚与下表不同**：舵机走 **PA1 / TIM2_CH2**，MQ6 改接 **PA5 / ADC_IN5**（PA5 的阀门灯改到 PB9）。原因是 Proteus 的 STM32 模型不实现 TIM4_CH3 走 PB8 这一路，而它实现 TIM2。**实物接线以上表为准，不需要跟着改。**详见 [Proteus 仿真说明](../hardware/proteus/README.md)第七节。

| 引脚 | 标签 | 连接 | 说明 |
| --- | --- | --- | --- |
| PA0 | MQ4_AO | MQ-4 | ADC1_IN0，10k/18k 分压 |
| PA1 | MQ6_AO | MQ-6 | ADC1_IN1，10k/18k 分压（**仿真构建里这个脚是舵机 PWM，MQ6 在 PA5**） |
| PA4 | MQ7_AO | MQ-7 | ADC1_IN4，10k/18k 分压 |
| PA8 | RELAY | 风扇继电器 | HIGH 触发：高电平风扇开，低电平风扇关；建议 10k 下拉 |
| PB8 | TIM4_CH3 | 舵机燃气阀门 | 50 Hz PWM，500 us 关闭、2500 us 打开，实物按行程校准（**仿真构建里舵机在 PA1**） |
| PA5 | VALVE_LED | 阀门指示灯 | 软件开阀指示（**仿真构建里这个脚是 MQ6，阀门灯在 PB9**） |
| PA6 | LED_RED | 红灯 | ALARM/FAULT |
| PA7 | BUZZER | 蜂鸣器 | 低电平触发，报警节奏输出 |
| PB0 | LED_GREEN | 绿灯 | NORMAL |
| PB1 | LED_YELLOW | 黄灯 | WARNING/SAFE_WAIT |
| PB12 | KEY1 | 页面切换 | EXTI 下降沿 |
| PB13 | KEY2 | 增加 | EXTI 下降沿 |
| PB14 | KEY3 | 减少 | EXTI 下降沿 |
| PB15 | KEY4 | 安全解除 | EXTI 下降沿 |
| PB5 | KEY5 | 设置项切换 | 上拉轮询 |
| PB6/PB7 | I2C1 | SSD1306 | 0x3C，100 kHz |
| PB10/PB11 | I2C2 | AT24C64 | 0x50，100 kHz |
| PA2/PA3 | USART2 | HC-05 | 9600 baud |
| PA9/PA10 | USART1 | USB-TTL | 115200 baud |
| PA13/PA14 | SWD | ST-Link | SWDIO/SWCLK |

## 状态与执行器映射

| 状态 | 风扇（继电器 PA8） | 舵机阀门 PB8 | 蜂鸣器 / 灯 |
| --- | --- | --- | --- |
| WARMUP | OFF | 关闭 | 等待 |
| NORMAL | OFF | 打开 | 绿灯 |
| WARNING | **OFF** | 打开 | 黄灯 |
| ALARM | **ON** | **关闭** | 红灯 + 蜂鸣器 |
| FAULT | ON | 关闭 | 红灯 + 蜂鸣器 |
| SAFE_WAIT | ON | 关闭 | 黄灯，等待 KEY4 |
| KEY4 成功恢复 NORMAL | OFF | 打开 | 绿灯 |

- PA5 表示舵机阀门的命令状态：OPEN 亮、CLOSE 灭，不是阀位传感器反馈。PA8 继电器只负责风扇。
- 普通上电先风扇 OFF、阀门关闭、蜂鸣器 OFF、阀门灯 OFF。若 EEPROM 恢复 `lockout=1`，预热期间也保持风扇 ON、阀门关闭，不能按普通 WARMUP 关闭排风。
- 环境安全保持 3 秒后，KEY4 才能成功解除锁存；清除的 `lockout` 写回 EEPROM。
- 蜂鸣器遵循现有静音、限时、持续报警设置；表中表示报警用途，不代表所有设置下持续发声。
- TIM2 保持原 10 ms 中断；TIM4 使用 1 MHz 计数、20 ms 周期（50 Hz），启动 PWM 前装载 CLOSE 的 CCR3。
- **仿真构建**把 TIM2 的周期改成 20 ms 并输出 CH2 给舵机，系统节拍由中断里补第二次 `app_tick_isr()` 维持 10 ms；实物构建完全不走这条路径。
- 本表为目标行为；继电器、舵机、报警闭环及 EEPROM 掉电恢复仍待实物验收。

### 锁存与掉电恢复

全表由两条规则推出来：

```text
NORMAL                      → 风扇 OFF、舵机阀门 OPEN、lockout = 0
ALARM / FAULT / SAFE_WAIT   → 风扇 ON、舵机阀门 CLOSED、lockout = 1
```

`lockout` 是唯一需要跨掉电保存的状态，写在 EEPROM 配置区第 12 字节。它是**边沿触发、立即落盘**而不是延时保存——对安全锁存来说，掉电窗口越小越好。

上电时序：

```text
上电
 └─ config_load_status() 读 EEPROM 的 lockout
     ├─ lockout = 0 → 正常走 WARMUP
     │                 └─ 预热结束且环境安全 → NORMAL，自动开阀，不需要按键
     └─ lockout = 1 → 不直接开阀
                       ├─ 舵机保持 CLOSED
                       ├─ 风扇 ON（预热期间也保持，不能按普通 WARMUP 关掉排风）
                       ├─ 等环境恢复安全并保持 3 秒
                       ├─ 按 KEY4 人工确认
                       ├─ lockout 清零并写回 EEPROM
                       └─ 风扇 OFF、舵机阀门 OPEN
```

**为什么用 EEPROM 而不是自保持继电器**：省掉一个机械自保持器件，且锁存状态能和阈值、采样周期共用同一份配置记录。代价是下面三条。

#### 三条已知限制

1. **EEPROM 写失败时，锁存只活在 RAM 里。** 写失败只把 `storage_ok` 置 false，不改变逻辑状态；此时若掉电，重启读到 `lockout = 0`，系统会当作安全状态直接开阀。这是这个方案的固有弱点——它的失效安全是**逻辑的**，不像自保持继电器是**物理的**。报告里应当写明这一条。
2. **舵机断电不保持位置。** 掉电期间舵机是松的，阀门可能被气流推动。EEPROM 只保证**重新上电时系统知道该关阀**，保证不了**断电期间阀门物理上还关着**。所以上电后先关阀、再等 KEY4 这个顺序是必须的，`alarm_init()` 第一件事就是 `alarm_force_safe()`。
3. **KEY4 的前置条件是三路都低于安全线**（各自阈值的 70%）并持续 3 秒。任何一路悬空或读数居高不下，`reset_ready` 就永远不成立，KEY4 按了没反应——那不是按键坏了。未接传感器的 ADC 输入必须接地。

> WARNING 不在上面两条规则里，保持**风扇 OFF、舵机 OPEN**：预警不停气、不排风，只亮黄灯。

## 供电

**只需要一路 5 V 电源。** 接到最小系统板的 `5V` 脚，板上的稳压器（AMS1117-3.3）产生 3.3 V，再从板的 `3V3` 脚引出来供给 MCU、OLED 和 AT24C64。这样不必额外准备 3.3 V 电源。

**绝对不能把 5 V 直接接到 MCU 的 VDD。** STM32F103 的 VDD 绝对最大额定值是 **4.0 V**，推荐范围 2.0～3.6 V，接 5 V 会烧芯片。最小系统板之所以能接 5 V，是因为 5 V 只进了稳压器，MCU 自己仍然是 3.3 V。

MQ 加热器、继电器模块线圈、**舵机**、HC-05 这几样直接吃 5 V。**所有器件必须共地**，包括 5 V 侧和 3.3 V 侧。

ADC 输入必须限制在 0～3.3 V（MQ AO 经 10 kΩ/18 kΩ 分压后约 3.21 V，留有余量）。

**I2C 上拉必须上拉到 3.3 V。** OLED 和 AT24C64 由板的 `3V3` 脚供电，它们模块上自带的上拉电阻跟着 VCC 走，就自然是 3.3 V，跟 MCU 电平对得上。如果给这两个器件供 5 V，上拉就会把 I2C 总线拉到 5 V，超出 MCU 的输入范围——轻则不通信，重则烧引脚。

AT24C64 使用 16 位存储地址和 32 字节页；编译时设置 `EEPROM_MODEL=2` 可切换为 256 字节、8 位地址、8 字节页的 AT24C02。
