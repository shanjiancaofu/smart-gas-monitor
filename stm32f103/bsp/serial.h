#ifndef SERIAL_H
#define SERIAL_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Longest line the receiver assembles, terminator included. A longer line is
 * discarded whole rather than truncated into a different command. */
#define SERIAL_LINE_MAX 48u
/* Enough for the longest response the protocol emits in one call, so a write
 * that starts on an idle port always fits and is never dropped. */
#define SERIAL_TX_RING 128u
#define SERIAL_RX_RING 64u

typedef struct {
    UART_HandleTypeDef *uart;
    uint8_t rx_byte;                 /* handed to the HAL one byte at a time */
    volatile uint8_t rx[SERIAL_RX_RING];
    volatile uint16_t rx_head, rx_tail;
    volatile uint8_t tx[SERIAL_TX_RING];
    volatile uint16_t tx_head, tx_tail;
    char line[SERIAL_LINE_MAX];
    uint8_t length;
    bool dropping;
    bool ready;
    bool tx_busy;
} serial_t;

/* Both directions are interrupt driven: the main loop never waits for a
 * character to shift out, because a long response would otherwise stall
 * sampling for longer than the sampler's own fault timeout. */
void serial_init(serial_t *s, UART_HandleTypeDef *uart);
/* Retries a transmission the HAL refused to start. Call once per main loop. */
void serial_poll(serial_t *s);

/* Completes one line from what has arrived so far, or returns false. Only one
 * port may be read per call site; each call consumes what it took. */
bool serial_read_line(serial_t *s, char *line, size_t size);
/* Queues text for transmission and returns at once. Returns false, sending
 * nothing, when the queue cannot hold it: a partial line would be worse than
 * none, since the far end is reading this as line-oriented text. */
bool serial_write(serial_t *s, const char *text);
bool serial_busy(const serial_t *s);
#endif
