#include "communication/protocol.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 够放所有命令：一个动词加至多两个参数。 */
#define TOKEN_MAX 3u
#define TOKEN_LEN 12u

static char upper(char c)
{
    /* 刻意不用 toupper()：这里不能依赖 newlib 恰好是用哪个 C locale
     * 编译出来的。 */
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

/* 按连续空格切分。返回 TOKEN_MAX + 1 表示「参数比任何命令能接收的还
 * 多」，这是语法错误，而不是对「SET MQ4 2500 junk」这类输入悄悄截断。 */
static unsigned split(char *text, char tokens[TOKEN_MAX][TOKEN_LEN])
{
    unsigned count = 0u;
    char *p = text;
    for (;;) {
        char *start;
        unsigned n = 0u;
        while (*p == ' ') ++p;
        if (*p == '\0' || count >= TOKEN_MAX) break;
        start = p;
        while (*p != '\0' && *p != ' ') ++p;
        while (start < p && n + 1u < TOKEN_LEN) tokens[count][n++] = *start++;
        tokens[count][n] = '\0';
        ++count;
    }
    return *p != '\0' ? TOKEN_MAX + 1u : count;
}

static bool parse_u16(const char *text, uint16_t *out)
{
    uint32_t value = 0u;
    if (*text == '\0') return false;
    while (*text != '\0') {
        if (*text < '0' || *text > '9') return false;
        value = value * 10u + (uint32_t)(*text - '0');
        if (value > 0xffffu) return false;
        ++text;
    }
    *out = (uint16_t)value;
    return true;
}

static bool channel_of(const char *token, gas_channel_t *out)
{
    unsigned i;
    for (i = 0; i < GAS_COUNT; ++i) {
        if (strcmp(token, gas_channel_name((gas_channel_t)i)) != 0) continue;
        *out = (gas_channel_t)i;
        return true;
    }
    return false;
}

/* 排入一条自身即完整的应答。 */
static void one(protocol_t *p, const char *text)
{
    (void)snprintf(p->text, sizeof(p->text), "%s", text);
    p->job = PROTOCOL_ONE;
}

static void onef(protocol_t *p, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    (void)vsnprintf(p->text, sizeof(p->text), format, args);
    va_end(args);
    p->job = PROTOCOL_ONE;
}

void protocol_init(protocol_t *p, gas_monitor_t *monitor, const history_t *history)
{
    memset(p, 0, sizeof(*p));
    p->monitor = monitor;
    p->history = history;
    p->job = PROTOCOL_IDLE;
}

bool protocol_busy(const protocol_t *p) { return p->job != PROTOCOL_IDLE; }

void protocol_alarm(protocol_t *p)
{
    p->job = PROTOCOL_NOTICE;
    p->line = 0u;
}

void protocol_command(protocol_t *p, const char *line, uint32_t now)
{
    char work[PROTOCOL_LINE_MAX];
    char tokens[TOKEN_MAX][TOKEN_LEN];
    unsigned count;
    size_t i;

    p->job = PROTOCOL_IDLE;
    p->line = 0u;
    p->count = 0u;
    if (strlen(line) >= sizeof(work)) {
        one(p, "ERR LONG");
        return;
    }
    for (i = 0u; line[i] != '\0'; ++i) work[i] = upper(line[i]);
    work[i] = '\0';

    count = split(work, tokens);
    /* 空行是行结束，不是命令；对它回话只会让终端的回显看起来像一场
     * 对话。 */
    if (count == 0u) return;
    if (count > TOKEN_MAX) {
        one(p, "ERR ARGS");
        return;
    }

    if (count == 1u && strcmp(tokens[0], "STATUS?") == 0) {
        p->job = PROTOCOL_STATUS;
    } else if (count == 1u && strcmp(tokens[0], "CONFIG?") == 0) {
        p->job = PROTOCOL_CONFIG;
    } else if (count == 1u && strcmp(tokens[0], "HISTORY?") == 0) {
        p->job = PROTOCOL_HISTORY;
        p->count = history_count(p->history);
    } else if (count == 3u && strcmp(tokens[0], "SET") == 0) {
        gas_channel_t channel;
        uint16_t value;
        if (!parse_u16(tokens[2], &value)) {
            one(p, "ERR VALUE");
        } else if (channel_of(tokens[1], &channel)) {
            if (gas_monitor_set_threshold(p->monitor, channel, value, now))
                onef(p, "OK %s=%u", gas_channel_name(channel), value);
            else one(p, "ERR RANGE");
        } else if (strcmp(tokens[1], "PERIOD") == 0) {
            if (gas_monitor_set_period(p->monitor, value, now))
                onef(p, "OK PERIOD=%u", value);
            else one(p, "ERR RANGE");
        } else {
            one(p, "ERR NAME");
        }
    } else if (count == 2u && strcmp(tokens[0], "VALVE") == 0) {
        if (strcmp(tokens[1], "CLOSE") == 0) {
            gas_monitor_close_valve(p->monitor, now);
            one(p, "OK VALVE=CLOSED");
        } else {
            /* 刻意没有远程开阀。解除锁存得靠站在面板前的人。 */
            one(p, "ERR ONLY CLOSE");
        }
    } else {
        one(p, "ERR UNKNOWN");
    }
}

const char *protocol_next(protocol_t *p)
{
    const gas_monitor_t *m = p->monitor;
    switch (p->job) {
    case PROTOCOL_IDLE:
        return NULL;
    case PROTOCOL_ONE:
        p->job = PROTOCOL_IDLE;
        return p->text;
    case PROTOCOL_NOTICE: {
        char mask[16];
        gas_alarm_mask_name(m->alarm_mask, mask, sizeof(mask));
        /* 主动推送，这样盯着这条链路的手机不必轮询就能知道发生了报警。 */
        (void)snprintf(p->text, sizeof(p->text), "ALARM %s %s=%u %s=%u %s=%u", mask,
                       gas_channel_name(GAS_MQ4), m->adc[GAS_MQ4],
                       gas_channel_name(GAS_MQ7), m->adc[GAS_MQ7],
                       gas_channel_name(GAS_MQ8), m->adc[GAS_MQ8]);
        p->job = PROTOCOL_IDLE;
        return p->text;
    }
    case PROTOCOL_STATUS:
        if (p->line == 0u) {
            (void)snprintf(p->text, sizeof(p->text), "STATE=%s VALVE=%s ALARMS=%lu",
                           gas_state_name(m->state),
                           gas_monitor_valve_open(m) ? "OPEN" : "CLOSED",
                           (unsigned long)m->alarm_count);
        } else if (p->line == 1u) {
            (void)snprintf(p->text, sizeof(p->text),
                           "%s=%u/%u %s=%u/%u %s=%u/%u",
                           gas_channel_name(GAS_MQ4), m->adc[GAS_MQ4], m->config.alarm[GAS_MQ4],
                           gas_channel_name(GAS_MQ7), m->adc[GAS_MQ7], m->config.alarm[GAS_MQ7],
                           gas_channel_name(GAS_MQ8), m->adc[GAS_MQ8], m->config.alarm[GAS_MQ8]);
        } else {
            p->job = PROTOCOL_IDLE;
            p->line = 0u;
            return NULL;
        }
        ++p->line;
        return p->text;
    case PROTOCOL_CONFIG:
        if (p->line == 0u) {
            (void)snprintf(p->text, sizeof(p->text),
                           "TH %s=%u %s=%u %s=%u PERIOD=%u",
                           gas_channel_name(GAS_MQ4), m->config.alarm[GAS_MQ4],
                           gas_channel_name(GAS_MQ7), m->config.alarm[GAS_MQ7],
                           gas_channel_name(GAS_MQ8), m->config.alarm[GAS_MQ8],
                           m->config.sample_period_ms);
        } else {
            p->job = PROTOCOL_IDLE;
            p->line = 0u;
            return NULL;
        }
        ++p->line;
        return p->text;
    case PROTOCOL_HISTORY:
        if (p->line == 0u) {
            (void)snprintf(p->text, sizeof(p->text), "HISTORY %u/%u", p->count,
                           (unsigned)HISTORY_SLOTS);
        } else {
            history_entry_t entry;
            uint8_t index = (uint8_t)(p->line - 1u);
            char mask[16];
            if (index >= p->count) {
                p->job = PROTOCOL_IDLE;
                p->line = 0u;
                return NULL;
            }
            if (!history_get(p->history, index, &entry)) {
                (void)snprintf(p->text, sizeof(p->text), "%u UNREADABLE", index + 1u);
            } else {
                gas_alarm_mask_name(entry.alarm_mask, mask, sizeof(mask));
                /* 索引放在最前面：面板上翻阅键移动的就是它，这样两处
                 * 视图能对得上。 */
                (void)snprintf(p->text, sizeof(p->text),
                               "%u SEQ=%u %s=%u %s=%u %s=%u ALARM=%s UP=%lu",
                               index + 1u, entry.seq,
                               gas_channel_name(GAS_MQ4), entry.adc[GAS_MQ4],
                               gas_channel_name(GAS_MQ7), entry.adc[GAS_MQ7],
                               gas_channel_name(GAS_MQ8), entry.adc[GAS_MQ8],
                               mask, (unsigned long)entry.uptime_s);
            }
        }
        ++p->line;
        return p->text;
    default:
        p->job = PROTOCOL_IDLE;
        return NULL;
    }
}
