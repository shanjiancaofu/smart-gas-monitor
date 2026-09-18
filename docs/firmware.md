# Firmware build and runtime notes

The STM32F103 firmware is organized as `main -> app -> gas/alarm/config/history/protocol/display -> bsp -> HAL`.

## Keil5 (course primary build)

Open `stm32f103/MDK-ARM/smart_gas_monitor.uvprojx` and build one of the two targets. Which one matters — they differ in which pins the servo and MQ6 use:

- `smart_gas_monitor_hw` — the physical board. Servo on PB8/TIM4_CH3, MQ6 on PA1.
- `smart_gas_monitor_soft` — Proteus. Servo on PA1/TIM2_CH2, MQ6 on PA5, valve LED on PB9. Proteus loads this one.

Both define `USE_SOFT_I2C` (0 and 1) and the firmware branches on it. `tools/build_keil.ps1` builds both and collects the artifacts.

- Final files: `build/keil5/<hw|soft>/Artifacts/` (`.axf`, `.hex`, `.map`)
- Intermediate files: `build/keil5/<hw|soft>/Listings/`
- Generate a binary with `tools/keil_make_bin.ps1` after adding Keil ARMCC `fromelf.exe` to PATH.

## CMake/Ninja (cross-check build)

Four presets, crossing optimisation level with I2C backend: `arm-debug-hw`, `arm-debug-soft`, `arm-release-hw`, `arm-release-soft`. Use `cmake --preset <name>` then `cmake --build --preset <name>`; `tools/build_arm.sh <name>` wraps both and prepends the STM32CubeIDE toolchain to PATH. Final `.elf`, `.hex`, and `.bin` files are written directly to the selected `build/arm-*` directory.

MQ thresholds are raw 12-bit ADC counts, not calibrated ppm values. During the MQ warm-up (`GAS_WARMUP_MS`, 3 s) the valve remains closed and gas threshold alarms are suppressed; ADC and sampling faults still enter `FAULT`.
