#include "bsp_relay.h"
#include "main.h"

void bsp_relay_open(void)
{
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, RELAY_OPEN_LEVEL);
}

void bsp_relay_close(void)
{
    GPIO_PinState closed = RELAY_OPEN_LEVEL == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, closed);
}
