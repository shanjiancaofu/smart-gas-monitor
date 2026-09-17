#!/usr/bin/env bash
# 用 ST-Link 烧写 / 调试 smart-gas-monitor。
#
# 为什么不用 STM32CubeIDE 自带的那个 openocd + st_scripts：
# 那两个凑一起会在 swj-dp.tcl 里 "Infinite eval recursion"（见 openocd_flash.log），
# 一次也没烧进去过。ESP-IDF 带的那份 OpenOCD 有完整的标准脚本，直接用它。
#
# 用法:
#   tools/openocd.sh flash [elf]        烧写（默认取 build/arm-debug-hw 里最新的 elf）
#   tools/openocd.sh server             起 GDB server，端口 3333
#   tools/openocd.sh gdb [elf]          reset halt 后进交互式 gdb
#   tools/openocd.sh run <脚本.gdb> [elf]   跑一段 gdb 脚本（自起自停 server）
set -euo pipefail

ESP=/d/tool/Espressif/tools/openocd-esp32/v0.12.0-esp32-20251215/openocd-esp32
OCD="$ESP/bin/openocd.exe"
SCRIPTS="$ESP/share/openocd/scripts"
TOOLCHAIN=/e/STM32CubeIDE/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin

for tool in "$OCD" "$SCRIPTS/target/stm32f1x.cfg" "$TOOLCHAIN/arm-none-eabi-gdb.exe"; do
    [ -e "$tool" ] || { echo "找不到: $tool" >&2; exit 1; }
done

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

latest_elf() {
    ls -t build/arm-debug-hw/smart_gas_monitor_*.elf | head -1
}

common=(-s "$SCRIPTS" -f interface/stlink.cfg -f target/stm32f1x.cfg)

start_server() {
    rm -f /tmp/ocd_server.log
    "$OCD" "${common[@]}" >/tmp/ocd_server.log 2>&1 &
    OCD_PID=$!
    trap 'kill $OCD_PID 2>/dev/null || true' EXIT
    sleep 3
}

case "${1:-server}" in
flash)
    ELF="${2:-$(latest_elf)}"
    echo "烧写 $ELF"
    MSYS_NO_PATHCONV=1 "$OCD" "${common[@]}" -c "program {$ELF} verify reset exit"
    ;;
server)
    "$OCD" "${common[@]}"
    ;;
gdb)
    ELF="${2:-$(latest_elf)}"
    start_server
    PATH="$TOOLCHAIN:$PATH" arm-none-eabi-gdb -q "$ELF" -ex "target extended-remote localhost:3333"
    ;;
run)
    SCRIPT="${2:?用法: openocd.sh run <脚本.gdb> [elf]}"
    ELF="${3:-$(latest_elf)}"
    start_server
    PATH="$TOOLCHAIN:$PATH" timeout 150 arm-none-eabi-gdb -batch -q -x "$SCRIPT" "$ELF" 2>&1 \
        | grep -vE "^warning:|^Reading|^Remote debugging|^\[stm32f1x|^Breakpoint [0-9]+ at|^0x0800.*in \?\?|^Note:|^$" || true
    ;;
*)
    sed -n '7,13p' "$0"
    exit 1
    ;;
esac
