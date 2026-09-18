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

/* 锁存期间光看 "SAFE" 会以为已经恢复完了：那个状态其实是「还锁着、阀门关着、
 * 风扇开着，等环境安全满 3 秒再按 KEY4」。分成两个词，一眼看出还差什么。 */
static const char *state_text(const gas_t *m)
{
    if (m->state == GAS_SAFE_WAIT) {
        return m->reset_ready ? "READY" : "LOCKED";
    }
    return gas_state_name(m->state);
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
    putf(d, 6u, true, "%-7s %s", state_text(m), valve_name(gas_valve_open(m)));
}

/* 可调项有 5 个，加上标题一共 6 行。大号字体一行占两个 page，8 个 page 只放
 * 得下 4 行，于是采样周期和蜂鸣器档位被挤出屏幕：按 KEY5 切到那两项时光标整
 * 个消失，只能盲调。这里改用小号字体，一行一个 page，6 行放得下。
 *
 * 行号直接由 gas_item_t 推出来（标题占 0，之后每项一行），免得以后加可调项时
 * 又漏画——这正是这次出问题的地方。 */
static void draw_settings(display_t *d, const gas_t *m, bool storage_ok)
{
    char value[16];
    unsigned i;
    putf(d, 0u, false, "SETTINGS");
    for (i = 0; i < GAS_COUNT; ++i) {
        putf(d, i + 1u, false, "%c%s %u", m->item == (uint8_t)i ? '>' : ' ',
             gas_channel_name((gas_channel_t)i), m->config.alarm[i]);
    }
    putf(d, GAS_ITEM_PERIOD + 1u, false, "%cPERIOD %uMS",
         m->item == GAS_ITEM_PERIOD ? '>' : ' ', m->config.sample_period_ms);
    config_buzzer_name(m->config.buzzer, value, sizeof(value));
    putf(d, GAS_ITEM_BUZZER + 1u, false, "%cBUZZER %s",
         m->item == GAS_ITEM_BUZZER ? '>' : ' ', value);
    /* 存储掉了的时候，历史页只会显示「没有记录」，跟「真的没报过警」分不清。
     * 把存储状态摆在设置页，一眼能判断 HIST 0/510 是哪种。 */
    putf(d, GAS_ITEM_BUZZER + 2u, false, "MEM %s", storage_ok ? "OK" : "ERR");
}

static void draw_alarm(display_t *d, const gas_t *m)
{
    unsigned i;
    putf(d, 0u, true, "!!! ALARM !!!");
    for (i = 0; i < GAS_COUNT; ++i)
        putf(d, (i + 1u) * 2u, true, "%c%s %5u", (m->alarm_mask & (1u << i)) ? '>' : ' ', gas_channel_name((gas_channel_t)i), m->adc[i]);
}

static void draw_fault(display_t *d)
{
    putf(d, 0u, true, "!!! FAULT !!!");
    putf(d, 2u, true, "ADC/COMM ERROR");
    putf(d, 4u, true, "VALVE:CLOSE");
    putf(d, 6u, true, "CHECK HARDWARE");
}

/* 记录存在 EEPROM 里，整条 16 字节读一次在软件 I2C 下不便宜，而站着这一页不动
 * 的时候它根本不会变——先前每次重画都回读，白花掉每 200 ms 一次的总线时间。缓存
 * 解码结果，只有换了下标、或者期间真的写进过新记录才回读。 */
static void draw_history(display_t *d, const history_t *h)
{
    const history_entry_t *entry;
    uint16_t count = history_count(h);
    putf(d, 0u, true, "HIST %u/%u", count, HISTORY_SLOTS);
    if (count == 0u) {
        d->history_cached = false;
        putf(d, 2u, true, "NO RECORDS"); putf(d, 4u, true, "KEY2/3 BROWSE"); putf(d, 6u, true, "KEY1 BACK");
        return;
    }
    if (!d->history_cached || d->drawn_history_index != d->history_index ||
        d->drawn_history_seq != h->next_seq) {
        if (!history_get(h, d->history_index, &d->drawn_history)) {
            d->history_cached = false;
            putf(d, 2u, true, "RECORD INVALID"); putf(d, 4u, true, "CHECK EEPROM"); putf(d, 6u, true, "KEY2/3 BACK");
            return;
        }
        /* 下标要在这里才更新：它既是缓存的有效条件，也是 display_update() 判断
         * 「换了一条记录、该整屏推」的依据，提前写掉会让那个判断永远不成立。 */
        d->drawn_history_index = d->history_index;
        d->drawn_history_seq = h->next_seq;
        d->history_cached = true;
    }
    entry = &d->drawn_history;
    putf(d, 0u, true, "HIST %u/%u", d->history_index + 1u, count);
    putf(d, 2u, true, "SEQ %u UP%lus", entry->seq, (unsigned long)entry->uptime_s);
    putf(d, 4u, true, "MQ4 %u MQ6 %u", entry->adc[GAS_MQ4], entry->adc[GAS_MQ6]);
    putf(d, 6u, true, "MQ7 %u A%u", entry->adc[GAS_MQ7], entry->alarm_mask);
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
    bool full;
    if (!bsp_oled_is_ready(d->oled)) {
        return;
    }
    /* Comment normalized for portability. */
    screen = m->state == GAS_ALARM ? DISPLAY_ALARM :
             (m->state == GAS_FAULT ? DISPLAY_FAULT : d->page);
    /* 整屏推的判据是「屏幕上要换成另一片内容」：换了一页，或者使用者在历史页上
     * 翻到了另一条记录。后者由 display_history_key() 置 force_full，不在这里反
     * 推——翻页后 draw_history() 就把当前下标记成「已画」，再来比对永远相等。 */
    full = screen != d->screen || d->force_full;
    d->force_full = false;
    if (screen == d->screen && d->oled->dirty == 0u &&
        (uint32_t)(now - d->last_draw_ms) < DISPLAY_REFRESH_MS) {
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
    /* 稳态每帧只推一个脏页：整屏 1024 字节走软件 I2C 要 700 ms 以上，每帧推满
     * 会把主循环连同采样一起卡住，而实时页一次也只变那么几行。
     *
     * full 那一帧是例外，要推满。清屏把八个 page 全标了脏，只推一个的话要八帧、
     * 约 0.7 秒才铺完；这期间屏幕上新旧两页的内容并存——新页的行压在旧页的行
     * 上面，看着就是花的。翻历史记录同理：四条大字占满八页，一条一条推出来就是
     * 半条旧记录配半条新记录。整屏推一次的阻塞时间，正是采样超时窗口预留的那笔
     * （见 config.h 里 GAS_SAMPLE_TIMEOUT_MARGIN_MS 的说明）。 */
    bsp_oled_flush(d->oled, full ? SSD1306_PAGES : 1u);
    d->screen = screen;
    d->last_draw_ms = now;
}

bool display_history_key(display_t *d, const history_t *h, unsigned key)
{
    uint16_t count = history_count(h);
    if (key == GAS_KEY_UP) {
        if (d->history_index > 0u) {
            --d->history_index;
            /* 换成另一条就整屏推：四条大字占满八页，一页一页推出来就是半条旧
             * 记录配半条新记录。已经在头一条上则什么都不会变，不推。 */
            d->force_full = true;
        }
        return true;
    }
    if (key == GAS_KEY_DOWN) {
        if ((unsigned)d->history_index + 1u < (unsigned)count) {
            ++d->history_index;
            d->force_full = true;
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





