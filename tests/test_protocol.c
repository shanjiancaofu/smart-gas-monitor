#include "communication/protocol.h"
#include "gas/gas_monitor.h"
#include "history/history.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint8_t data[256]; } fake_t;

static bool read_mem(void *ctx, uint16_t addr, uint8_t *data, size_t size)
{
    fake_t *f = ctx;
    assert((size_t)addr + size <= 256);
    memcpy(data, f->data + addr, size);
    return true;
}
static bool write_mem(void *ctx, uint16_t addr, const uint8_t *data, size_t size)
{
    fake_t *f = ctx;
    assert((size_t)addr + size <= 256);
    memcpy(f->data + addr, data, size);
    return true;
}

/* 把整段应答收进一个字符串，行间用 '|' 分隔，于是一条用例可以用一次比较
 * 说清整段回复。 */
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

static void sample(gas_monitor_t *m, uint32_t now, uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t values[GAS_COUNT] = {a, b, c};
    gas_monitor_sample(m, values, true, now);
}
static void run_to_normal(gas_monitor_t *m, uint32_t start)
{
    uint32_t t;
    gas_monitor_init(m, NULL, start);
    for (t = 0; t <= GAS_WARMUP_MS + GAS_SAFE_HOLD_MS; t += 100)
        sample(m, start + t, 500, 500, 500);
    assert(m->state == GAS_NORMAL && gas_monitor_valve_open(m));
}

static void test_queries(void)
{
    gas_monitor_t m;
    protocol_t p;
    fake_t f;
    settings_io_t io = {&f, read_mem, write_mem};
    history_t h;
    char out[512];
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    protocol_init(&p, &m, &h);

    command(&p, "STATUS?", 63100, out, sizeof(out));
    assert(strcmp(out, "STATE=NORMAL VALVE=OPEN ALARMS=0|"
                       "MQ4=500/2400 MQ7=500/2000 MQ8=500/2400") == 0);

    command(&p, "CONFIG?", 63100, out, sizeof(out));
    assert(strcmp(out, "TH MQ4=2400 MQ7=2000 MQ8=2400 PERIOD=100") == 0);

    /* 空记录也要回一行表头而不是什么都不回，否则空应答会被当成链路断了。 */
    command(&p, "HISTORY?", 63100, out, sizeof(out));
    assert(strcmp(out, "HISTORY 0/14") == 0);

    /* 全部大小写不敏感：在终端上敲命令的人不该需要知道手册里哪个字母是大写。 */
    command(&p, "status?", 63100, out, sizeof(out));
    assert(strncmp(out, "STATE=NORMAL", 12) == 0);

    /* 空行是线路噪声，不是命令。 */
    command(&p, "   ", 63100, out, sizeof(out));
    assert(strcmp(out, "") == 0);
    command(&p, "", 63100, out, sizeof(out));
    assert(strcmp(out, "") == 0);
}

static void test_set(void)
{
    gas_monitor_t m;
    protocol_t p;
    fake_t f;
    settings_io_t io = {&f, read_mem, write_mem};
    history_t h;
    char out[512];
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    protocol_init(&p, &m, &h);
    assert(!m.dirty);

    command(&p, "SET MQ4 2600", 63100, out, sizeof(out));
    assert(strcmp(out, "OK MQ4=2600") == 0);
    assert(m.config.alarm[GAS_MQ4] == 2600u);
    /* 远程改动必须和按键一样持久：走同一条记账路径置位保存标志，否则这个值
     * 下次掉电就没了。 */
    assert(m.dirty);

    command(&p, "set mq7 900", 63100, out, sizeof(out));
    assert(strcmp(out, "OK MQ7=900") == 0 && m.config.alarm[GAS_MQ7] == 900u);

    command(&p, "SET PERIOD 1000", 63100, out, sizeof(out));
    assert(strcmp(out, "OK PERIOD=1000") == 0 && m.config.sample_period_ms == 1000u);

    /* 配置解码器会拒绝的值，这里也必须拒绝，否则远程写入可以存下一份开机的
     * 时候自己过不了 CRC 的配置。 */
    command(&p, "SET MQ4 100", 63100, out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0 && m.config.alarm[GAS_MQ4] == 2600u);
    command(&p, "SET MQ4 9999", 63100, out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0);
    /* 255 不是整数个定时节拍，会被凑成另一个周期，而不是它自称的那个。 */
    command(&p, "SET PERIOD 255", 63100, out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0 && m.config.sample_period_ms == 1000u);
    command(&p, "SET PERIOD 50", 63100, out, sizeof(out));
    assert(strcmp(out, "ERR RANGE") == 0 && m.config.sample_period_ms == 1000u);
    /* 范围内的任意整数个节拍都接受，包括按键步进选不到的值：按键是快捷方式，
     * 不是全部合法取值。 */
    command(&p, "SET PERIOD 250", 63100, out, sizeof(out));
    assert(strcmp(out, "OK PERIOD=250") == 0 && m.config.sample_period_ms == 250u);

    command(&p, "SET MQ9 1000", 63100, out, sizeof(out));
    assert(strcmp(out, "ERR NAME") == 0);
    command(&p, "SET MQ4 abc", 63100, out, sizeof(out));
    assert(strcmp(out, "ERR VALUE") == 0);
    command(&p, "SET MQ4", 63100, out, sizeof(out));
    assert(strcmp(out, "ERR UNKNOWN") == 0);
    command(&p, "SET MQ4 2500 junk", 63100, out, sizeof(out));
    assert(strcmp(out, "ERR ARGS") == 0 && m.config.alarm[GAS_MQ4] == 2600u);
    command(&p, "NONSENSE", 63100, out, sizeof(out));
    assert(strcmp(out, "ERR UNKNOWN") == 0);
}

