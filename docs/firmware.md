# Firmware build and runtime notes

The STM32F103 firmware is organized as `main -> app -> gas/alarm/config/history/protocol/display -> bsp -> HAL`.

## Keil5 (course primary build)

Open `stm32f103/MDK-ARM/smart_gas_monitor.uvprojx` and build the `smart_gas_monitor` target.

- Final files: `build/keil5/Artifacts/` (`.axf`, `.hex`, `.map`)
- Intermediate files: `build/keil5/Listings/`
- Generate a binary with `tools/keil_make_bin.ps1` after adding Keil ARMCC `fromelf.exe` to PATH.

## CMake/Ninja (cross-check build)

Use `cmake --preset arm-debug` or `cmake --preset arm-release`, then `cmake --build --preset arm-debug` or `cmake --build --preset arm-release`. Final `.elf`, `.hex`, and `.bin` files are written directly to the selected `build/arm-*` directory.

MQ thresholds are raw 12-bit ADC counts, not calibrated ppm values. During the 60-second MQ warm-up the valve remains closed and gas threshold alarms are suppressed; ADC and sampling faults still enter `FAULT`.
