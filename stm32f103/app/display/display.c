#include "display/display.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 两种字模都是等宽的，一行正好是整数个字符宽：128 / 8 和 126 / 6；
 * 小号行末尾的两个像素留空不用。 */
#define LARGE_COLS 16u
#define SMALL_COLS 21u
_Static_assert(SMALL_COLS >= LARGE_COLS,
               "display row buffer is too small");
/* 一行格式化文字的暂存区，随后会补齐到面板一行的宽度。 */
#define LINE_MAX 48u

#define SCREEN_REALTIME 0u
#define SCREEN_SETTINGS 1u
#define SCREEN_HISTORY  2u

/* 补齐到整行，使该行原先较长的内容被覆盖，而不是残留在它后面。 */
static void put(display_t *d, unsigned page, bool large, const char *text)
{
    char row[SMALL_COLS + 1u];
    unsigned cols = large ? LARGE_COLS : SMALL_COLS;
    size_t n = strlen(text);
    if (n > cols) n = cols;
    memcpy(row, text, n);
    memset(row + n, ' ', cols - n);
    row[cols] = '\0';
    ssd1306_text(&d->oled, 0u, page, row, large);
}

static void putf(display_t *d, unsigned page, bool large, const char *format, ...)
{
    char text[LINE_MAX];
    va_list args;
    va_start(args, format);
    (void)vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    put(d, page, large, text);
}

static const char *valve_name(bool open) { return open ? "OPEN" : "CLOSED"; }

static void draw_realtime(display_t *d, const gas_monitor_t *m)
{
    unsigned i;
    for (i = 0; i < GAS_COUNT; ++i)
        putf(d, (unsigned)(i * 2u), true, "%s %5u/%4u",
             gas_channel_name((gas_channel_t)i), m->adc[i], m->config.alarm[i]);
    /* 阀门字样与状态并排显示，因为两者不一致是刻意的：锁存时显示 ALARM
     * 而阀门保持关闭，已经恢复时显示 SAFE 而阀门仍然关闭。 */
    putf(d, 6u, true, "%-7s %s", gas_state_name(m->state),
         valve_name(gas_monitor_valve_open(m)));
}

static void draw_settings(display_t *d, const gas_monitor_t *m, bool storage_ok)
{
    char buzz[8];
    unsigned i;
    putf(d, 0u, false, "SETTINGS");
    for (i = 0; i < GAS_COUNT; ++i)
        putf(d, 1u + i, false, "%c %s  TH %5u",
             m->selected == (uint8_t)(GAS_SEL_MQ4 + i) ? '>' : ' ',
             gas_channel_name((gas_channel_t)i), m->config.alarm[i]);
    putf(d, 4u, false, "%c PERIOD %5u ms",
         m->selected == GAS_SEL_PERIOD ? '>' : ' ', m->config.sample_period_ms);
    gas_buzzer_name(m->config.buzzer, buzz, sizeof(buzz));
    putf(d, 5u, false, "%c BUZZ %s",
         m->selected == GAS_SEL_BUZZER ? '>' : ' ', buzz);
    /* STORE 与 ALARMS 并成一行，把腾出来的那一行给蜂鸣器——八行是面板的物理
     * 上限，没有页内翻页可用。掉电后能留下的是存储的那一份，所以即使运行中
     * 的配置不受影响，写入失败也值得显示出来。 */
    putf(d, 6u, false, "STORE %s  ALARMS %lu", storage_ok ? "OK" : "FAIL",
         (unsigned long)m->alarm_count);
    putf(d, 7u, false, "KEY1 NEXT KEY2/3 ADJ");
}

static void draw_history(display_t *d, const history_t *h)
{
    history_entry_t entry;
    char mask[16];
    uint8_t count = history_count(h);
    putf(d, 0u, false, "HISTORY %u/%u", count, HISTORY_SLOTS);
    if (count == 0u) {
        putf(d, 1u, false, "NO RECORDS");
        putf(d, 2u, false, "KEY1 NEXT");
        putf(d, 3u, false, "");
        putf(d, 4u, false, "");
        putf(d, 5u, false, "");
        putf(d, 6u, false, "");
        putf(d, 7u, false, "");
        return;
    }
    if (!history_get(h, d->history_index, &entry)) {
        putf(d, 1u, false, "RECORD %u UNREADABLE", d->history_index);
        putf(d, 2u, false, "");
        putf(d, 3u, false, "");
        putf(d, 4u, false, "");
        putf(d, 5u, false, "");
        putf(d, 6u, false, "");
        putf(d, 7u, false, "");
        return;
    }
    gas_alarm_mask_name(entry.alarm_mask, mask, sizeof(mask));
    /* 索引与序号都显示：索引对应翻阅键的位置，序号说明总共发生过多少次
     * 报警。 */
    putf(d, 0u, false, "HISTORY %u/%u", d->history_index + 1u, count);
    putf(d, 1u, false, "REC %-4u SEQ %u", d->history_index + 1u, entry.seq);
    putf(d, 2u, false, "UP %lu s", (unsigned long)entry.uptime_s);
    putf(d, 3u, false, "%s %5u", gas_channel_name(GAS_MQ4), entry.adc[GAS_MQ4]);
    putf(d, 4u, false, "%s %5u", gas_channel_name(GAS_MQ7), entry.adc[GAS_MQ7]);
    putf(d, 5u, false, "%s %5u", gas_channel_name(GAS_MQ8), entry.adc[GAS_MQ8]);
    putf(d, 6u, false, "ALARM %s", mask);
    putf(d, 7u, false, "KEY2 NEWER KEY3 OLD");
}

void display_init(display_t *d, I2C_HandleTypeDef *i2c)
{
    memset(d, 0, sizeof(*d));
    d->ready = ssd1306_init(&d->oled, i2c);
    d->screen = (uint8_t)-1;
}

void display_update(display_t *d, const gas_monitor_t *m, const history_t *h,
                    bool storage_ok, uint32_t now)
{
    uint8_t screen;
    if (!d->ready) return;
    screen = m->selected == GAS_SEL_HISTORY ? SCREEN_HISTORY
           : m->selected == GAS_SEL_MAIN ? SCREEN_REALTIME
           : SCREEN_SETTINGS;
    if (screen == d->screen && (uint32_t)(now - d->last_draw_ms) < DISPLAY_REFRESH_MS)
        return;
    /* 切换页面会留下新页面不绘制的行，所以缓冲区先清空，而不是依赖每个
     * 页面都覆盖全部八行。 */
    if (screen != d->screen) {
        ssd1306_clear(&d->oled);
        /* 浏览位置按每次进入页面重置：再次进入该页应显示最新一条报警，
         * 而不是上次离开时的位置。 */
        if (screen == SCREEN_HISTORY) d->history_index = 0u;
    }
    switch (screen) {
    case SCREEN_SETTINGS: draw_settings(d, m, storage_ok); break;
    case SCREEN_HISTORY:  draw_history(d, h); break;
    default:              draw_realtime(d, m); break;
    }
    ssd1306_flush(&d->oled);
    d->screen = screen;
    d->last_draw_ms = now;
}

bool display_history_key(display_t *d, const history_t *h, unsigned key)
{
    uint8_t count = history_count(h);
    if (key == 2u) {
        if (d->history_index > 0u) --d->history_index;
        return true;
    }
    if (key == 3u) {
        if ((unsigned)d->history_index + 1u < (unsigned)count) ++d->history_index;
        return true;
    }
    return false;
}
