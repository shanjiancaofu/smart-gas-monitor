#!/usr/bin/env bash
# 用 STM32CubeIDE 自带的 cmake/ninja/arm-gcc 构建固件。
# 这三个都不在系统 PATH 上，所以每次构建都要先把它们的 bin 目录挂上去。
set -euo pipefail

PLUGINS="/e/STM32CubeIDE/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins"
TOOLS="$PLUGINS/com.st.stm32cube.ide.mcu.externaltools"

export PATH="$TOOLS.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin:$PATH"
export PATH="$TOOLS.cmake.win32_1.1.200.202605190741/tools/bin:$PATH"
export PATH="$TOOLS.ninja.win32_1.1.200.202606260906/tools/bin:$PATH"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PRESET="${1:-arm-debug}"

# --preset 是从当前目录找 CMakePresets.json 的，所以必须先回到仓库根。
cd "$ROOT"
cmake --preset "$PRESET" >/dev/null
cmake --build --preset "$PRESET" -j 4
