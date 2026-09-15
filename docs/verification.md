# 软件验证记录

验证日期：2026 年 9 月 15 日。

## 当前基线

当前结构为 `main → app → bsp → HAL`。三路检测为 MQ4/MQ6/MQ7，五键职责、三页 OLED、报警覆盖、通用 EEPROM、扩大历史区和多传感器蜂鸣器模式均已接入。

| 检查项目 | 结果 |
| --- | --- |
| 五键业务 | PASS：KEY1 页面、KEY2 增加、KEY3 减少、KEY4 安全解除、KEY5 设置项 |
| 页面与设置项 | PASS：页面保存在 display，设置项保存在 gas；KEY1 和 KEY5 互不混用 |
| MQ 型号 | PASS：PA0/PA1/PA4 对应 MQ4/MQ6/MQ7；协议和显示无 MQ8 |
| ADC | PASS：每路八次平均，任一次读取失败使整组无效 |
| 报警 | PASS：1/2/3 路报警使用单滴/双滴/三滴非阻塞模式 |
| EEPROM | PASS：AT24C64 为 8192 字节、16 位地址、32 字节页；AT24C02 模式也通过主机测试和 BSP 交叉编译 |
| 历史 | PASS：AT24C64 下 510 条循环记录，计数、槽位和索引使用 16 位 |
| lockout | PASS：ALARM、FAULT、远程 CLOSE 设置锁存，KEY4 在安全条件满足后解除 |
| 协议 | PASS：STATUS、CONFIG、HISTORY、SET、VALVE CLOSE，禁止远程 OPEN |
| Host Test | PASS：七套测试在 EEPROM_MODEL=64 和 EEPROM_MODEL=2 下全部通过 |
| ARM 构建 | PASS：GNU Arm 14.3.rel1，零 error；最终 warning 检查见本轮构建日志 |
| 实物 / Proteus | NOT VERIFIED |

## 最终构建

最终 clean build：

```text
text = 28984 bytes
data = 92 bytes
bss  = 4172 bytes
BIN  = 29076 bytes
SHA256 = 01709b4868f8c401a20b32210a9d919ee14ca885375076669243781704cc9c68
```固件位于 `stm32f103/cubemx/build/`。

## 待验证

- MQ 模块输出、分压、预热和实际气体响应。
- SSD1306 地址、方向和页面布局。
- AT24C64 16 位寻址、32 字节页写和掉电恢复。
- 继电器有效电平、PA8 下拉、LED 和蜂鸣器极性。
- 五键实际消抖和长时间操作。
- HC-05 连续收发以及 510 条历史查询的完整性。

文档中的 PASS 仅表示软件检查、主机测试或构建完成，不代表实物验证通过。
