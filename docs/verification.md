# 软件验证记录

验证对象为 STM32F103C8T6 的 HAL 固件和主机测试模型。实物、Proteus 和 PCB 尚未纳入本记录。

| 项目 | 结果 |
| --- | --- |
| MQ4/MQ6/MQ7 三路采样 | PASS |
| 五按键和 OLED 页面 | PASS |
| 报警、继电器和蜂鸣器节奏 | PASS |
| AT24C64 参数与历史 | PASS |
| lockout 安全锁存 | PASS |
| USART1/HC-05 协议 | PASS |
| Host Test | PASS |
| Keil5 工程编译 | PASS |
| 实物 / Proteus | NOT VERIFIED |

## Keil5 构建

打开 stm32f103/MDK-ARM/smart_gas_monitor.uvprojx，选择 smart_gas_monitor Target 并执行 Build。输出位于 build/keil5/。

文档中的 PASS 仅表示软件检查、主机测试或构建已完成，不代表实物验证已经通过。
