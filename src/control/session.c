/* See session.h. */
#include "session.h"

#include <stdio.h>
#include <string.h>

void session_init(session_t *s)
{
    memset(s, 0, sizeof(*s));
    s->state = SES_IDLE;
    panel_init(&s->panel);
    banner_init(&s->banner);
    clocksync_reset(&s->clk);
    sound_init(&s->sound);
    /* black screen: all of it to be shown once */
    for (int y = 0; y < SCR_H; y++)
        s->dirty.bits[y >> 5] |= 1u << (y & 31);
}

int session_begin(session_t *s, uint32_t now, const char *host, unsigned port,
                  const uint8_t nonce[16], char *req, size_t cap)
{
    s->state = SES_HANDSHAKE;
    s->error = 0;
    s->started = s->last_rx = s->now = now;
    s->out_len = 0;
    s->frames = s->texts = s->bad_panels = 0;
    s->rng = (uint32_t)nonce[0] << 24 | (uint32_t)nonce[5] << 16 | (uint32_t)nonce[10] << 8 |
             nonce[15] | 1u;
    ws_rx_init(&s->rx);
    clocksync_reset(&s->clk);
    panel_init(&s->panel);
    banner_init(&s->banner);
    int n = ws_hs_request(&s->hs, req, cap, host, port, "/", nonce);
    if (n < 0) {
        s->state = SES_CLOSED;
        s->error = n;
    }
    return n;
}

static void fail(session_t *s, int error)
{
    if (s->state != SES_CLOSED) {
        s->state = SES_CLOSED;
        s->error = error;
    }
}

void session_lost(session_t *s, int error)
{
    fail(s, error);
}

/* xorshift32: the masks have no security role (RFC 6455 10.3 is about
 * proxies), they only have to change. */
static void mask4(session_t *s, uint8_t m[4])
{
    uint32_t x = s->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng = x;
    m[0] = (uint8_t)(x >> 24);
    m[1] = (uint8_t)(x >> 16);
    m[2] = (uint8_t)(x >> 8);
    m[3] = (uint8_t)x;
}

static void on_text(session_t *s, const char *t, size_t len)
{
    static msg_t m;   /* large (theme); display thread only */
    s->texts++;
    if (msg_parse(t, len, &m) != 0) {
        if (len > 0 && t[0] == 'K')
            s->bad_panels++;
        return;
    }
    clocksync_sample(&s->clk, s->now, m.now);
    switch (m.kind) {
    case 'K':
        panel_update(&s->panel, &m, &s->clk, s->now);
        break;
    case 'M':
        panel_theme(&s->panel, &m);
        break;
    case 'B':
        banner_update(&s->banner, &m, s->now);
        break;
    case 'L':
        s->lang = m.lang;
        s->lang_known = true;
        break;
    case 'T':
    case 'S':
        panel_sound(&s->panel, &m, &s->clk);
        sound_message(&s->sound, &m, &s->clk);
        break;
    case 'R':
        sound_message(&s->sound, &m, &s->clk);
        break;
    default:   /* P: the clock only */
        break;
    }
}

static void on_msg(void *ctx, int opcode, const uint8_t *data, size_t len)
{
    session_t *s = ctx;
    switch (opcode) {
    case WS_OP_BINARY:
        s->frames++;
        rows_apply(s->screen, data, len, &s->dirty);   /* rows already applied stay right */
        break;
    case WS_OP_TEXT:
        on_text(s, (const char *)data, len);
        break;
    case WS_OP_PING: {
        /* pong with the same payload (125 bytes at most), at once */
        uint8_t m[4];
        mask4(s, m);
        int n = ws_build_frame(s->out + s->out_len, sizeof(s->out) - s->out_len, WS_OP_PONG, data, len, m);
        if (n > 0)
            s->out_len += (size_t)n;
        else
            fail(s, WS_ERR_NOROOM);   /* the caller no longer sends: cut */
        break;
    }
    case WS_OP_CLOSE:
        fail(s, SES_ERR_CLOSED);
        break;
    default:   /* PONG: no ping is sent */
        break;
    }
}

int session_feed(session_t *s, const uint8_t *data, size_t len, uint32_t now)
{
    if (s->state != SES_HANDSHAKE && s->state != SES_OPEN)
        return s->error ? s->error : SES_ERR_IO;
    s->now = now;
    if (len == 0)
        return 0;
    s->last_rx = now;
    if (s->state == SES_HANDSHAKE) {
        size_t used;
        int r = ws_hs_feed(&s->hs, data, len, &used);
        if (r < 0) {
            fail(s, r);
            return r;
        }
        if (r == 0)
            return 0;
        s->state = SES_OPEN;
        data += used;
        len -= used;
    }
    int r = ws_rx_feed(&s->rx, data, len, on_msg, s);
    if (r < 0)
        fail(s, r);
    return s->state == SES_CLOSED ? s->error : 0;
}

int session_tick(session_t *s, uint32_t now)
{
    if (s->state == SES_HANDSHAKE && (int32_t)(now - s->started) > SESSION_HS_MS)
        fail(s, SES_ERR_HS_LATE);
    else if (s->state == SES_OPEN && (int32_t)(now - s->last_rx) > SESSION_SILENCE_MS)
        fail(s, SES_ERR_SILENCE);
    return s->state == SES_CLOSED ? s->error : 0;
}

int session_close_frame(session_t *s, uint8_t *out, size_t cap)
{
    const uint8_t code[2] = {0x03, 0xE8};   /* 1000: normal closure */
    uint8_t m[4];
    mask4(s, m);
    return ws_build_frame(out, cap, WS_OP_CLOSE, code, 2, m);
}
