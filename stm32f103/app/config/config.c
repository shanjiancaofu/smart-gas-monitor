#include "config/config.h"
#include <string.h>
#include <stdio.h>

void config_defaults(gas_config_t *c)
{
    /* 课设演示用的 ADC counts，不是标定过的 ppm 阈值。三路先都取 2400：板子
     * 上不接 MQ 时 ADC 引脚浮空，停在 3.3V 中点约 2048，阈值设在它之下会空
     * 载报警。接上传感器后按实测标定值逐路调。 */
    c->alarm[0] = 2400;
    c->alarm[1] = 2400;
    c->alarm[2] = 2400;
    c->sample_period_ms = 100;
    /* 与改动前那个固定的 5 秒一致，升级到 v5 不会让蜂鸣器行为跟着变。 */
    c->buzzer = 5u;
    c->lockout = false;
}

bool config_valid(const gas_config_t *c)
{
    unsigned i;
    for (i = 0; i < GAS_COUNT; ++i) {
        if (c->alarm[i] < GAS_THRESHOLD_MIN || c->alarm[i] > GAS_THRESHOLD_MAX) {
            return false;
        }
    }
    /* 蜂鸣器只判上界：0 是合法取值（不响），而下界由无符号类型本身兜住。 */
    return c->buzzer <= GAS_BUZZER_ALWAYS && c->sample_period_ms >= GAS_PERIOD_MIN_MS &&
           c->sample_period_ms <= GAS_PERIOD_MAX_MS && c->sample_period_ms % GAS_TICK_MS == 0u;
}

void config_buzzer_name(uint8_t value, char *out, size_t size)
{
    if (size == 0u) {
        return;
    }
    /* 判 >= 而不是 == ：万一有个超出范围的值得了进来，它读作最长的那个档位，
     * 而不是显示成「62S」这种不存在的设置。 */
    if (value == GAS_BUZZER_OFF) {
        (void)snprintf(out, size, "OFF");
    } else if (value >= GAS_BUZZER_ALWAYS) {
        (void)snprintf(out, size, "ALWAYS");
    } else {
        (void)snprintf(out, size, "%uS", (unsigned)value);
    }
}

uint16_t config_buzzer_duration_ms(const gas_config_t *config)
{
    if (config->buzzer == GAS_BUZZER_OFF) {
        return 0u;
    }
    if (config->buzzer >= GAS_BUZZER_ALWAYS) {
        return GAS_BUZZER_FOREVER_MS;
    }
    return (uint16_t)((uint16_t)config->buzzer * 1000u);
}

/* 两个 16 字节槽，位于 0x00 和 0x10。最后一个字节是提交标志。
 *
 * 布局（版本 5）：0 magic / 1 version / 2 sequence / 3 buzzer / 4-9 alarm[3] /
 * 10-11 sample_period_ms / 12 lockout / 13-14 CRC16(覆盖 0..12) / 15 commit。
 *
 * 版本 5 把 sequence 由 2 字节压成 1 字节，腾出的字节 3 放蜂鸣器时长：槽是
 * 16 字节满的，不腾地方就没有字节给它，而扩大槽会挤掉历史区。这个压缩不改变
 * 行为——sequence 只用来判断两个槽哪个更新，它 +1 递增、按模比较，相邻两次
 * 写入永远相差 1，8 位和 16 位的结果一样。占用腾出来的字节、而不是把后面的
 * 字段整体前移，是为了让其余字段的偏移和 v4 保持一致。 */
#define CONFIG_SLOT_SIZE 16u
#define CONFIG_MAGIC 0xa5u
#define CONFIG_VERSION 6u
#define CONFIG_COMMIT 0x5au

