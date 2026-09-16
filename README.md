# 智能燃气监测与自动防护系统

基于 STM32F103C8T6、STM32 HAL 和 Keil5 的课程设计工程。系统使用 MQ4、MQ6、MQ7 三路模拟量进行燃气监测，并使用 AT24C64 保存参数和报警历史。

## Keil5 编译

使用 Keil5 打开 stm32f103/MDK-ARM/smart_gas_monitor.uvprojx，选择 smart_gas_monitor Target 后执行 Build。编译生成的 .axf、.hex、.map 和中间文件统一放在 build/keil5/。该目录是本地构建目录，不提交到 Git。

代码分层为：main → app → bsp → HAL。

## 主机测试

python tools/run_host_tests.py --cc cl
python tools/run_host_tests.py --cc cl --eeprom 2

## 主要功能

- 三路 MQ ADC 采样和状态判断
- OLED 实时、设置、历史页面
- 五按键交互和报警解除
- AT24C64 参数保存与报警历史
- lockout 安全锁存
- 按报警传感器数量变化的蜂鸣器节奏
- USART1/HC-05 文本协议
