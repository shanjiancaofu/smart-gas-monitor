# 智能燃气监测与自动防护系统

基于 STM32F103C8T6、STM32 HAL 和 Keil5 的课程设计工程。系统使用 MQ4、MQ6、MQ7 三路模拟量进行燃气监测，并使用 AT24C64 保存参数和报警历史。

## Keil5 编译

使用 Keil5 打开 `stm32f103/MDK-ARM/smart_gas_monitor.uvprojx`，按目标选 Target：

| Target | 用途 | 舵机 | MQ6 |
| --- | --- | --- | --- |
| `smart_gas_monitor_hw` | 实物板 | PB8 / TIM4_CH3 | PA1 |
| `smart_gas_monitor_soft` | Proteus 仿真 | PA1 / TIM2_CH2 | PA5 |

两个 Target 靠 `USE_SOFT_I2C` 区分，引脚差异只在 BSP 里的 `#if` 分叉，实物那条路径不受仿真影响。
`tools/build_keil.ps1` 一次编两个并收集产物。

- 可烧录文件：`build/keil5/<hw|soft>/Artifacts/smart_gas_monitor_keil_<hw|soft>.hex`
- 调试文件：同目录下的 `.axf`、`.map`
- 中间文件：`build/keil5/<hw|soft>/Listings/`

Keil 的 ARMCC 工具链不会自动生成 BIN；需要 BIN 时，在工程目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools/keil_make_bin.ps1
```

生成的 `.bin` 也会放在同一目录。`build/` 是本地构建目录，不提交到 Git。

代码分层为：main → app → bsp → HAL。

## 主机测试

```bash
python tools/run_host_tests.py --cc cl            # 需要 MSVC 环境
python tools/run_host_tests.py --cc cl --eeprom 2 # 换成 AT24C02 参数
```

Windows 上不想手工 `vcvars`，直接跑封装：

```bat
tools\run_host_tests.bat
```

## 其他工具

| 工具 | 用途 |
| --- | --- |
| `tools/build_arm.sh <preset>` | CMake/Ninja 交叉编译，四个 preset：`arm-{debug,release}-{hw,soft}`。工具链在 STM32CubeIDE 目录下，不在 PATH 上，脚本自己挂 |
| `tools/openocd.sh flash\|server\|gdb\|run` | ST-Link 烧写与调试。**不要用 STM32CubeIDE 自带的 OpenOCD**，它的脚本组合会递归报错连不上 |
| `tools/serial_check.py COM11` | 走串口跑一遍协议功能测试，逐条比对真实应答；加 `--alarm` 额外测报警通路 |
| `tools/gen_oled_font.py` | 由参考工程的字库重新生成 `bsp_oled.c` 里的字模表，字模要改就重跑它 |
| `tools/render_oled.py dump.bin` | 把读回的 SSD1306 帧缓冲渲染成文字，用来在不看屏幕的情况下确认面板内容 |
| `tools/ocd_dump_pages.gdb` | 配合上面的脚本，把 OLED 各页面逐页 dump 出来 |

## 提交规范

提交信息使用以下格式，冒号后用英文小写动词开头，简要说明本次变更：

```text
[feature]: add ...
[fix]: handle ...
[refactor]: organize ...
[docs]: update ...
```

## 主要功能

- 三路 MQ ADC 采样和状态判断
- OLED 实时、设置、历史页面
- 五按键交互和报警解除
- AT24C64 参数保存与报警历史
- lockout 安全锁存
- 按报警传感器数量变化的蜂鸣器节奏
- USART1/HC-05 文本协议
