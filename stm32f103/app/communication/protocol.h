#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "gas/gas_monitor.h"
#include "history/history.h"
#include <stdbool.h>
#include <stdint.h>

/* Longest line the protocol emits, including the terminator. */
#define PROTOCOL_LINE_MAX 96u

/* One response is produced a line at a time and only while the link is free,
 * so a fourteen-record history dump never blocks the main loop. The caller
 * pumps with protocol_next() until it returns NULL. */
typedef enum {
    PROTOCOL_IDLE = 0,
    PROTOCOL_STATUS,
    PROTOCOL_CONFIG,
    PROTOCOL_HISTORY,
    PROTOCOL_NOTICE,
    /* Single lines that need no state beyond the line itself. */
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
/* Parses one command line and queues the response. Unrecognised input queues
 * an error line: a silently ignored command is indistinguishable from a dead
 * link, which is exactly what someone typing at it is trying to find out. */
void protocol_command(protocol_t *p, const char *line, uint32_t now);
/* Queues the unsolicited notice for an alarm that has just started. */
void protocol_alarm(protocol_t *p);
/* True while a response is still being sent. */
bool protocol_busy(const protocol_t *p);
/* Returns the next line to send without its terminator, or NULL when the
 * response is complete. The string stays valid until the next call. */
const char *protocol_next(protocol_t *p);
#endif
