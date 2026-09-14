#include "alarm_output.h"

/* Pin names come from the CubeMX User Labels, so a pin move in the .ioc shows
 * up here as a compile error instead of a silently dead output. */
static bool buzzer_alarm;
static uint32_t buzzer_start_ms;

static GPIO_PinState opposite(GPIO_PinState level)
{
    return level == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

void alarm_output_force_safe(void)
{
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, opposite(RELAY_OPEN_LEVEL));
    HAL_GPIO_WritePin(VALVE_LED_GPIO_Port, VALVE_LED_Pin, GPIO_PIN_RESET);
}

void alarm_output_init(void)
{
    buzzer_alarm = false;
    buzzer_start_ms = 0;
    alarm_output_force_safe();
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, opposite(BUZZER_ON_LEVEL));
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin, GPIO_PIN_RESET);
}

void alarm_output_apply(bool valve_open, bool green, bool yellow, bool alarm,
                        uint32_t now)
{
    bool buzzer_on;
    /* A fresh alarm restarts the window; the alarm staying on does not. */
    if (alarm && !buzzer_alarm) buzzer_start_ms = now;
    buzzer_alarm = alarm;
    buzzer_on = alarm && (uint32_t)(now - buzzer_start_ms) < BUZZER_ALARM_MS;
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin,
                      valve_open ? RELAY_OPEN_LEVEL : opposite(RELAY_OPEN_LEVEL));
    HAL_GPIO_WritePin(VALVE_LED_GPIO_Port, VALVE_LED_Pin,
                      valve_open ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin,
                      alarm ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin,
                      buzzer_on ? BUZZER_ON_LEVEL : opposite(BUZZER_ON_LEVEL));
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin,
                      green ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin,
                      yellow ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
