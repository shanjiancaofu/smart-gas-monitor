# 智能燃气监测与自动防护系统硬件设计与接线说明

本文档是当前 `main` 分支的硬件接线基线，用于原理图、Proteus、PCB、焊接和实物联调。传感器阈值使用 12 位 ADC 原始计数，不代表标定后的 ppm。

## 1. 系统组成

| 模块 | 器件或接口 | 说明 |
| --- | --- | --- |
| 主控 | STM32F103C8T6 最小系统板 | SWD 下载调试 |
| 气体检测 | MQ-4、MQ-6、MQ-7 | 三路模拟 AO，经分压后进入 ADC |
| 显示 | SSD1306 I2C OLED | I2C1 |
| 存储 | AT24C64 | I2C2，保存配置和报警历史 |
| 通信 | USB-TTL、HC-05 | USART1 / USART2 |
| 执行与报警 | 继电器、蜂鸣器、绿黄红 LED、阀门 LED | 报警时关阀并锁存 |
| 输入 | KEY1～KEY5 | 页面、参数调整、安全确认和设置项 |

## 2. MCU 引脚分配

| 引脚 | 功能 | 连接 |
| --- | --- | --- |
| PA0 | ADC1_IN0 | MQ-4 AO 分压节点 |
| PA1 | ADC1_IN1 | MQ-6 AO 分压节点 |
| PA4 | ADC1_IN4 | MQ-7 AO 分压节点 |
| PA2 / PA3 | USART2_TX / RX | HC-05 RXD / TXD |
| PA9 / PA10 | USART1_TX / RX | USB-TTL RXD / TXD |
| PB6 / PB7 | I2C1 SCL / SDA | SSD1306 OLED |
| PB10 / PB11 | I2C2 SCL / SDA | AT24C64 |
| PB0 | GPIO | 绿 LED，NORMAL |
| PB1 | GPIO | 黄 LED，WARNING |
| PA6 | GPIO | 红 LED，ALARM/FAULT |
| PA7 | GPIO | 有源蜂鸣器 |
| PA8 | GPIO | 继电器 IN，默认关阀 |
| PA5 | GPIO | 阀门状态 LED |
| PB12 | EXTI12 | KEY1 页面/选择 |
| PB13 | EXTI13 | KEY2 增加 |
| PB14 | EXTI14 | KEY3 减少 |
| PB15 | EXTI15 | KEY4 安全确认 |
| PB5 | GPIO 输入 | KEY5 设置项轮询 |
| PA13 / PA14 | SWDIO / SWCLK | ST-Link，保留 |

KEY1～KEY4 使用内部上拉、按下接 GND；KEY5 使用 PB5 内部上拉轮询。按键消抖和业务处理在主循环执行，不在 IRQ 中访问 OLED、I2C 或 EEPROM。

## 3. 电源和模拟输入

- MQ-4、MQ-6、MQ-7 加热器使用 5 V，三路共地；5 V 电源建议至少 1 A，并为继电器留余量。
- MCU、OLED、AT24C64 的逻辑电平使用 3.3 V。确认模块自带 I2C 上拉不会把 SDA/SCL 拉到 5 V。
- 每路 MQ AO 使用 10 kΩ / 18 kΩ 分压：`AO -> 10 kΩ -> ADC -> 18 kΩ -> GND`。5 V 输入时 ADC 约 3.21 V。
- ADC 节点可并联 100 nF 对地电容。DO 数字输出不接 MCU。

## 4. 总线和通信接线

### OLED I2C1

| OLED | STM32 |
| --- | --- |
| VCC | 3.3 V |
| GND | GND |
| SCL | PB6 |
| SDA | PB7 |

常见 SSD1306 地址为 `0x3C`，以实际模块为准。驱动使用 Page Addressing Mode，并按 dirty page 刷新。

### AT24C64 I2C2

| AT24C64 | STM32 |
| --- | --- |
| VCC | 3.3 V |
| GND | GND |
| SCL | PB10 |
| SDA | PB11 |

AT24C64 为 8192 字节、32 字节页、16 位存储地址。常见基础地址为 `0x50`，A0/A1/A2 以模块接法为准。写入必须等待内部写周期或使用 ACK polling。

### 串口

- USART1：PA9 -> USB-TTL RXD，PA10 <- USB-TTL TXD，默认 115200-8-N-1。
- USART2：PA2 -> HC-05 RXD，PA3 <- HC-05 TXD，常见透传速率 9600。
- 两个串口必须共地；USB-TTL 优先使用 3.3 V TTL 电平。
- USB 和 HC-05 在软件中使用独立协议队列，回复不会互串。

## 5. 各模块接线与所需电阻

### 5.1 电源划分

