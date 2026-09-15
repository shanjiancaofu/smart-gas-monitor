# 智能燃气监测与自动防护系统

基于 STM32F103 的智能燃气监测与自动防护系统，仓库用于管理固件、硬件设计及项目文档。

## 目录结构

```text
smart-gas-monitor/
├── stm32f103/
│   ├── cubemx/             # CubeMX 工程和生成文件
│   │   ├── Core/           # 生成的初始化代码
│   │   ├── Drivers/        # STM32F1 HAL 和 CMSIS
│   │   └── smart_gas_monitor.ioc
│   ├── bsp/                # 自定义硬件驱动
│   └── app/                # 业务逻辑
├── hardware/
│   ├── schematic/          # 原理图
│   ├── pcb/                # PCB 设计文件
│   └── proteus/            # Proteus 仿真工程
├── docs/                   # 设计文档、课设报告及使用说明
├── tools/                  # HAL 生成和主机测试脚本
├── tests/                  # 主机 C 单元测试
├── .gitignore
├── README.md
└── LICENSE
```

已用 STM32CubeMX 生成 STM32F103C8T6 HAL 工程，配置为 `stm32f103/cubemx/smart_gas_monitor.ioc`。固件已实现三路 ADC 采样（每路 8 次平均）、可跨掉电保持的安全锁存与 KEY4 现场恢复、按键阈值、采样周期与报警蜂鸣器时长的调整、AT24C02 参数保存、OLED 三界面显示、EEPROM 报警历史记录，以及 USART1/HC-05 双路共用的文本协议。按键走 PB12～PB15 的 EXTI 下降沿，采样节拍由 TIM2 的 10 ms 中断驱动，两路串口均为中断收发。软件部分已完成，尚未进行实物或 Proteus 验证。

硬件空目录通过 `.gitkeep` 纳入 Git 管理。固件分层如下：

```text
stm32f103/
├── app/
│   ├── app.c/.h             # 应用入口、初始化和任务调度
│   ├── gas/gas.c/.h         # 通道映射、八次平均、阈值和状态机
│   ├── alarm/alarm.c/.h     # 报警输出策略及 TIM2 响铃计时
│   ├── config/config.c/.h   # 参数类型、默认值、校验和 EEPROM 双副本
│   ├── history/history.c/.h # 报警历史增、读、清除
│   ├── protocol/protocol.c/.h # 两路串口共用的命令解析
│   └── display/display.c/.h # OLED 页面
├── bsp/
│   ├── bsp_adc.c/.h         # 单次 ADC 硬件通道读取
│   ├── bsp_uart.c/.h        # UART 中断收发
│   ├── bsp_i2c.c/.h         # I2C 硬件传输
│   ├── bsp_oled.c/.h        # SSD1306 页缓冲、字模和刷新
│   ├── bsp_key.c/.h         # EXTI 事件、按键消抖
│   ├── bsp_led.c/.h         # 状态灯和阀门指示
│   ├── bsp_buzzer.c/.h      # 蜂鸣器 GPIO 开关
│   ├── bsp_relay.c/.h       # 继电器 GPIO 开关
│   └── bsp_at24c02.c/.h     # EEPROM 分页访问
└── cubemx/                 # 原有 CubeMX 工程
```

`main` 只调用 `app_init()` 和 `app_update()`。应用状态及外设对象保存在 `app.c` 内部，主循环函数仅按顺序调度任务。BSP 不依赖应用层，ADC 驱动也不需要知道 MQ 型号。`gas/config/history/protocol` 可直接做主机测试，`alarm` 通过模拟 BSP 输出测试。

代码采用四空格缩进、条件语句花括号、中文职责注释，风格配置见 `.clang-format`。格式化范围仅限手写 app/bsp，不改 CubeMX 生成区。

## 串口协议

USART1（115200，USB-TTL）和 USART2（9600，HC-05）走同一套文本协议，行以 `\r\n` 结束，命令大小写不敏感。支持 `STATUS?`、`CONFIG?`、`HISTORY?`、`SET MQ4|MQ7|MQ8 <值>`、`SET PERIOD <毫秒>`、`SET BUZZER <OFF|ALWAYS|1-60>` 和 `VALVE CLOSE`；报警开始时两路都会主动推一行 `ALARM`。

没有远程开阀：`VALVE OPEN` 一律回 `ERR ONLY CLOSE`，解除阀门锁存的唯一途径是面板上的 KEY4。ALARM、FAULT 和远程 CLOSE 都会立即持久化 lockout，断电重启后仍保持关阀。命令与应答格式见[固件说明](docs/firmware.md)。

## 项目文档

- [硬件设计与接线说明](docs/智能燃气监测与自动防护系统_硬件设计与接线说明.docx)
- [Proteus 仿真搭建与演示脚本](hardware/proteus/README.md)
- [固件分层、配置与构建](docs/firmware.md)
- [验证结果与待验证项](docs/verification.md)
- [变更记录](docs/changelog.md)
- [硬件接口表](docs/hardware_interface.md)
- [串口协议](docs/uart_protocol.md)

## 代码组织

- `cubemx/`：存放 `.ioc`、Core、Drivers、启动文件、链接脚本、生成的 Makefile 和构建扩展；手工修改生成代码时使用 `USER CODE` 区域。
- `bsp/`：存放传感器、执行器及其他外设的自定义驱动，一个模块负责一类硬件。
- `app/`：存放数据采集、报警判断及自动防护等业务逻辑。

## 许可证

手写代码采用 [MIT License](LICENSE)。STM32Cube HAL/CMSIS 保留其供应商许可和版权声明，见 `stm32f103/cubemx/Drivers/`。

## 构建与测试

将 GNU Arm 工具链和 GNU Make 加入 PATH 后执行：

```sh
make -C stm32f103/cubemx -j4
python tools/run_host_tests.py --cc gcc
```

固件输出在 `stm32f103/cubemx/build/`。`cubemx/GNUmakefile` 加入外层 app/bsp 源码，CubeMX 可重新生成其管理的 `Makefile`。主机测试的清单在 `tools/run_host_tests.py` 里显式列出——每个测试需要链接哪几个 `.c` 是脚本推断不出来的，所以新增测试要同时在那个表里登记。

当前默认阈值为 ADC 原始计数演示参数，未标定为 ppm。软件部分已完成，五个主机测试套件全部通过，未进行实物或 Proteus 验证。
