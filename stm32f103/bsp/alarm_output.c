#include "alarm_output.h"

/* CubeMX carries no User Labels on these pins yet, so the mapping lives here.
 * Swap for the generated *_GPIO_Port / *_Pin macros once they are labelled. */
#define VALVE_LED_PIN  GPIO_PIN_5  /* PA5, valve command state, not feedback */
#define LED_RED_PIN    GPIO_PIN_6  /* PA6 */
#define BUZZER_PIN     GPIO_PIN_7  /* PA7 */
#define RELAY_PIN      GPIO_PIN_8  /* PA8 */
#define LED_GREEN_PIN  GPIO_PIN_8  /* PB8 */
#define LED_YELLOW_PIN GPIO_PIN_9  /* PB9 */

static GPIO_PinState opposite(GPIO_PinState level)
{
    return level == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

void alarm_output_force_safe(void)
{
    HAL_GPIO_WritePin(GPIOA, RELAY_PIN, opposite(RELAY_OPEN_LEVEL));
    HAL_GPIO_WritePin(GPIOA, VALVE_LED_PIN, GPIO_PIN_RESET);
}

void alarm_output_init(void)
{
    alarm_output_force_safe();
    HAL_GPIO_WritePin(GPIOA, BUZZER_PIN, opposite(BUZZER_ON_LEVEL));
    HAL_GPIO_WritePin(GPIOA, LED_RED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, LED_GREEN_PIN | LED_YELLOW_PIN, GPIO_PIN_RESET);
}

void alarm_output_apply(bool valve_open, bool green, bool yellow, bool alarm)
{
    HAL_GPIO_WritePin(GPIOA, RELAY_PIN,
                      valve_open ? RELAY_OPEN_LEVEL : opposite(RELAY_OPEN_LEVEL));
    HAL_GPIO_WritePin(GPIOA, VALVE_LED_PIN, valve_open ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, LED_RED_PIN, alarm ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, BUZZER_PIN,
                      alarm ? BUZZER_ON_LEVEL : opposite(BUZZER_ON_LEVEL));
    HAL_GPIO_WritePin(GPIOB, LED_GREEN_PIN, green ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, LED_YELLOW_PIN, yellow ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
