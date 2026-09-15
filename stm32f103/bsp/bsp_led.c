#include "bsp_led.h"
#include "main.h"

void bsp_led_set(bool green, bool yellow, bool red, bool valve_open)
{
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, green ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin, yellow ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, red ? GPIO_PIN_SET : GPIO_PIN_RESET);
    bsp_led_valve(valve_open);
}

void bsp_led_valve(bool open)
{
    HAL_GPIO_WritePin(VALVE_LED_GPIO_Port, VALVE_LED_Pin, open ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
