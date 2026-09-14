# Handwritten build extension. CubeMX owns Makefile; regenerate it freely.
include Makefile

# Object files are flattened into one directory, so no two sources may share a
# basename.
APP_SOURCES := ../app/app.c ../app/gas/gas_monitor.c ../app/settings/settings.c \
               ../bsp/alarm_output.c ../bsp/at24c02.c ../bsp/key.c ../bsp/mq_sensor.c
APP_OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(APP_SOURCES:.c=.o)))
CFLAGS += -I../app -I../bsp
vpath %.c $(sort $(dir $(APP_SOURCES)))
OBJECTS += $(APP_OBJECTS)
$(BUILD_DIR)/$(TARGET).elf: $(APP_OBJECTS)
$(APP_OBJECTS): GNUmakefile
