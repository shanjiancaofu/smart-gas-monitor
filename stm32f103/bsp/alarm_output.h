#ifndef ALARM_OUTPUT_H
#define ALARM_OUTPUT_H
/* main.h carries the CubeMX User Label macros this module drives. */
#include "main.h"
#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* Verify against the actual relay contacts: energised means valve OPEN. */
#ifndef RELAY_OPEN_LEVEL
#define RELAY_OPEN_LEVEL GPIO_PIN_SET
#endif
#ifndef BUZZER_ON_LEVEL
#define BUZZER_ON_LEVEL GPIO_PIN_SET
#endif

/* How long the buzzer sounds from the moment an alarm starts. It is fixed
 * rather than configurable: its job is to be noticed, not to be tuned. */
#define BUZZER_ALARM_MS 5000u

/* Drives the relay closed and quiets every indicator. */
void alarm_output_init(void);
/* Drives only the relay and valve indicator to the closed state, and is safe to
 * call from a fault handler. It writes ODR, so it cannot drive a pin that is
 * still configured as an input: between reset and MX_GPIO_Init the relay pin is
 * floating, and only a hardware pulldown holds it in the safe state. */
void alarm_output_force_safe(void);
/* Applies the valve decision before the indicators or any slow I/O. The buzzer
 * is driven for BUZZER_ALARM_MS after each fresh alarm and then falls silent,
 * so a latched fault does not sound until someone acknowledges it. */
void alarm_output_apply(bool valve_open, bool green, bool yellow, bool alarm,
                        uint32_t now);
#endif