**前提**：STM32F103 的 VDD 绝对最大额定值是 **4.0 V**，推荐工作范围 **2.0～3.6 V**。**给 MCU 接 5 V 会烧芯片**，仿真里可能照跑，那是行为级模型的假象。

| 电源域 | 器件 |
| --- | --- |
| **3.3 V** | STM32、OLED、AT24C64/FM24C64、蜂鸣器模块、LED |
| **5 V** | MQ 加热器、继电器模块 DC+、HC-05、USB-TTL |

**所有器件必须共地**——5 V 侧和 3.3 V 侧共用同一根 GND。不共地时信号线没有共同的参考电平，通信必然失败。

**I2C 上拉必须上拉到 3.3 V，不能上拉到 5 V。** MCU 是 3.3 V 器件，上拉拉到 5 V 会让总线电平超出 MCU 的输入范围，轻则不通信，重则烧引脚。

### 5.2 电阻一览

| 位置 | 阻值 | 数量 | 作用 |
| --- | --- | --- | --- |
| I2C1 上拉（SCL、SDA 各一） | 4.7 kΩ → **3.3 V** | 2 | I2C 开漏输出必须有上拉 |
| I2C2 上拉（SCL、SDA 各一） | 4.7 kΩ → **3.3 V** | 2 | 同上 |
| MQ AO 分压（每路两个） | 10 kΩ + 18 kΩ | 6 | 把 5 V 信号压到 ADC 量程内 |
| LED 限流（每颗一个） | 330 Ω | 4 | 见 5.4 |
| PA8 继电器输入下拉 | 10 kΩ → GND | 1 | 复位到 GPIO 初始化之间保持关断 |
| 电机续流二极管 | 1N4007 | 1 | 见 5.6，**必须加** |
| 蜂鸣器基极（仅裸蜂鸣器方案） | 1 kΩ 串 + 10 kΩ 下拉 | 2 | 见 5.5 |
| 按键 | —— | 0 | **用 STM32 内部上拉，不要外加** |

OLED 和 AT24C64 的成品模块**板上自带 I2C 上拉**，用模块时不用再加；用裸芯片时必须自己加。

### 5.3 显示与存储（I2C）

| 模块 | 引脚 | 接到 |
| --- | --- | --- |
| OLED SSD1306 | VCC / GND | **3.3 V** / GND |
| | SCL / SDA | **PB6 / PB7** |
| AT24C64 | VCC / GND | **3.3 V** / GND |
| | SCL / SDA | PB10 / PB11 |
| | WP | GND（关写保护） |
| | A0 / A1 / A2 | GND（地址 0x50） |

OLED 常见地址 `0x3C`（8 位写地址 `0x78`），以模块实际为准。

### 5.4 指示灯

每颗 LED **必须串限流电阻**，否则电流不受控，会烧 LED 或烧引脚。

```text
PB0 ──[330Ω]──|>|── GND        绿灯，NORMAL
PB1 ──[330Ω]──|>|── GND        黄灯，WARNING
PA6 ──[330Ω]──|>|── GND        红灯，ALARM/FAULT
PA5 ──[330Ω]──|>|── GND        阀门指示
```

阻值算法：`(3.3 V − Vf) / I`。红色 LED 的 Vf 约 2.0 V，取 5 mA 时约 260 Ω，**取 330 Ω 得到约 4 mA**，够亮且留有余量。蓝/白 LED 的 Vf 约 3.0 V，用 330 Ω 时只有约 1 mA，会偏暗，需要减小到 100～150 Ω。

### 5.5 蜂鸣器

两种方案，**有效电平不同，固件里靠 `BUZZER_ON_LEVEL` 切换**（见 `bsp/bsp_buzzer.h`）。

**方案一：低电平触发的蜂鸣器模块**（本项目实物采用）

```text
模块 VCC ── 3.3 V
模块 GND ── GND
模块 I/O ── PA7          模块内部自带驱动，引脚拉低即响
```

固件里 `BUZZER_ON_LEVEL` 定义为 `GPIO_PIN_RESET`。

**方案二：裸蜂鸣器 + NPN 低边开关**

```text
        VCC(3.3V)
         │
       [BUZ1]
         │
         ├──────── C
PA7 ─[R 1k]──┬──── B      Q2 = NPN
             │            E ── GND
         [R 10k]
             │
            GND
```

引脚拉高即响，`BUZZER_ON_LEVEL` 定义为 `GPIO_PIN_SET`（默认值）。

> **注意**：`gpio.c` 的 `MX_GPIO_Init()` 会把 PA7 初始化成低电平，对方案一（低电平触发）来说上电瞬间就会响。`main.c` 在 `app_init()` 之前补了一句 `bsp_buzzer_set(false)` 把它摆回空闲电平，改极性时别把那句删掉。

### 5.6 继电器模块与负载

