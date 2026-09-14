#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "gas/gas_monitor.h"
#include "history/history.h"
#include <stdbool.h>
#include <stdint.h>

/* 协议发出的最长一行，含结束符。 */
#define PROTOCOL_LINE_MAX 96u

/* 一条应答逐行产生，且只在链路空闲时产生，所以 14 条记录的历史转储不会
 * 阻塞主循环。调用方反复调用 protocol_next()，直到它返回 NULL。 */
typedef enum {
    PROTOCOL_IDLE = 0,
    PROTOCOL_STATUS,
    PROTOCOL_CONFIG,
    PROTOCOL_HISTORY,
    PROTOCOL_NOTICE,
    /* 单行应答，除这一行本身外不需要任何状态。 */
    PROTOCOL_ONE
} protocol_job_t;

typedef struct {
    gas_monitor_t *monitor;
    const history_t *history;
    protocol_job_t job;
    uint8_t line;
    uint8_t count;
    char text[PROTOCOL_LINE_MAX];
} protocol_t;

void protocol_init(protocol_t *p, gas_monitor_t *monitor, const history_t *history);
/* 解析一行命令并排入应答。无法识别的输入会排入一行错误：被静默忽略的
 * 命令与链路不通无法区分，而后者正是对着它敲命令的人想弄清楚的。 */
void protocol_command(protocol_t *p, const char *line, uint32_t now);
/* 为刚刚开始的报警排入主动推送的通知。 */
void protocol_alarm(protocol_t *p);
/* 应答还在发送中时为 true。 */
bool protocol_busy(const protocol_t *p);
/* 返回下一行待发送内容，不含结束符；应答结束时返回 NULL。该字符串在
 * 下一次调用之前保持有效。 */
const char *protocol_next(protocol_t *p);
#endif
