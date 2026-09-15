#include "alarm/alarm.h"
#include "bsp_led.h"
#include "bsp_buzzer.h"
#include "bsp_relay.h"
#include <assert.h>
#include <stdio.h>

static bool relay, buzzer, green_led, yellow_led, red_led, valve_led;
void bsp_relay_open(void) { relay = true; }
void bsp_relay_close(void) { relay = false; }
void bsp_buzzer_set(bool on) { buzzer = on; }
void bsp_led_valve(bool open) { valve_led = open; }
void bsp_led_set(bool green, bool yellow, bool red, bool valve)
{
    green_led = green;
    yellow_led = yellow;
    red_led = red;
    valve_led = valve;
}

int main(void)
{
    alarm_t alarm;
    gas_t gas;
    uint32_t start = UINT32_MAX - 250u;

    gas_init(&gas, NULL, 0);
    alarm_init(&alarm);
    assert(!relay && !buzzer && !valve_led);
    gas.state = GAS_NORMAL;
    alarm_update(&alarm, &gas, 0);
    assert(relay && green_led && valve_led && !buzzer);
    gas.state = GAS_WARNING;
    alarm_update(&alarm, &gas, 1);
    assert(relay && yellow_led && !red_led);

    gas.state = GAS_ALARM;
    gas.latched = true;
    gas.alarm_mask = 1u;
    alarm_update(&alarm, &gas, start);
    assert(!relay && !valve_led && red_led && buzzer);
    alarm_update(&alarm, &gas, start + 20u);
    assert(!buzzer);
    gas.alarm_mask = 3u;
    alarm_update(&alarm, &gas, start + 40u);
    assert(buzzer);
    alarm_update(&alarm, &gas, start + 500u);
    assert(!buzzer && red_led && !relay);
    alarm_update(&alarm, &gas, start + 1000u);
    assert(!buzzer);

    gas.state = GAS_SAFE_WAIT;
    alarm_update(&alarm, &gas, 1000);
    assert(!relay && !buzzer && yellow_led);
    gas.state = GAS_FAULT;
    alarm_update(&alarm, &gas, 1001);
    assert(buzzer && !relay && red_led);
    gas.config.buzzer = GAS_BUZZER_OFF;
    alarm_update(&alarm, &gas, 1002);
    assert(!buzzer);
    gas.config.buzzer = GAS_BUZZER_ALWAYS;
    alarm_update(&alarm, &gas, 1001u + 128u * 1000u);
    assert(buzzer);
    alarm_force_safe();
    assert(!relay && !buzzer && !valve_led);
    puts("PASS: alarm output mapping/timer/wraparound/off/always/safe");
    return 0;
}
