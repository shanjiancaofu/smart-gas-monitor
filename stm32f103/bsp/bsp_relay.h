#ifndef BSP_RELAY_H
#define BSP_RELAY_H
#include <stdbool.h>

/* Active-high exhaust fan relay. */
#ifndef RELAY_OPEN_LEVEL
#define RELAY_OPEN_LEVEL GPIO_PIN_SET
#endif
void bsp_fan_set(bool on);
/* Active-high exhaust fan relay. */
void bsp_relay_open(void);
void bsp_relay_close(void);
#endif

