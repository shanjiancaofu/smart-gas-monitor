#ifndef HISTORY_H
#define HISTORY_H
#include "gas/gas.h"
#include <stdbool.h>
#include <stdint.h>

/* 配置占 0x00～0x1F，历史从 0x20 开始，每条 16 字节。
 * AT24C64 可用 510 个槽；AT24C02 可用 14 个槽，写满后循环覆盖最旧记录。 */
#define HISTORY_BASE 0x20u
#define HISTORY_SLOTS ((EEPROM_CAPACITY - HISTORY_BASE) / HISTORY_SLOT_SIZE)
#define HISTORY_SLOT_SIZE 16u

/* 没有 RTC，所以记录里带的是当时的上电时长，而不是它无从知道的日期
 * 时间。 */
typedef struct {
    uint16_t seq; /* 从 1 开始，最新一条一目了然 */
    uint32_t uptime_s;
    uint16_t adc[GAS_COUNT];
    uint8_t alarm_mask; /* 哪几路超过阈值 */
} history_entry_t;

typedef struct {
    const config_io_t *io;
    uint16_t next_seq;
    uint16_t next_slot; /* 最旧的槽位，也是下一个复用的槽位 */
    uint16_t count;
} history_t;

/* 扫描所有槽位，只要有一条记录可读就返回 true。EEPROM 读不出来时记录
 * 为空，这不值得让启动失败。 */
bool history_init(history_t *h, const config_io_t *io);
/* 写入一条记录。seq 在这里赋值，并写回 *entry。 */
bool history_add(history_t *h, history_entry_t *entry);
uint16_t history_count(const history_t *h);
/* 索引 0 是最新的一条记录。为空或越过末尾时返回 false，槽位 CRC 校验
 * 不过时也返回 false，因此写到一半的记录读起来等同于不存在。 */
bool history_get(const history_t *h, uint16_t index, history_entry_t *out);
/* 清除历史区；不改配置。失败可能已清除部分记录，返回 false。 */
bool history_clear(history_t *h);
#endif
