#include "display/display.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Comment normalized for portability. */
#define LARGE_COLS 16u
#define SMALL_COLS 21u
#if !defined(__ARMCC_VERSION)
_Static_assert(SMALL_COLS >= LARGE_COLS, "display row buffer is too small");
#endif
/* Comment normalized for portability. */
#define LINE_MAX 48u

/* Comment normalized for portability. */
static void put(display_t *d, unsigned page, bool large, const char *text)
{
    char row[SMALL_COLS + 1u];
    unsigned cols = large ? LARGE_COLS : SMALL_COLS;
    size_t n = strlen(text);
    if (n > cols) {
        n = cols;
    }
    memcpy(row, text, n);
    memset(row + n, ' ', cols - n);
    row[cols] = '\0';
    bsp_oled_text(d->oled, 0u, page, row, large);
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

static const char *valve_name(bool open)
{
    return open ? "OPEN" : "CLOSED";
}

static void draw_realtime(display_t *d, const gas_t *m)
{
    unsigned i;
    for (i = 0; i < GAS_COUNT; ++i) {
        putf(d, (unsigned)(i * 2u), true, "%s %5u/%4u", gas_channel_name((gas_channel_t)i),
             m->adc[i], m->config.alarm[i]);
    }
    /* Comment normalized for portability. */
    putf(d, 6u, true, "%-7s %s", gas_state_name(m->state), valve_name(gas_valve_open(m)));
}

static void draw_settings(display_t *d, const gas_t *m, bool storage_ok)
{
    char buzz[8];
    unsigned i;
    /* Comment normalized for portability. */
    putf(d, 0u, false, "SETTINGS");
    for (i = 0; i < GAS_COUNT; ++i) {
        putf(d, 1u + i, false, "%c %s  TH %5u", m->item == (uint8_t)i ? '>' : ' ',
             gas_channel_name((gas_channel_t)i), m->config.alarm[i]);
    }
    putf(d, 4u, false, "%c PERIOD %5u ms", m->item == GAS_ITEM_PERIOD ? '>' : ' ',
         m->config.sample_period_ms);
    config_buzzer_name(m->config.buzzer, buzz, sizeof(buzz));
    putf(d, 5u, false, "%c BUZZ %s", m->item == GAS_ITEM_BUZZER ? '>' : ' ', buzz);
    /* Comment normalized for portability. */
    putf(d, 6u, false, "STORE %s  ALARMS %lu", storage_ok ? "OK" : "FAIL",
         (unsigned long)m->alarm_count);
    putf(d, 7u, false, "K5 ITEM K2+ K3-");
}

static void draw_alarm(display_t *d, const gas_t *m)
{
    char mask[16];
    unsigned i;
    gas_alarm_mask_name(m->alarm_mask, mask, sizeof(mask));
    putf(d, 0u, false, "!!! ALARM !!!");
    /* Comment normalized for portability. */
    for (i = 0; i < GAS_COUNT; ++i) {
        putf(d, 1u + i, false, "%c %s %5u", (m->alarm_mask & (1u << i)) ? '>' : ' ',
             gas_channel_name((gas_channel_t)i), m->adc[i]);
    }
    putf(d, 4u, false, "TRIGGER %s", mask);
    putf(d, 5u, false, "VALVE:CLOSE");
    /* Comment normalized for portability. */
    putf(d, 6u, false, "PRESS KEY4");
    putf(d, 7u, false, "AFTER SAFE 3S");
}

static void draw_fault(display_t *d)
{
    putf(d, 0u, false, "!!! FAULT !!!");
    putf(d, 1u, false, "ADC/COMM ERROR");
    putf(d, 3u, false, "VALVE:CLOSE");
    putf(d, 5u, false, "CHECK HARDWARE");
    putf(d, 7u, false, "PRESS KEY4 AFTER SAFE");
}

static void draw_history(display_t *d, const history_t *h)
{
    history_entry_t entry;
    char mask[16];
    uint16_t count = history_count(h);
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
    /* Comment normalized for portability. */
    putf(d, 0u, false, "HISTORY %u/%u", d->history_index + 1u, count);
    putf(d, 1u, false, "REC %-4u SEQ %u", d->history_index + 1u, entry.seq);
    putf(d, 2u, false, "UP %lu s", (unsigned long)entry.uptime_s);
    putf(d, 3u, false, "%s %5u", gas_channel_name(GAS_MQ4), entry.adc[GAS_MQ4]);
    putf(d, 4u, false, "%s %5u", gas_channel_name(GAS_MQ6), entry.adc[GAS_MQ6]);
    putf(d, 5u, false, "%s %5u", gas_channel_name(GAS_MQ7), entry.adc[GAS_MQ7]);
    putf(d, 6u, false, "ALARM %s", mask);
    putf(d, 7u, false, "K2 NEWER K3 OLDER");
}

void display_init(display_t *d, bsp_oled_t *oled)
{
    memset(d, 0, sizeof(*d));
    d->oled = oled;
    d->screen = (uint8_t)-1;
}

void display_update(display_t *d, const gas_t *m, const history_t *h, bool storage_ok, uint32_t now)
{
    uint8_t screen;
    if (!bsp_oled_is_ready(d->oled)) {
        return;
    }
    /* Comment normalized for portability. */
    screen = m->state == GAS_ALARM ? DISPLAY_ALARM :
             (m->state == GAS_FAULT ? DISPLAY_FAULT : d->page);
    if (screen == d->screen && (uint32_t)(now - d->last_draw_ms) < DISPLAY_REFRESH_MS) {
        return;
    }
    /* Comment normalized for portability. */
    if (screen != d->screen) {
        bsp_oled_clear(d->oled);
        /* Comment normalized for portability. */
        if (screen == DISPLAY_HISTORY) {
            d->history_index = 0u;
        }
    }
    switch (screen) {
    case DISPLAY_SETTINGS:
        draw_settings(d, m, storage_ok);
        break;
    case DISPLAY_HISTORY:
        draw_history(d, h);
        break;
    case DISPLAY_ALARM:
        draw_alarm(d, m);
        break;
    case DISPLAY_FAULT:
        draw_fault(d);
        break;
    default:
        draw_realtime(d, m);
        break;
    }
    bsp_oled_flush(d->oled);
    d->screen = screen;
    d->last_draw_ms = now;
}

bool display_history_key(display_t *d, const history_t *h, unsigned key)
{
    uint16_t count = history_count(h);
    if (key == GAS_KEY_UP) {
        if (d->history_index > 0u) {
            --d->history_index;
        }
        return true;
    }
    if (key == GAS_KEY_DOWN) {
        if ((unsigned)d->history_index + 1u < (unsigned)count) {
            ++d->history_index;
        }
        return true;
    }
    return false;
}

void display_key(display_t *d, gas_t *gas, const history_t *history, unsigned key, uint32_t now)
{
    if (key == GAS_KEY_CONFIRM) {
        gas_key(gas, key, now);
    } else if (key == GAS_KEY_PAGE) {
        d->page = (uint8_t)((d->page + 1u) % DISPLAY_PAGE_COUNT);
        d->history_index = 0;
    } else if (d->page == DISPLAY_SETTINGS) {
        gas_key(gas, key, now);
    } else if (d->page == DISPLAY_HISTORY) {
        (void)display_history_key(d, history, key);
    }
}