static void test_valve(void)
{
    gas_monitor_t m;
    protocol_t p;
    fake_t f;
    settings_io_t io = {&f, read_mem, write_mem};
    history_t h;
    uint32_t t;
    char out[512];
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    protocol_init(&p, &m, &h);

    command(&p, "VALVE CLOSE", 63100, out, sizeof(out));
    assert(strcmp(out, "OK VALVE=CLOSED") == 0);
    assert(!gas_monitor_valve_open(&m));
    assert(m.config.lockout && m.dirty);
    m.dirty = false;

    /* 这里故意不做远程开阀。锁存阀门的意义就在于解除它必须有人在面板前，所以
     * 链路不能有能力撤销一次关阀。 */
    command(&p, "VALVE OPEN", 63100, out, sizeof(out));
    assert(strcmp(out, "ERR ONLY CLOSE") == 0);
    assert(!gas_monitor_valve_open(&m));
    command(&p, "VALVE close", 63100, out, sizeof(out));
    assert(strcmp(out, "OK VALVE=CLOSED") == 0);
    assert(!gas_monitor_valve_open(&m));
    assert(!m.dirty);

    /* 远程关阀之后重新开阀，和报警之后是同一条规则：浓度要够低，且必须按下
     * KEY4。 */
    for (t = 63200; t <= 66300; t += 100) sample(&m, t, 500, 500, 500);
    assert(m.state == GAS_SAFE_WAIT && m.reset_ready);
    command(&p, "VALVE OPEN", 66300, out, sizeof(out));
    assert(strcmp(out, "ERR ONLY CLOSE") == 0 && !gas_monitor_valve_open(&m));
    gas_monitor_key(&m, 4, 66300);
    assert(gas_monitor_valve_open(&m));
}

static void test_history(void)
{
    gas_monitor_t m;
    protocol_t p;
    fake_t f;
    settings_io_t io = {&f, read_mem, write_mem};
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
        entry.adc[GAS_MQ7] = 300u;
        entry.adc[GAS_MQ8] = 200u;
        entry.uptime_s = 60u + i;
        entry.alarm_mask = (uint8_t)(1u << i);
        assert(history_append(&h, &entry));
    }
    protocol_init(&p, &m, &h);
    command(&p, "HISTORY?", 63100, out, sizeof(out));
    /* 最新的在前，并且带上序号，这样跨掉电读记录时不会丢掉报警总数。 */
    assert(strcmp(out,
        "HISTORY 3/14|"
        "1 SEQ=3 MQ4=2602 MQ7=300 MQ8=200 ALARM=MQ8 UP=62|"
        "2 SEQ=2 MQ4=2601 MQ7=300 MQ8=200 ALARM=MQ7 UP=61|"
        "3 SEQ=1 MQ4=2600 MQ7=300 MQ8=200 ALARM=MQ4 UP=60") == 0);
}

static void test_alarm_notice(void)
{
    gas_monitor_t m;
    protocol_t p;
    fake_t f;
    settings_io_t io = {&f, read_mem, write_mem};
    history_t h;
    char out[512];
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    protocol_init(&p, &m, &h);
    assert(!protocol_busy(&p));

    sample(&m, 63100, m.config.alarm[GAS_MQ4], 500, 500);
    sample(&m, 63200, (uint16_t)(m.config.alarm[GAS_MQ4] + 10u), 500, 500);
    assert(m.state == GAS_ALARM);
    protocol_alarm(&p);
    assert(protocol_busy(&p));
    reply(&p, out, sizeof(out));
    assert(strcmp(out, "ALARM MQ4 MQ4=2410 MQ7=500 MQ8=500") == 0);
    assert(!protocol_busy(&p));
    /* 通知只有一行且一次给完；再问不会再有。 */
    assert(protocol_next(&p) == NULL);
}

/* 超过协议行长上限的输入不能撑爆任何东西，而且之后协议仍然可用。 */
static void test_long_line(void)
{
    gas_monitor_t m;
    protocol_t p;
    fake_t f;
    settings_io_t io = {&f, read_mem, write_mem};
    history_t h;
    char out[512];
    char line[PROTOCOL_LINE_MAX * 2u];
    run_to_normal(&m, 0);
    memset(&f, 0xff, sizeof(f));
    assert(!history_init(&h, &io));
    protocol_init(&p, &m, &h);
    memset(line, 'X', sizeof(line) - 1u);
    line[sizeof(line) - 1u] = '\0';
    command(&p, line, 63100, out, sizeof(out));
    assert(strcmp(out, "ERR LONG") == 0);
    command(&p, "STATUS?", 63100, out, sizeof(out));
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
