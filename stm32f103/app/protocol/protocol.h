#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "gas/gas.h"
#include "history/history.h"
#include <stdbool.h>
#include <stdint.h>

#define PROTOCOL_LINE_MAX 96u

typedef enum {
    PROTOCOL_IDLE = 0,
    PROTOCOL_STATUS,
    PROTOCOL_CONFIG,
    PROTOCOL_HISTORY,
    PROTOCOL_NOTICE,

    PROTOCOL_ONE
} protocol_job_t;

typedef struct {
    gas_t *monitor;
    const history_t *history;
    protocol_job_t job;
    uint16_t line;
    uint16_t count;
    char text[PROTOCOL_LINE_MAX];
} protocol_t;

void protocol_init(protocol_t *p, gas_t *monitor, const history_t *history);

void protocol_command(protocol_t *p, const char *line, uint32_t now);

void protocol_alarm(protocol_t *p);

bool protocol_busy(const protocol_t *p);

const char *protocol_next(protocol_t *p);
#endif
