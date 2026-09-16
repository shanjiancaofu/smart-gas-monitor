#include "display/display.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 涓ょ瀛楁ā閮芥槸绛夊鐨勶紝涓€琛屾濂芥槸鏁存暟涓瓧绗﹀锛?28 / 8 鍜?126 / 6锛?
 * 灏忓彿琛屾湯灏剧殑涓や釜鍍忕礌鐣欑┖涓嶇敤銆?*/
#define LARGE_COLS 16u
#define SMALL_COLS 21u
#if !defined(__ARMCC_VERSION)
_Static_assert(SMALL_COLS >= LARGE_COLS, "display row buffer is too small");
#endif
/* 涓€琛屾牸寮忓寲鏂囧瓧鐨勬殏瀛樺尯锛岄殢鍚庝細琛ラ綈鍒伴潰鏉夸竴琛岀殑瀹藉害銆?*/
#define LINE_MAX 48u

/* 琛ラ綈鍒版暣琛岋紝浣胯琛屽師鍏堣緝闀跨殑鍐呭琚鐩栵紝鑰屼笉鏄畫鐣欏湪瀹冨悗闈€?*/
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
    /* 闃€闂ㄥ瓧鏍蜂笌鐘舵€佸苟鎺掓樉绀猴紝鍥犱负涓よ€呬笉涓€鑷存槸鍒绘剰鐨勶細閿佸瓨鏃舵樉绀?ALARM
     * 鑰岄榾闂ㄤ繚鎸佸叧闂紝宸茬粡鎭㈠鏃舵樉绀?SAFE 鑰岄榾闂ㄤ粛鐒跺叧闂€?*/
    putf(d, 6u, true, "%-7s %s", gas_state_name(m->state), valve_name(gas_valve_open(m)));
}

static void draw_settings(display_t *d, const gas_t *m, bool storage_ok)
{
    char buzz[8];
    unsigned i;
    /* 鍏夋爣璺熺潃 m->item 璧帮細鍓嶄笁琛屾槸闃堝€硷紝鍚庨潰涓ら」鍚勫崰涓€琛屻€?*/
    putf(d, 0u, false, "SETTINGS");
    for (i = 0; i < GAS_COUNT; ++i) {
        putf(d, 1u + i, false, "%c %s  TH %5u", m->item == (uint8_t)i ? '>' : ' ',
             gas_channel_name((gas_channel_t)i), m->config.alarm[i]);
    }
    putf(d, 4u, false, "%c PERIOD %5u ms", m->item == GAS_ITEM_PERIOD ? '>' : ' ',
         m->config.sample_period_ms);
    config_buzzer_name(m->config.buzzer, buzz, sizeof(buzz));
    putf(d, 5u, false, "%c BUZZ %s", m->item == GAS_ITEM_BUZZER ? '>' : ' ', buzz);
    /* STORE 涓?ALARMS 骞舵垚涓€琛岋紝鎶婅吘鍑烘潵鐨勯偅涓€琛岀粰铚傞福鍣ㄢ€斺€斿叓琛屾槸闈㈡澘鐨勭墿鐞?
     * 涓婇檺锛屾病鏈夐〉鍐呯炕椤靛彲鐢ㄣ€傛帀鐢靛悗鑳界暀涓嬬殑鏄瓨鍌ㄧ殑閭ｄ竴浠斤紝鎵€浠ュ嵆浣胯繍琛屼腑
     * 鐨勯厤缃笉鍙楀奖鍝嶏紝鍐欏叆澶辫触涔熷€煎緱鏄剧ず鍑烘潵銆?*/
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
    /* 鍝嚑璺秴鏍囷紝浠ュ強姣忎竴璺綋鏃剁殑璇绘暟銆?*/
    for (i = 0; i < GAS_COUNT; ++i) {
        putf(d, 1u + i, false, "%c %s %5u", (m->alarm_mask & (1u << i)) ? '>' : ' ',
             gas_channel_name((gas_channel_t)i), m->adc[i]);
    }
    putf(d, 4u, false, "TRIGGER %s", mask);
    putf(d, 5u, false, "VALVE:CLOSE");
    /* 闃€闂ㄥ彧鑳界敱鐜板満鐨勪汉鎭㈠锛屾墍浠ヨ繖涓€琛屾槸鎻愮ず鑰屼笉鏄彲閫夐」銆?*/
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
    /* 绱㈠紩涓庡簭鍙烽兘鏄剧ず锛氱储寮曞搴旂炕闃呴敭鐨勪綅缃紝搴忓彿璇存槑鎬诲叡鍙戠敓杩囧灏戞
     * 鎶ヨ銆?*/
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
    /* 鎶ヨ鏃堕《涓婃姤璀﹂〉锛屼笉绠′娇鐢ㄨ€呭仠鍦ㄩ偅涓€椤碉紱鎶ヨ瑙ｉ櫎鍚庤嚜鍔ㄩ€€鍥炰粬鍘熸潵閭ｄ竴椤碉紝
     * 鍥犱负 d->page 涓€鐩存槸浠栫殑閫夋嫨锛屾病鏈夎鎶ヨ鏀瑰啓銆?*/
    screen = m->state == GAS_ALARM ? DISPLAY_ALARM :
             (m->state == GAS_FAULT ? DISPLAY_FAULT : d->page);
    if (screen == d->screen && (uint32_t)(now - d->last_draw_ms) < DISPLAY_REFRESH_MS) {
        return;
    }
    /* 鍒囨崲椤甸潰浼氱暀涓嬫柊椤甸潰涓嶇粯鍒剁殑琛岋紝鎵€浠ョ紦鍐插尯鍏堟竻绌猴紝鑰屼笉鏄緷璧栨瘡涓?
     * 椤甸潰閮借鐩栧叏閮ㄥ叓琛屻€?*/
    if (screen != d->screen) {
        bsp_oled_clear(d->oled);
        /* 娴忚浣嶇疆鎸夋瘡娆¤繘鍏ラ〉闈㈤噸缃細鍐嶆杩涘叆璇ラ〉搴旀樉绀烘渶鏂颁竴鏉℃姤璀︼紝
         * 鑰屼笉鏄笂娆＄寮€鏃剁殑浣嶇疆銆?*/
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