下面是**高电平触发**的继电器模块接法，与固件里 `RELAY_OPEN_LEVEL = GPIO_PIN_SET` 一致。

控制侧：

```text
5V        ── 模块 DC+      ← 不要接 3.3 V，线圈要 5 V
GND       ── 模块 DC-
STM32 PA8 ── 模块 IN
GND 与 DC- 必须共地
```

负载侧（用 3～6 V 小电机代替阀门）：

```text
5V     ── COM
NO     ── 电机 +
电机 − ── GND
NC     ── 不接
```

逻辑：`PA8 高 → 继电器吸合 → COM-NO 导通 → 电机转`；`PA8 低 → 释放 → 电机停`。复位或断电时 PA8 为低，电机停、阀门关——**这是失效安全的方向**，燃气阀就该这样。

**必须加续流二极管**。电机是感性负载，断电瞬间会产生反向高压尖峰，能把驱动管或电源打坏：

```text
         电机
NO ──── +     − ──── GND
        |     |
        └─|<|─┘      1N4007
```

**带白环（阴极）的一端接电机正极**，另一端接负极——也就是**反向并联**在电机两端，正常工作时截止，断电瞬间给电流提供泄放回路。

**电机要就近去耦**：并联 100 µF 电解 + 0.1 µF 陶瓷到地。电机启动和堵转会把 5 V 拉下来，MCU 可能因此复位。

**PA8 的 10 kΩ 下拉不能省**：复位后到 `MX_GPIO_Init()` 之间引脚是浮空输入，没有这个下拉，继电器在这段窗口里的状态不可控。

**3.3 V 能否触发 5 V 模块要实测**：接好后按一次，听继电器有没有「咔哒」声、看模块指示灯有没有变化。没反应的话，先查模块上有没有 **VCC / JD_VCC 跳线帽**——很多模块用它把光耦供电和线圈供电分开，把光耦那侧接 3.3 V 就能可靠触发。没有跳线就要加一级 NPN 抬电平。

### 5.7 按键

```text
PB12 ~ PB15（KEY1~KEY4）── 按键 ── GND
PB5（KEY5）             ── 按键 ── GND
```

**用 STM32 内部上拉**（`MX_GPIO_Init()` 已配置），按键另一端直接接地，**不要再外加外部上拉或下拉**。KEY1～KEY4 走 EXTI 下降沿，KEY5 在主循环轮询。

### 5.8 串口

| 模块 | 模块引脚 | 接到 | 说明 |
| --- | --- | --- | --- |
| USB-TTL | RXD | PA9 | 交叉接线 |
| | TXD | PA10 | |
| | GND | GND | |
| | VCC | 不接（或 3.3 V） | 板子另有供电时别接 |
| HC-05 | RXD | PA2 | 交叉接线 |
| | TXD | PA3 | |
| | VCC | **5 V** | 模块自带稳压，裸模块只能 3.3 V |
| | GND | GND | |

USB-TTL 的跳线要设成 **3.3 V 档**。HC-05 的 RXD 是 3.3 V 逻辑，PA2 输出 3.3 V 可以直接接；反过来 HC-05 的 TXD 输出 3.3 V 电平，接 PA3 也安全。

## 6. 状态和安全策略

| 状态 | 指示 | 阀门 |
| --- | --- | --- |
| WARMUP | 阀门关闭，暂不判定气体阈值 | CLOSED |
| NORMAL | 绿灯 | OPEN |
| WARNING | 黄灯 | OPEN |
| ALARM | 红灯、蜂鸣器、记录事件 | CLOSED |
| FAULT | 红灯、持续故障提示 | CLOSED |
| SAFE_WAIT | 等待 KEY4 | CLOSED |

MQ 预热 3 秒（`GAS_WARMUP_MS`）期间阀门保持关闭，ADC 或采样链路真正失败仍进入 FAULT。真实 MQ 传感器要热机几分钟以上才稳定，本项目按演示需要固定取 3 秒。ALARM 或 FAULT 解除后，只有环境恢复安全并按下 KEY4 才允许重新开阀。

## 7. 构建和联调

- Keil5 是课设主构建：打开 `stm32f103/MDK-ARM/smart_gas_monitor.uvprojx`。最终文件位于 `build/keil5/Artifacts/`，中间文件位于 `build/keil5/Listings/`。
- CMake/Ninja 是可选交叉验证：使用 `cmake --preset arm-debug` 或 `arm-release`，最终 `.elf/.hex/.bin` 位于对应 `build/arm-*` 目录。
- 上电顺序：先测 12 V / 5 V / 3.3 V，再验证 SWD、USART1、ADC、OLED、AT24C64、按键、LED/蜂鸣器、继电器，最后接 HC-05。
- 实物联调前确认继电器有效电平、OLED/EEPROM 上拉电压、MQ 分压节点电压和 HC-05 供电版本。
