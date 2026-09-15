#include "bsp_buzzer.h"
#include "main.h"

void bsp_buzzer_set(bool on)
{
    GPIO_PinState off = BUZZER_ON_LEVEL == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, on ? BUZZER_ON_LEVEL : off);
}
