#include "alarm/alarm.h"
#include "bsp_buzzer.h"
#include "bsp_led.h"
#include "bsp_relay.h"
#include "bsp_servo.h"

void alarm_force_safe(void)
{
    bsp_fan_set(false);
    bsp_buzzer_set(false);
    bsp_led_valve(false);
    bsp_servo_set(false);
}

void alarm_init(alarm_t *alarm)
{
    alarm->active = false;
    alarm->started_tick = 0;
    alarm->alarm_mask = 0;
    alarm_force_safe();
    bsp_led_set(false, false, false, false);
}

void alarm_update(alarm_t *alarm, const gas_t *gas, uint32_t tick)
{
    bool active = gas->state == GAS_ALARM || gas->state == GAS_FAULT;
    bool open = gas_valve_open(gas);
    uint16_t duration = config_buzzer_duration_ms(&gas->config);

    if (active && (!alarm->active || (gas->alarm_mask & (uint8_t)~alarm->alarm_mask) != 0u)) {
        alarm->started_tick = tick;
    }
    alarm->active = active;
    alarm->alarm_mask = active ? gas->alarm_mask : 0u;

    /* 先落实关阀，再更新提示输出。 */
    /* A closed valve during normal warmup does not require exhaust.
       A persisted lockout keeps exhaust running even during warmup. */
    bsp_fan_set(active || gas->state == GAS_SAFE_WAIT || gas->latched ||
                gas->config.lockout);
    bsp_servo_set(open);
    bool buzzer = false;
    if (active && gas->state == GAS_FAULT) {
        bsp_buzzer_set(duration != GAS_BUZZER_OFF);
        bsp_led_set(false, false, true, open);
        return;
    }
    if (active && (duration == GAS_BUZZER_FOREVER_MS ||
                   (uint32_t)(tick - alarm->started_tick) < duration / GAS_TICK_MS)) {
        unsigned count = 0;
        uint32_t elapsed = tick - alarm->started_tick;
        uint32_t phase;
        for (unsigned i = 0; i < GAS_COUNT; ++i) {
            if (gas->alarm_mask & (1u << i)) {
                ++count;
            }
        }
        if (count == 0) {
            count = 1;
        }
        /* 一组里有 count 滴，每滴的起点往后挪一个 BEEP_STEP_TICKS。 */
        phase = elapsed % (count * BEEP_STEP_TICKS + BEEP_GROUP_GAP_TICKS);
        for (unsigned i = 0; i < count; ++i) {
            uint32_t start = i * BEEP_STEP_TICKS;
            if (phase >= start && phase < start + BEEP_ON_TICKS) {
                buzzer = true;
            }
        }
    }
    bsp_buzzer_set(buzzer);
    bsp_led_set(gas->state == GAS_NORMAL, !active && gas->state != GAS_NORMAL, active, open);
}