static uint16_t crc16(const uint8_t *p, unsigned n)
{
    uint16_t crc = 0xffff;
    unsigned i;
    while (n--) {
        crc ^= (uint16_t)*p++ << 8;
        for (i = 0; i < 8; ++i) {
            crc = (uint16_t)((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
        }
    }
    return crc;
}
static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}
static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static bool decode(const uint8_t *b, gas_config_t *c)
{
    unsigned i;
    if (b[0] != CONFIG_MAGIC || b[1] != CONFIG_VERSION || b[15] != CONFIG_COMMIT ||
        get16(b + 13) != crc16(b, 13)) {
        return false;
    }
    for (i = 0; i < GAS_COUNT; ++i) {
        c->alarm[i] = get16(b + 4 + i * 2);
    }
    c->sample_period_ms = get16(b + 10);
    c->lockout = b[12] != 0u;
    c->buzzer = b[3];
    return config_valid(c);
}
static int latest(uint8_t b[2][CONFIG_SLOT_SIZE], bool valid[2])
{
    if (!valid[0]) {
        return valid[1] ? 1 : -1;
    }
    if (!valid[1]) {
        return 0;
    }
    /* 两个序号只差 1，所以按模跨过半圈的那一半比较，序号回绕时也成立。 */
    return (uint8_t)(b[1][2] - b[0][2]) < 0x80u ? 1 : 0;
}
static bool read_slots(const config_io_t *io, uint8_t b[2][CONFIG_SLOT_SIZE], bool valid[2])
{
    gas_config_t c;
    unsigned i;
    for (i = 0; i < 2; ++i) {
        if (!io->read(io->context, (uint16_t)(i * CONFIG_SLOT_SIZE), b[i], CONFIG_SLOT_SIZE)) {
            return false;
        }
        valid[i] = decode(b[i], &c);
    }
    return true;
}
bool config_load(const config_io_t *io, gas_config_t *c)
{
    return config_load_status(io, c) == CONFIG_LOAD_OK;
}

config_load_status_t config_load_status(const config_io_t *io, gas_config_t *c)
{
    uint8_t b[2][CONFIG_SLOT_SIZE];
    bool valid[2];
    int slot;
    if (!read_slots(io, b, valid)) {
        return CONFIG_LOAD_IO_ERROR;
    }
    slot = latest(b, valid);
    if (slot < 0) {
        return CONFIG_LOAD_EMPTY;
    }
    return decode(b[slot], c) ? CONFIG_LOAD_OK : CONFIG_LOAD_EMPTY;
}
bool config_save(const config_io_t *io, const gas_config_t *c)
{
    uint8_t b[2][CONFIG_SLOT_SIZE], record[CONFIG_SLOT_SIZE] = {0}, verify[CONFIG_SLOT_SIZE];
    uint8_t marker = 0;
    bool valid[2];
    int slot;
    unsigned i;
    uint16_t address;
    uint8_t sequence;
    if (!config_valid(c) || !read_slots(io, b, valid)) {
        return false;
    }
    slot = latest(b, valid);
    sequence = slot < 0 ? 0u : (uint8_t)(b[slot][2] + 1u);
    address = slot == 0 ? CONFIG_SLOT_SIZE : 0;
    record[0] = CONFIG_MAGIC;
    record[1] = CONFIG_VERSION;
    record[2] = sequence;
    record[3] = c->buzzer;
    for (i = 0; i < GAS_COUNT; ++i) {
        put16(record + 4 + i * 2, c->alarm[i]);
    }
    put16(record + 10, c->sample_period_ms);
    record[12] = c->lockout ? 1u : 0u;
    put16(record + 13, crc16(record, 13));
    record[15] = CONFIG_COMMIT;
    /* 先清除提交标志，这样写入被打断时，槽位会处于无效状态，而不是更新到
     * 一半。 */
    if (!io->write(io->context, (uint16_t)(address + CONFIG_SLOT_SIZE - 1u), &marker, 1) ||
        !io->write(io->context, address, record, CONFIG_SLOT_SIZE - 1u) ||
        !io->write(io->context, (uint16_t)(address + CONFIG_SLOT_SIZE - 1u),
                   record + CONFIG_SLOT_SIZE - 1u, 1) ||
        !io->read(io->context, address, verify, CONFIG_SLOT_SIZE)) {
        return false;
    }
    return memcmp(record, verify, CONFIG_SLOT_SIZE) == 0;
}
