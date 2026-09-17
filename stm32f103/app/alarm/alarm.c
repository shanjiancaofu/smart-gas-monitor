#include "alarm/alarm.h"
#include "bsp_buzzer.h"
#include "bsp_led.h"
#include "bsp_relay.h"
#include "bsp_servo.h"

void alarm_force_safe(void)
{
    /* 和 alarm_update() 里的异常策略保持一致：阀门关、**风扇开**、阀门灯灭。
     *
     * 原来是风扇关——那是照「普通上电」写的，跟 FAULT 的定义冲突：状态机里
     * FAULT/ALARM/SAFE_WAIT 都是 风扇 ON + 阀门 CLOSED，而故障处理程序反而
     * 把风扇也停了。异常时排风要继续，否则故障处理程序自己违反了安全策略。
     *
     * 蜂鸣器仍然静默：故障处理程序里不再有主循环去驱动报警节奏，留着响会变成
     * 一直长鸣。 */
    bsp_fan_set(true);
    bsp_buzzer_set(false);
    bsp_led_valve(false);
    bsp_servo_set(false);
}

void alarm_init(alarm_t *alarm)
{
    alarm->active = false;
    alarm->fault = false;
    alarm->started_tick = 0;
    alarm->duration_ms = GAS_BUZZER_OFF;
    alarm->alarm_mask = 0;
    alarm->state = GAS_WARMUP;

    /* 正常上电：所有输出置为不动作。**这里刻意不调 alarm_force_safe()**——
     * 那个是给 HardFault / Error_Handler 的，它会把排风扇打开。
     *
     * 两者的区别是「真出了异常」和「还没轮到判断」：上电时状态机连第一轮都
     * 还没跑，没有理由排风；而 alarm_init() 之后才初始化 OLED、扫 EEPROM，
     * 软件 I2C 下这段有几百毫秒，让风扇先空转一阵既没道理，也和 WARMUP 的
     * 「阀门关、不排风」定义冲突。 */
    bsp_fan_set(false);
    bsp_buzzer_set(false);
    bsp_led_valve(false);
    bsp_servo_set(false);
    bsp_led_set(false, false, false, false);
}

static bool alarm_buzzer_at(const alarm_t *alarm, uint32_t tick)
{
    uint32_t elapsed;
    uint32_t phase;
    unsigned count = 0u;
    unsigned i;

    if (!alarm->active || alarm->duration_ms == GAS_BUZZER_OFF) {
        return false;
    }
    elapsed = tick - alarm->started_tick;
    if (alarm->duration_ms != GAS_BUZZER_FOREVER_MS &&
        elapsed >= alarm->duration_ms / GAS_TICK_MS) {
        return false;
    }
    if (alarm->fault) {
        return elapsed % FAULT_BEEP_PERIOD_TICKS < FAULT_BEEP_ON_TICKS;
    }
    for (i = 0; i < GAS_COUNT; ++i) {
        if (alarm->alarm_mask & (1u << i)) {
            ++count;
        }
    }
    if (count == 0u) {
        count = 1u;
    }
    phase = elapsed % (count * BEEP_STEP_TICKS + BEEP_GROUP_GAP_TICKS);
    for (i = 0; i < count; ++i) {
        uint32_t pulse = i * BEEP_STEP_TICKS;
        if (phase >= pulse && phase < pulse + BEEP_ON_TICKS) {
            return true;
        }
    }
    return false;
}

void alarm_tick_isr(const alarm_t *alarm, uint32_t tick)
{
    bsp_buzzer_set(alarm_buzzer_at(alarm, tick));
}

void alarm_update(alarm_t *alarm, const gas_t *gas, uint32_t tick)
{
    bool active = gas->state == GAS_ALARM || gas->state == GAS_FAULT;
    bool open = gas_valve_open(gas);
    uint16_t duration = config_buzzer_duration_ms(&gas->config);

    if (active && (!alarm->active || alarm->state != gas->state ||
                   (gas->alarm_mask & (uint8_t)~alarm->alarm_mask) != 0u)) {
        alarm->started_tick = tick;
    }
    /* Publish active last when enabling and first when disabling, so the
       timer ISR never consumes a partially updated pattern. */
    if (!active) {
        alarm->active = false;
    }
    alarm->fault = gas->state == GAS_FAULT;
    alarm->duration_ms = duration;
    alarm->alarm_mask = active ? gas->alarm_mask : 0u;
    alarm->state = gas->state;
    if (active) {
        alarm->active = true;
    }

    bsp_fan_set(active || gas->state == GAS_SAFE_WAIT || gas->latched ||
                gas->config.lockout);
    bsp_servo_set(open);
    alarm_tick_isr(alarm, tick);
    bsp_led_set(gas->state == GAS_NORMAL, !active && gas->state != GAS_NORMAL,
                active, open);
}
