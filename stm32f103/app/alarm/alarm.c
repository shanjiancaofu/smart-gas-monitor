#include "alarm/alarm.h"
#include "bsp_buzzer.h"
#include "bsp_led.h"
#include "bsp_relay.h"

void alarm_force_safe(void)
{
    bsp_relay_close();
    bsp_buzzer_set(false);
    bsp_led_valve(false);
}

void alarm_init(alarm_t *alarm)
{
    alarm->active = false;
    alarm->started_tick = 0;
    alarm_force_safe();
    bsp_led_set(false, false, false, false);
}

void alarm_update(alarm_t *alarm, const gas_t *gas, uint32_t tick)
{
    bool active = gas->state == GAS_ALARM || gas->state == GAS_FAULT;
    bool open = gas_valve_open(gas);
    uint16_t duration = config_buzzer_duration_ms(&gas->config);

    if (active && !alarm->active) {
        alarm->started_tick = tick;
    }
    alarm->active = active;

    /* 先落实关阀，再更新提示输出。 */
    if (open) {
        bsp_relay_open();
    } else {
        bsp_relay_close();
    }
    bsp_buzzer_set(active && (duration == GAS_BUZZER_FOREVER_MS ||
                              (uint32_t)(tick - alarm->started_tick) < duration / GAS_TICK_MS));
    bsp_led_set(gas->state == GAS_NORMAL, !active && gas->state != GAS_NORMAL, active, open);
}
