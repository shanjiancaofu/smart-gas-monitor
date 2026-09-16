#include "bsp_relay.h"
#include "main.h"

void bsp_fan_set(bool on)
{
    GPIO_PinState level = on ? RELAY_OPEN_LEVEL : (RELAY_OPEN_LEVEL == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, level);
}
void bsp_relay_open(void) { bsp_fan_set(true); }

void bsp_relay_close(void)
{
    bsp_fan_set(false);
}
