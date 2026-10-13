/* Screen rows sent by the crew (binary WebSocket message). Pure.
 *
 * Format (encodeRow() in the game's diffusion.cpp), rows one after the other
 * in a message:
 *   [y][0][240 pixels x 2 bytes]              raw
 *   [y][1][n][colour, 2 bytes] ...            runs, n = 1..255, up to 240 pixels
 * RGB565 colours, high byte first. */
#ifndef ROWS_H
#define ROWS_H

#include <stddef.h>
#include <stdint.h>

#define SCR_W 240
#define SCR_H 135

/* Errors: the message is malformed from there on. Rows already applied stay
 * valid (each one is complete in itself). */
enum {
    ROWS_ERR_Y     = -1,  /* row number >= SCR_H */
    ROWS_ERR_MODE  = -2,  /* mode neither 0 nor 1 */
    ROWS_ERR_SHORT = -3,  /* message cut in the middle of a row */
    ROWS_ERR_RUN   = -4,  /* empty run, or one past 240 pixels */
};

/* Changed rows: one bit per row, cleared by the caller once used. */
typedef struct {
    uint32_t bits[(SCR_H + 31) / 32];
} rows_dirty_t;

/* Applies a message to screen (SCR_W x SCR_H pixels, row after row).
 * Returns the number of rows applied, or an error. A row is written only
 * once checked whole: an error never leaves a half-updated row. */
int rows_apply(uint16_t *screen, const uint8_t *data, size_t len, rows_dirty_t *dirty);

#endif
