#include "bsp_relay.h"
#include "main.h"

void bsp_fan_set(bool on)
{
    GPIO_PinState off = RELAY_OPEN_LEVEL == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, on ? RELAY_OPEN_LEVEL : off);
}
