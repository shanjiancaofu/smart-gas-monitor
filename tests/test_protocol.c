#include "protocol/protocol.h"
#include "gas/gas.h"
#include "history/history.h"
#include <assert.h>
#if EEPROM_MODEL == 64
#define TEST_HISTORY_CAPACITY "510"
#else
#define TEST_HISTORY_CAPACITY "14"
#endif
#include <stdio.h>
#include <string.h>

/* 同 test_gas.c：用例里的绝对时刻从 run_to_normal() 最后一次采样算起，
 * 必须落在采样超时窗口内，否则会被读成采样中断而不是状态变化。 */
#define T0 (GAS_WARMUP_MS + GAS_SAFE_HOLD_MS)

typedef struct { uint8_t data[8192]; } fake_t;

static bool read_mem(void *ctx, uint16_t addr, uint8_t *data, size_t size)
{
    fake_t *f = ctx;
    assert((size_t)addr + size <= 8192);
    memcpy(data, f->data + addr, size);
    return true;
}
static bool write_mem(void *ctx, uint16_t addr, const uint8_t *data, size_t size)
{
    fake_t *f = ctx;
    assert((size_t)addr + size <= 8192);
    memcpy(f->data + addr, data, size);
    return true;
}


static void reply(protocol_t *p, char *out, size_t size)
{
    const char *line;
    size_t used = 0u;
    out[0] = '\0';
    while ((line = protocol_next(p)) != NULL) {
        (void)snprintf(out + used, size - used, "%s%s", used != 0u ? "|" : "", line);
        used = strlen(out);
    }
}

static void command(protocol_t *p, const char *text, uint32_t now, char *out,
                    size_t size)
{
    protocol_command(p, text, now);
    reply(p, out, size);
}

static void sample(gas_t *m, uint32_t now, uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t values[GAS_COUNT] = {a, b, c};
    gas_sample(m, values, true, now);
}
static void run_to_normal(gas_t *m, uint32_t start)
{
    uint32_t t;
    gas_init(m, NULL, start);
    for (t = 0; t <= GAS_WARMUP_MS + GAS_SAFE_HOLD_MS; t += 100)
        sample(m, start + t, 500, 500, 500);
    assert(m->state == GAS_NORMAL && gas_valve_open(m));
}

static void test_queries(void)
{
    gas_t m;
    protocol_t p;
    fake_t f;
    config_io_t io = {&f, read_mem, write_mem};
    history_t h;
    char out[512];
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    protocol_init(&p, &m, &h);

    command(&p, "STATUS?", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "STATE=NORMAL VALVE=OPEN ALARMS=0|"
                       "MQ4=500/4000 MQ6=500/2000 MQ7=500/3000") == 0);

    command(&p, "CONFIG?", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "TH MQ4=4000 MQ6=2000 MQ7=3000 PERIOD=100 BUZZ=ALWAYS") == 0);


    command(&p, "HISTORY?", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "HISTORY 0/" TEST_HISTORY_CAPACITY) == 0);


    command(&p, "status?", (T0 + 100u), out, sizeof(out));
    assert(strncmp(out, "STATE=NORMAL", 12) == 0);


    command(&p, "   ", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "") == 0);
    command(&p, "", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "") == 0);
}

