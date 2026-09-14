#include "alarm_output.h"

/* 引脚名取自 CubeMX 的 User Labels，在 .ioc 里挪动引脚会在这里变成编译错误，
 * 而不是一个悄悄失效的输出。 */
static bool buzzer_alarm;
static uint32_t buzzer_start_tick;

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
    buzzer_start_tick = 0;
    alarm_output_force_safe();
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, opposite(BUZZER_ON_LEVEL));
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin, GPIO_PIN_RESET);
}

void alarm_output_apply(bool valve_open, bool green, bool yellow, bool alarm,
                        uint32_t tick)
{
    bool buzzer_on;
    /* 新的一次报警会重新开始计时窗口；报警持续并不会。 */
    if (alarm && !buzzer_alarm) buzzer_start_tick = tick;
    buzzer_alarm = alarm;
    buzzer_on = alarm && (uint32_t)(tick - buzzer_start_tick) < BUZZER_ALARM_TICKS;
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
