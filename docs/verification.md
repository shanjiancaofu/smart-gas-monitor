# 软件验证记录

日期：2026-09-14。

目录迁移验证：CubeMX 相关文件统一迁入 `stm32f103/cubemx/` 后，已通过新路径重新生成 HAL，并从无旧缓存的状态完整构建。外层转发文件随后移除，当前构建命令为 `make -C stm32f103/cubemx -j4`，产物位于 `stm32f103/cubemx/build/`。

| 检查 | 结果 |
| --- | --- |
| CubeMX 实际生成 | PASS：6.18.0，STM32CubeF1 V1.8.7，生成 Core、Drivers、启动文件、链接脚本、Makefile |
| CubeMX 重新生成 | PASS：main.c 的应用初始化/轮询及 Error_Handler、异常处理中的关阀 USER CODE 保留 |
| ARM 编译和链接 | PASS：GNU Arm 14.3.rel1，Cortex-M3，生成 ELF/HEX/BIN，无编译 warning/error |
| 主机 C 单元测试 | PASS：MSVC 19.38，C11，/W4 /WX |
| 实物 / Proteus | NOT VERIFIED |

主机测试覆盖三路分别触发预警/报警的边界、危险时禁止人工开阀、恢复后维持锁存、连续安全 3 秒后确认、采样失败/越界/超时、恢复采样不能掩盖停顿、32 位时间回绕、阈值上下限、延时保存、空 EEPROM、CRC 损坏回退，以及双副本写入各字节处中断后保留旧配置。

最终 ARM 构建：text = 12644 bytes，data = 12 bytes，bss = 2028 bytes。BIN 为 12656 bytes；链接脚本预留 heap/stack，目标容量为 Flash 64 KB、RAM 20 KB。

BIN SHA256：`e446a48b1489a36a74975995291668a140d0f377a3bd3310f6a56440721f711d`。

上述软件结果不代表传感器标定或硬件功能通过。待验证：8 MHz 晶振、ADC 电压和分压接线、MQ 模块预热/响应、继电器接点与有效电平、EEPROM 地址/写周期、按键实物消抖。OLED、双串口应用协议和报警历史持久化尚未实现。
