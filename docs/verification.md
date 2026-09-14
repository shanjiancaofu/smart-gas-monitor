# 软件验证记录

日期：2026-09-14。

## 本次重构后（当前 HEAD）

目录重构与状态机修复后重新验证，先 `make clean` 再从无缓存状态完整构建。

| 检查 | 结果 |
| --- | --- |
| ARM 编译和链接 | PASS：GNU Arm 14.3.rel1，Cortex-M3，零 warning/error |
| 主机 C 单元测试 | PASS：MSVC 19.38，C11，/W4 /WX，`tests/test_gas_monitor.c` 与 `tests/test_settings.c` 全部通过 |
| 实物 / Proteus | NOT VERIFIED |

最终 ARM 构建：text = 12980 bytes，data = 12 bytes，bss = 2028 bytes。BIN 为 12992 bytes。

BIN SHA256：`ac29369653cb6d382ace7ad0185eabb6386b38b44e53db3a0a118040fa13dd01`。

交叉工具链无需单独安装，STM32CubeIDE 自带：`<CubeIDE>/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_*/tools/bin` 和同级的 `com.st.stm32cube.ide.mcu.externaltools.make.win32_*/tools/bin`，两者都加入 PATH 后即可执行上面的 make 命令。

重构本身是纯搬移：`gas_board.c` 拆成 `bsp/mq_sensor`、`bsp/key`、`bsp/alarm_output`、`bsp/at24c02`，`app/runtime` 改为 `app/app.c`，`app/monitor` 改为 `app/gas`，`app/parameters` 改为 `app/settings`。行为改动只有状态机部分，见下。

## 上一次验证（提交 ac803a9）

目录迁移验证：CubeMX 相关文件统一迁入 `stm32f103/cubemx/` 后，已通过新路径重新生成 HAL，并从无旧缓存的状态完整构建。

| 检查 | 结果 |
| --- | --- |
| CubeMX 实际生成 | PASS：6.18.0，STM32CubeF1 V1.8.7，生成 Core、Drivers、启动文件、链接脚本、Makefile |
| CubeMX 重新生成 | PASS：main.c 的应用初始化/轮询及 Error_Handler、异常处理中的关阀 USER CODE 保留 |
| ARM 编译和链接 | PASS：GNU Arm 14.3.rel1，Cortex-M3，生成 ELF/HEX/BIN，无编译 warning/error |
| 主机 C 单元测试 | PASS：MSVC 19.38，C11，/W4 /WX |
| 实物 / Proteus | NOT VERIFIED |

最终 ARM 构建：text = 12644 bytes，data = 12 bytes，bss = 2028 bytes。BIN 为 12656 bytes；链接脚本预留 heap/stack，目标容量为 Flash 64 KB、RAM 20 KB。

BIN SHA256：`e446a48b1489a36a74975995291668a140d0f377a3bd3310f6a56440721f711d`。

该哈希对应 ac803a9 的构建产物，本次改动后已不再适用。

## 测试覆盖

主机测试覆盖：三路分别触发预警/报警的边界、危险时禁止人工开阀、恢复后维持锁存、连续安全 3 秒后确认、采样失败/越界/超时、恢复采样不能掩盖停顿、32 位时间回绕、阈值上下限、采样周期设置与超时联动、延时保存、首次采样前不误判故障、上电自动开阀、低阈值下仍可恢复。

存储测试覆盖：空 EEPROM、CRC 损坏回退、版本不匹配回退、越界值拒绝，以及双副本写入各字节处中断后保留旧配置。

本次新增的关键回归用例是低阈值恢复：把阈值降到下限 200 后，旧实现要求读数低于 60 才允许恢复，实际不可能满足，阀门会被永久锁死。该用例在旧实现上确认失败，在当前实现上通过。

## 待验证

上述软件结果不代表传感器标定或硬件功能通过。待验证：8 MHz 晶振、ADC 电压和分压接线、MQ 模块预热/响应、继电器接点与有效电平、复位期间 PA8 的默认状态、EEPROM 地址/写周期、按键实物消抖。OLED、双串口应用协议和报警历史持久化尚未实现。
