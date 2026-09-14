#ifndef ALARM_OUTPUT_H
#define ALARM_OUTPUT_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* Verify against the actual relay contacts: energised means valve OPEN. */
#ifndef RELAY_OPEN_LEVEL
#define RELAY_OPEN_LEVEL GPIO_PIN_SET
#endif
#ifndef BUZZER_ON_LEVEL
#define BUZZER_ON_LEVEL GPIO_PIN_SET
#endif

/* Drives the relay closed and quiets every indicator. */
void alarm_output_init(void);
/* Drives only the relay and valve indicator to the closed state, and is safe to
 * call from a fault handler. It writes ODR, so it cannot drive a pin that is
 * still configured as an input: between reset and MX_GPIO_Init the relay pin is
 * floating, and only a hardware pulldown holds it in the safe state. */
void alarm_output_force_safe(void);
/* Applies the valve decision before the indicators or any slow I/O. */
void alarm_output_apply(bool valve_open, bool green, bool yellow, bool alarm);
#endif
