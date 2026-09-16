# Review fixes (2026-09)

This note records the boundary fixes applied after the full code review.

- SSD1306 now uses page addressing (`0x20, 0x02`) to match dirty-page refresh.
- A blank EEPROM is initialized with defaults; EEPROM I/O failure is reported separately.
- Warm-up keeps the valve closed and suppresses gas threshold alarms.
- A newly added alarm channel restarts the buzzer window.
- FAULT has a dedicated OLED overlay and continuous buzzer behavior.
- USB and HC-05 use independent protocol queues, so replies do not cross ports.
- History recovery stops at the first invalid slot and skips the reserved `0xffff` sequence.
- Keil5 output is split into `build/keil5/Artifacts/` and `build/keil5/Listings/`.

Host tests require a native C compiler. Firmware builds were verified with both CMake presets using GNU Arm Embedded GCC and Ninja.