static void test_set(void)
{
    gas_t m;
    protocol_t p;
    fake_t f;
    config_io_t io = {&f, read_mem, write_mem};
    history_t h;
    char out[512];
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    protocol_init(&p, &m, &h);
    assert(!m.dirty);

    command(&p, "SET MQ4 2600", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK MQ4=2600") == 0);
    assert(m.config.alarm[GAS_MQ4] == 2600u);

    assert(m.dirty);

    command(&p, "set mq6 900", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK MQ6=900") == 0 && m.config.alarm[GAS_MQ6] == 900u);

    command(&p, "SET PERIOD 1000", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK PERIOD=1000") == 0 && m.config.sample_period_ms == 1000u);


    command(&p, "SET MQ4 100", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0 && m.config.alarm[GAS_MQ4] == 2600u);
    command(&p, "SET MQ4 9999", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0);

    command(&p, "SET PERIOD 255", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0 && m.config.sample_period_ms == 1000u);
    command(&p, "SET PERIOD 50", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0 && m.config.sample_period_ms == 1000u);

    command(&p, "SET PERIOD 250", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK PERIOD=250") == 0 && m.config.sample_period_ms == 250u);


    command(&p, "SET BUZZER 30", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK BUZZ=30S") == 0 && m.config.buzzer == 30u);
    assert(m.dirty);
    command(&p, "SET BUZZER ALWAYS", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK BUZZ=ALWAYS") == 0 && m.config.buzzer == GAS_BUZZER_ALWAYS);
    command(&p, "SET BUZZER OFF", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK BUZZ=OFF") == 0 && m.config.buzzer == GAS_BUZZER_OFF);

    command(&p, "SET BUZZER always", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK BUZZ=ALWAYS") == 0);
    command(&p, "SET BUZZER 5S", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK BUZZ=5S") == 0 && m.config.buzzer == 5u);

    command(&p, "SET BUZZER 0", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK BUZZ=OFF") == 0 && m.config.buzzer == GAS_BUZZER_OFF);


    command(&p, "SET BUZZER 60", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK BUZZ=60S") == 0 && m.config.buzzer == 60);
    command(&p, "SET BUZZER 1", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK BUZZ=1S") == 0 && m.config.buzzer == 1);
    /* 编码本身也能直接敲回来：-1 就是 ALWAYS。 */
    command(&p, "SET BUZZER -1", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK BUZZ=ALWAYS") == 0 && m.config.buzzer == GAS_BUZZER_ALWAYS);
    command(&p, "SET BUZZER 61", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0 && m.config.buzzer == GAS_BUZZER_ALWAYS);
    command(&p, "SET BUZZER -2", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0 && m.config.buzzer == GAS_BUZZER_ALWAYS);

    command(&p, "SET BUZZER 256", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0 && m.config.buzzer == GAS_BUZZER_ALWAYS);
    /* 0xffff 收窄成 int8 就是 -1，一个合法档位——所以必须在收窄前拒掉。 */
    command(&p, "SET BUZZER 65535", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0 && m.config.buzzer == GAS_BUZZER_ALWAYS);

    command(&p, "SET BUZZER S", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR VALUE") == 0 && m.config.buzzer == GAS_BUZZER_ALWAYS);
    command(&p, "SET BUZZER -", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR VALUE") == 0 && m.config.buzzer == GAS_BUZZER_ALWAYS);

    command(&p, "SET MQ9 1000", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR NAME") == 0);
    command(&p, "SET MQ4 abc", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR VALUE") == 0);
    command(&p, "SET MQ4", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR UNKNOWN") == 0);

    command(&p, "SET BUZZZER 5", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR NAME") == 0);
    command(&p, "SET BUZZER", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR UNKNOWN") == 0);
    command(&p, "SET MQ4 2500 junk", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR ARGS") == 0 && m.config.alarm[GAS_MQ4] == 2600u);
    command(&p, "NONSENSE", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR UNKNOWN") == 0);
}

static void test_valve(void)
{
    gas_t m;
    protocol_t p;
    fake_t f;
    config_io_t io = {&f, read_mem, write_mem};
    history_t h;
    uint32_t t;
    char out[512];
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    protocol_init(&p, &m, &h);

    command(&p, "VALVE CLOSE", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK VALVE=CLOSED") == 0);
    assert(!gas_valve_open(&m));
    assert(m.config.lockout && m.dirty);
    m.dirty = false;


    command(&p, "VALVE OPEN", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR ONLY CLOSE") == 0);
    assert(!gas_valve_open(&m));
    command(&p, "VALVE close", (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "OK VALVE=CLOSED") == 0);
    assert(!gas_valve_open(&m));
    assert(!m.dirty);


    for (t = (T0 + 200u); t <= (T0 + 3300u); t += 100) sample(&m, t, 500, 500, 500);
    assert(m.state == GAS_SAFE_WAIT && m.reset_ready);
    command(&p, "VALVE OPEN", (T0 + 3300u), out, sizeof(out));
    assert(strcmp(out, "ERR ONLY CLOSE") == 0 && !gas_valve_open(&m));
    gas_key(&m, GAS_KEY_CONFIRM, (T0 + 3300u));
    assert(gas_valve_open(&m));
}

static void test_history(void)
{
    gas_t m;
    protocol_t p;
    fake_t f;
    config_io_t io = {&f, read_mem, write_mem};
    history_t h;
    history_entry_t entry;
    char out[512];
    unsigned i;
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    for (i = 0; i < 3u; ++i) {
        memset(&entry, 0, sizeof(entry));
        entry.adc[GAS_MQ4] = (uint16_t)(2600u + i);
        entry.adc[GAS_MQ6] = 300u;
        entry.adc[GAS_MQ7] = 200u;
        entry.uptime_s = 60u + i;
        entry.alarm_mask = (uint8_t)(1u << i);
        assert(history_add(&h, &entry));
    }
    protocol_init(&p, &m, &h);
    command(&p, "HISTORY?", (T0 + 100u), out, sizeof(out));

    assert(strcmp(out,
        "HISTORY 3/" TEST_HISTORY_CAPACITY "|"
        "1 SEQ=3 MQ4=2602 MQ6=300 MQ7=200 ALARM=MQ7 UP=62|"
        "2 SEQ=2 MQ4=2601 MQ6=300 MQ7=200 ALARM=MQ6 UP=61|"
        "3 SEQ=1 MQ4=2600 MQ6=300 MQ7=200 ALARM=MQ4 UP=60") == 0);
}

static void test_alarm_notice(void)
{
    gas_t m;
    protocol_t p;
    fake_t f;
    config_io_t io = {&f, read_mem, write_mem};
    history_t h;
    char out[512];
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    protocol_init(&p, &m, &h);
    assert(!protocol_busy(&p));

    sample(&m, (T0 + 100u), m.config.alarm[GAS_MQ4], 500, 500);
    sample(&m, (T0 + 200u), (uint16_t)(m.config.alarm[GAS_MQ4] + 10u), 500, 500);
    assert(m.state == GAS_ALARM);
    protocol_alarm(&p);
    assert(protocol_busy(&p));
    reply(&p, out, sizeof(out));
    assert(strcmp(out, "ALARM MQ4 MQ4=2410 MQ6=500 MQ7=500") == 0);
    assert(!protocol_busy(&p));

    assert(protocol_next(&p) == NULL);
}


static void test_long_line(void)
{
    gas_t m;
    protocol_t p;
    fake_t f;
    config_io_t io = {&f, read_mem, write_mem};
    history_t h;
    char out[512];
    char line[PROTOCOL_LINE_MAX * 2u];
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    protocol_init(&p, &m, &h);
    memset(line, 'X', sizeof(line) - 1u);
    line[sizeof(line) - 1u] = '\0';
    command(&p, line, (T0 + 100u), out, sizeof(out));
    assert(strcmp(out, "ERR LONG") == 0);
    command(&p, "STATUS?", (T0 + 100u), out, sizeof(out));
    assert(strncmp(out, "STATE=NORMAL", 12) == 0);
}

int main(void)
{
    test_queries();
    test_set();
    test_valve();
    test_history();
    test_alarm_notice();
    test_long_line();
    printf("PASS: protocol status/config/history/set/valve/notice/limits\n");
    return 0;
}
