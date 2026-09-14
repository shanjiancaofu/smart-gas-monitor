# Handwritten build extension. CubeMX owns Makefile; regenerate it freely.
include Makefile

APP_SOURCES := ../app/monitor/gas_monitor.c ../app/parameters/gas_store.c \
               ../app/runtime/gas_app.c ../bsp/gas_board.c
APP_OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(APP_SOURCES:.c=.o)))
CFLAGS += -I../app -I../bsp
vpath %.c $(sort $(dir $(APP_SOURCES)))
OBJECTS += $(APP_OBJECTS)
$(BUILD_DIR)/$(TARGET).elf: $(APP_OBJECTS)
$(APP_OBJECTS): GNUmakefile
