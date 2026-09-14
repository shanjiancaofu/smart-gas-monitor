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

已用 STM32CubeMX 生成 STM32F103C8T6 HAL 工程，配置为 `stm32f103/cubemx/smart_gas_monitor.ioc`。已实现三路采样、报警锁存、KEY4 安全恢复、按键阈值与采样周期调整及 AT24C02 参数保存。OLED 显示、串口命令和报警历史持久化尚未实现。

硬件空目录通过 `.gitkeep` 纳入 Git 管理。固件分层如下：

```text
stm32f103/
├── bsp/                    # 只访问 HAL，不依赖 app
│   ├── mq_sensor.*         # PA0/PA1/PA4 三路 ADC 采集
│   ├── key.*               # PB12～PB15 扫描与消抖
│   ├── alarm_output.*      # 继电器、LED、蜂鸣器
│   └── at24c02.*           # I2C2 分页读写
└── app/                    # 只依赖标准 C，可在主机上测试
    ├── app.*               # 模块装配与主循环调度
    ├── gas/                # 阈值、监测状态机、报警锁存和人工恢复
    └── settings/           # 参数序列化、CRC、双副本保存
```

`cubemx/Core/`、`cubemx/Drivers/` 由 CubeMX 生成；BSP 访问 HAL，app 不依赖 HAL，`app.c` 是唯一把两者装配起来的地方。根目录 `tests/` 保存主机 C 测试。

## 项目文档

- [硬件设计与接线说明](docs/智能燃气监测与自动防护系统_硬件设计与接线说明.docx)
- [固件分层、配置与构建](docs/firmware.md)
- [验证结果与待验证项](docs/verification.md)

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

固件输出在 `stm32f103/cubemx/build/`。`cubemx/GNUmakefile` 加入外层 app/bsp 源码，CubeMX 可重新生成其管理的 `Makefile`。主机测试脚本自动编译并运行 `tests/` 下的全部测试。

当前默认阈值为 ADC 原始计数演示参数，未标定为 ppm。已完成软件构建和主机测试，未进行实物或 Proteus 验证。
