# 手写构建扩展，CubeMX 继续管理 Makefile。
include Makefile

# 统一把中间文件和 HEX/BIN/ELF 放到仓库根目录 build/。


APP_SOURCES := ../app/app.c ../app/gas/gas.c ../app/alarm/alarm.c \
               ../app/config/config.c ../app/history/history.c \
               ../app/protocol/protocol.c ../app/display/display.c \
               ../bsp/bsp_adc.c ../bsp/bsp_uart.c ../bsp/bsp_i2c.c \
               ../bsp/bsp_oled.c ../bsp/bsp_key.c ../bsp/bsp_led.c \
               ../bsp/bsp_buzzer.c ../bsp/bsp_relay.c ../bsp/bsp_eeprom.c
APP_OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(APP_SOURCES:.c=.o)))
CFLAGS += -I../app -I../bsp
vpath %.c $(sort $(dir $(APP_SOURCES)))
OBJECTS += $(APP_OBJECTS)
$(BUILD_DIR)/$(TARGET).elf: $(APP_OBJECTS)
$(APP_OBJECTS): GNUmakefile
