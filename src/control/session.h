/* One connection to the crew, protocol side. Pure: no network call.
 *
 * control.c opens the socket, sends session_begin()'s request, then gives
 * session_feed() all it receives. The session:
 *   - checks the handshake (ws.c);
 *   - applies the picture (rows.c), follows the clock (clocksync.c), the
 *     pages and the maze theme (panel.c), the banner (banner.c), the
 *     language and the sound (sound.c);
 *   - prepares a pong for each ping (s->out), which the caller sends;
 *   - tells when to close: stream error, server closing, handshake too slow
 *     (SESSION_HS_MS) or silence (SESSION_SILENCE_MS).
 *
 * What is shown lives here and outlives reconnections: the screen stays the
 * last picture received, as on the page. */
#ifndef SESSION_H
#define SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "banner.h"
#include "clocksync.h"
#include "msg.h"
#include "panel.h"
#include "rows.h"
#include "sound.h"
#include "ws.h"

#define SESSION_HS_MS      10000   /* 101 response expected within 10 s */
#define SESSION_SILENCE_MS 30000   /* 30 s without a byte */

enum {
    SES_IDLE,        /* no connection */
    SES_HANDSHAKE,   /* request sent, waiting for the 101 */
    SES_OPEN,        /* stream running */
    SES_CLOSED,      /* to be closed: see error */
};

/* Why it ended (error), besides ws.h's WS_ERR_... */
enum {
    SES_ERR_CLOSED  = -100,  /* close frame from the server */
    SES_ERR_HS_LATE = -101,  /* no 101 in time */
    SES_ERR_SILENCE = -102,  /* nothing for SESSION_SILENCE_MS */
    SES_ERR_PEER    = -103,  /* connection closed by the crew (recv = 0) */
    SES_ERR_IO      = -104,  /* read or write error */
};

typedef struct {
    int      state;
    int      error;
    ws_hs_t  hs;
    ws_rx_t  rx;
    uint32_t now;            /* time of the current feed */
    uint32_t started;        /* time of session_begin */
    uint32_t last_rx;        /* last byte received */
    uint32_t rng;            /* masks of the frames sent */

    /* to send by the caller (pongs); it sets out_len back to 0 */
    uint8_t  out[4 * 136];
    size_t   out_len;

    /* what is shown */
    uint16_t     screen[SCR_W * SCR_H];
    rows_dirty_t dirty;
    clocksync_t  clk;
    panel_t      panel;
    banner_t     banner;
    int          lang;       /* 0 French, 1 English (message L) */
    bool         lang_known; /* a message L arrived */
    sound_t      sound;      /* notes and rumble (T, S, R); goes on from one connection to the next */

    /* counts, since session_begin */
    uint32_t frames, texts;
    uint32_t bad_panels;     /* K messages refused: a game of another version? */
} session_t;

/* Once, at start: black screen, French, no page. */
void session_init(session_t *s);

/* New connection (the socket has just opened): writes the request into req
 * (cap bytes) and returns its length, or an error. nonce: 16 bytes that
 * change at each connection. The clock, the page, the theme and the banner
 * start again: the crew sends its whole state to each new connection. */
int session_begin(session_t *s, uint32_t now, const char *host, unsigned port,
                  const uint8_t nonce[16], char *req, size_t cap);

/* Bytes received at time now. Returns 0, or an error (state = SES_CLOSED):
 * the socket must then be closed. */
int session_feed(session_t *s, const uint8_t *data, size_t len, uint32_t now);

/* At each frame: handshake too slow, silence. Returns 0 or the error
 * (state = SES_CLOSED). */
int session_tick(session_t *s, uint32_t now);

/* The socket was closed by the crew or a read/write failed (SES_ERR_PEER
 * or SES_ERR_IO). */
void session_lost(session_t *s, int error);

/* Polite close frame (code 1000) to send before cutting. Returns its length
 * or an error. */
int session_close_frame(session_t *s, uint8_t *out, size_t cap);

#endif
