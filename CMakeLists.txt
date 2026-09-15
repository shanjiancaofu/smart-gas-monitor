cmake_minimum_required(VERSION 3.22)
project(smart_gas_monitor LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(APP_ROOT "${CMAKE_SOURCE_DIR}/stm32f103")
set(CUBEMX_ROOT "${APP_ROOT}/cubemx")
set(APP_SOURCES
  ${CUBEMX_ROOT}/Core/Src/main.c
  ${CUBEMX_ROOT}/Core/Src/gpio.c
  ${CUBEMX_ROOT}/Core/Src/adc.c
  ${CUBEMX_ROOT}/Core/Src/i2c.c
  ${CUBEMX_ROOT}/Core/Src/usart.c
  ${CUBEMX_ROOT}/Core/Src/tim.c
  ${CUBEMX_ROOT}/Core/Src/stm32f1xx_it.c
  ${CUBEMX_ROOT}/Core/Src/stm32f1xx_hal_msp.c
  ${CUBEMX_ROOT}/Core/Src/system_stm32f1xx.c
  ${CUBEMX_ROOT}/Core/Src/sysmem.c
  ${CUBEMX_ROOT}/Core/Src/syscalls.c
  ${APP_ROOT}/app/app.c
  ${APP_ROOT}/app/gas/gas.c
  ${APP_ROOT}/app/alarm/alarm.c
  ${APP_ROOT}/app/config/config.c
  ${APP_ROOT}/app/history/history.c
  ${APP_ROOT}/app/protocol/protocol.c
  ${APP_ROOT}/app/display/display.c
  ${APP_ROOT}/bsp/bsp_adc.c
  ${APP_ROOT}/bsp/bsp_uart.c
  ${APP_ROOT}/bsp/bsp_i2c.c
  ${APP_ROOT}/bsp/bsp_oled.c
  ${APP_ROOT}/bsp/bsp_key.c
  ${APP_ROOT}/bsp/bsp_led.c
  ${APP_ROOT}/bsp/bsp_buzzer.c
  ${APP_ROOT}/bsp/bsp_relay.c
  ${APP_ROOT}/bsp/bsp_eeprom.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
)
set(STARTUP ${CUBEMX_ROOT}/startup_stm32f103xb.s)
file(GLOB OLD_FIRMWARE_ARTIFACTS
  "${CMAKE_BINARY_DIR}/smart_gas_monitor_20*"
  "${CMAKE_BINARY_DIR}/smart_gas_monitor_*.elf"
  "${CMAKE_BINARY_DIR}/smart_gas_monitor_*.hex"
  "${CMAKE_BINARY_DIR}/smart_gas_monitor_*.bin"
  "${CMAKE_BINARY_DIR}/smart_gas_monitor_*.map")
file(REMOVE ${OLD_FIRMWARE_ARTIFACTS})
string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d_%H%M%S")
set(FIRMWARE_BASENAME "smart_gas_monitor_${BUILD_TIMESTAMP}")
add_executable(smart_gas_monitor ${APP_SOURCES} ${STARTUP})
set_target_properties(smart_gas_monitor PROPERTIES
  OUTPUT_NAME "${FIRMWARE_BASENAME}"
  SUFFIX ".elf")
target_include_directories(smart_gas_monitor PRIVATE
  ${CUBEMX_ROOT}/Core/Inc
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc
  ${CUBEMX_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
  ${CUBEMX_ROOT}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
  ${CUBEMX_ROOT}/Drivers/CMSIS/Include
  ${APP_ROOT}/app ${APP_ROOT}/bsp)
target_compile_definitions(smart_gas_monitor PRIVATE USE_HAL_DRIVER STM32F103xB)
target_compile_options(smart_gas_monitor PRIVATE -mcpu=cortex-m3 -mthumb -ffunction-sections -fdata-sections -Wall -Wextra -fstack-usage $<$<CONFIG:Debug>:-Og -g3> $<$<CONFIG:Release>:-Os -g3>)
target_link_options(smart_gas_monitor PRIVATE -mcpu=cortex-m3 -mthumb --specs=nano.specs -T${CUBEMX_ROOT}/STM32F103xx_FLASH.ld -Wl,--gc-sections -Wl,-Map=${CMAKE_BINARY_DIR}/${FIRMWARE_BASENAME}.map)
add_custom_command(TARGET smart_gas_monitor POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:smart_gas_monitor> ${CMAKE_BINARY_DIR}/${FIRMWARE_BASENAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:smart_gas_monitor> ${CMAKE_BINARY_DIR}/${FIRMWARE_BASENAME}.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:smart_gas_monitor>)
