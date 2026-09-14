#ifndef HISTORY_H
#define HISTORY_H
#include "gas/gas_monitor.h"
#include "settings/settings.h"
#include <stdbool.h>
#include <stdint.h>

/* 报警记录位于 AT24C02 的上半部分。0x00..0x1f 属于配置模块，0x20..0xff
 * 是 224 字节，正好是 14 条 16 字节的记录。写满后覆盖最旧的一条，而不是
 * 停止记录。 */
#define HISTORY_BASE 0x20u
#define HISTORY_SLOTS 14u
#define HISTORY_SLOT_SIZE 16u

/* 没有 RTC，所以记录里带的是当时的上电时长，而不是它无从知道的日期
 * 时间。 */
typedef struct {
    uint16_t seq;                 /* 从 1 开始，最新一条一目了然 */
    uint32_t uptime_s;
    uint16_t adc[GAS_COUNT];
    uint8_t alarm_mask;           /* 哪几路超过阈值 */
} history_entry_t;

typedef struct {
    const settings_io_t *io;
    uint16_t next_seq;
    uint8_t next_slot;            /* 最旧的槽位，也是下一个复用的槽位 */
    uint8_t count;
} history_t;

/* 扫描所有槽位，只要有一条记录可读就返回 true。EEPROM 读不出来时记录
 * 为空，这不值得让启动失败。 */
bool history_init(history_t *h, const settings_io_t *io);
/* 写入一条记录。seq 在这里赋值，并写回 *entry。 */
bool history_append(history_t *h, history_entry_t *entry);
uint8_t history_count(const history_t *h);
/* 索引 0 是最新的一条记录。为空或越过末尾时返回 false，槽位 CRC 校验
 * 不过时也返回 false，因此写到一半的记录读起来等同于不存在。 */
bool history_get(const history_t *h, uint8_t index, history_entry_t *out);
#endif
