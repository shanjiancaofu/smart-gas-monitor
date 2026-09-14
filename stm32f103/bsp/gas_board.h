#ifndef GAS_BOARD_H
#define GAS_BOARD_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stddef.h>

/* Verify against the actual relay contacts: energised means valve OPEN. */
#ifndef GAS_RELAY_OPEN_LEVEL
#define GAS_RELAY_OPEN_LEVEL GPIO_PIN_SET
#endif
#ifndef GAS_BUZZER_ON_LEVEL
#define GAS_BUZZER_ON_LEVEL GPIO_PIN_SET
#endif
#ifndef GAS_EEPROM_ADDRESS
#define GAS_EEPROM_ADDRESS (0x50u << 1)
#endif
typedef struct {
    ADC_HandleTypeDef *adc;
    I2C_HandleTypeDef *eeprom;
    uint32_t key_changed[4];
    uint8_t key_raw, key_stable;
} gas_board_t;
bool gas_board_init(gas_board_t *b, ADC_HandleTypeDef *adc, I2C_HandleTypeDef *eeprom);
bool gas_board_sample(gas_board_t *b, uint16_t values[3]);
void gas_board_outputs(bool open, bool green, bool yellow, bool alarm);
void gas_board_close(void);
/* Poll every loop; returns debounced press bits, one event per press. */
uint8_t gas_board_keys(gas_board_t *b, uint32_t now);
bool gas_board_eeprom_read(void *context, uint16_t address, uint8_t *data, size_t size);
bool gas_board_eeprom_write(void *context, uint16_t address, const uint8_t *data, size_t size);
#endif
