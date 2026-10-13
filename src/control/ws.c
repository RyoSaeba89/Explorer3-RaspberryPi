/* See ws.h. */
#include "ws.h"

#include <stdio.h>
#include <string.h>

static const char WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

/* --- SHA-1 and base64, only for Sec-WebSocket-Accept (no security role:
 * RFC 6455 uses them to prove the server understood the request) --------------- */

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

typedef struct {
    uint32_t h[5];
    uint64_t total;
    uint8_t  block[64];
    size_t   used;
} sha1_ctx;

static void sha1_block(uint32_t h[5], const uint8_t *p)
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 |
               (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 80; i++)
        w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;                   k = 0xCA62C1D6; }
        uint32_t t = ROL(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ROL(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

static void sha1_init(sha1_ctx *c)
{
    c->h[0] = 0x67452301;
    c->h[1] = 0xEFCDAB89;
    c->h[2] = 0x98BADCFE;
    c->h[3] = 0x10325476;
    c->h[4] = 0xC3D2E1F0;
    c->total = 0;
    c->used = 0;
}

static void sha1_update(sha1_ctx *c, const void *data, size_t len)
{
    const uint8_t *p = data;
    c->total += len;
    while (len > 0) {
        size_t n = 64 - c->used;
        if (n > len)
            n = len;
        memcpy(c->block + c->used, p, n);
        c->used += n;
        p += n;
        len -= n;
        if (c->used == 64) {
            sha1_block(c->h, c->block);
            c->used = 0;
        }
    }
}

static void sha1_final(sha1_ctx *c, uint8_t out[20])
{
    uint64_t bits = c->total * 8;
    uint8_t pad = 0x80;
    sha1_update(c, &pad, 1);
    pad = 0;
    while (c->used != 56)
        sha1_update(c, &pad, 1);
    uint8_t len[8];
    for (int i = 0; i < 8; i++)
        len[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha1_update(c, len, 8);
    for (int i = 0; i < 5; i++) {
        out[4 * i]     = (uint8_t)(c->h[i] >> 24);
        out[4 * i + 1] = (uint8_t)(c->h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(c->h[i] >> 8);
        out[4 * i + 3] = (uint8_t)c->h[i];
    }
}

/* out: 4 * ((len + 2) / 3) characters and a zero */
static void base64(const uint8_t *p, size_t len, char *out)
{
    static const char A[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)p[i] << 16;
        if (i + 1 < len)
            v |= (uint32_t)p[i + 1] << 8;
        if (i + 2 < len)
            v |= p[i + 2];
        out[o++] = A[v >> 18 & 63];
        out[o++] = A[v >> 12 & 63];
        out[o++] = i + 1 < len ? A[v >> 6 & 63] : '=';
        out[o++] = i + 2 < len ? A[v & 63] : '=';
    }
    out[o] = 0;
}

/* --- handshake ------------------------------------------------------------------- */

static void accept_for_key(const char *key, char out[29])
{
    sha1_ctx c;
    uint8_t d[20];
    sha1_init(&c);
    sha1_update(&c, key, strlen(key));
    sha1_update(&c, WS_GUID, sizeof(WS_GUID) - 1);
    sha1_final(&c, d);
    base64(d, sizeof(d), out);
}

int ws_hs_request(ws_hs_t *hs, char *out, size_t cap, const char *host,
                  unsigned port, const char *path, const uint8_t nonce[16])
{
    char key[25];
    base64(nonce, 16, key);
    accept_for_key(key, hs->accept);
    hs->len = 0;

    int n = snprintf(out, cap,
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s:%u\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Key: %s\r\n"
                     "Sec-WebSocket-Version: 13\r\n"
                     "\r\n",
                     path, host, port, key);
    if (n < 0 || (size_t)n >= cap)
        return WS_ERR_NOROOM;
    return n;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* Equal ignoring case, over n characters. */
static bool ieq(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (lower((unsigned char)a[i]) != lower((unsigned char)b[i]))
            return false;
    return true;
}

/* needle (lower case) in [s, s+n), ignoring case. */
static bool icontains(const char *s, size_t n, const char *needle)
{
    size_t m = strlen(needle);
    for (size_t i = 0; i + m <= n; i++)
        if (ieq(s + i, needle, m))
            return true;
    return false;
}

static int hs_check(const ws_hs_t *hs)
{
    const char *p = hs->buf;
    const char *end = hs->buf + hs->len;

    /* status line: "HTTP/1.1 101 ..." */
    const char *eol = strstr(p, "\r\n");
    if (eol == NULL || eol - p < 12 || !ieq(p, "HTTP/1.", 7) || memcmp(p + 8, " 101", 4) != 0)
        return WS_ERR_HS_STATUS;
    p = eol + 2;

    bool upgrade = false, connection = false, accept = false;
    while (p < end) {
        eol = strstr(p, "\r\n");
        if (eol == NULL || eol == p)
            break;  /* empty line: end of the headers */
        const char *colon = memchr(p, ':', (size_t)(eol - p));
        if (colon != NULL) {
            size_t nlen = (size_t)(colon - p);
            const char *v = colon + 1;
            while (v < eol && (*v == ' ' || *v == '\t'))
                v++;
            const char *ve = eol;
            while (ve > v && (ve[-1] == ' ' || ve[-1] == '\t'))
                ve--;
            size_t vlen = (size_t)(ve - v);

            if (nlen == 7 && ieq(p, "upgrade", 7))
                upgrade = vlen == 9 && ieq(v, "websocket", 9);
            else if (nlen == 10 && ieq(p, "connection", 10))
                connection = icontains(v, vlen, "upgrade");
            else if (nlen == 20 && ieq(p, "sec-websocket-accept", 20))
                accept = vlen == 28 && memcmp(v, hs->accept, 28) == 0;
        }
        p = eol + 2;
    }
    if (!upgrade || !connection)
        return WS_ERR_HS_UPGRADE;
    if (!accept)
        return WS_ERR_HS_ACCEPT;
    return 1;
}

int ws_hs_feed(ws_hs_t *hs, const uint8_t *data, size_t len, size_t *consumed)
{
    *consumed = 0;
    for (size_t i = 0; i < len; i++) {
        if (hs->len >= sizeof(hs->buf) - 1)
            return WS_ERR_HS_TOOLONG;
        hs->buf[hs->len++] = (char)data[i];
        hs->buf[hs->len] = 0;
        if (hs->len >= 4 && memcmp(hs->buf + hs->len - 4, "\r\n\r\n", 4) == 0) {
            *consumed = i + 1;
            return hs_check(hs);
        }
    }
    *consumed = len;
    return 0;
}

/* --- reception ------------------------------------------------------------------- */

static void rx_new_frame(ws_rx_t *rx)
{
    rx->hdr_len = 0;
    rx->hdr_need = 2;
    rx->plen = 0;
    rx->got = 0;
}

void ws_rx_init(ws_rx_t *rx)
{
    rx->msg_opcode = 0;
    rx->msg_len = 0;
    rx_new_frame(rx);
}

/* Header complete: decode and check it. */
static int rx_header_done(ws_rx_t *rx)
{
    const uint8_t *h = rx->hdr;
    rx->fin = (h[0] & 0x80) != 0;
    rx->opcode = h[0] & 0x0F;

    uint64_t n = h[1] & 0x7F;
    if (n == 126) {
        n = (uint64_t)h[2] << 8 | h[3];
    } else if (n == 127) {
        n = 0;
        for (int i = 2; i < 10; i++)
            n = n << 8 | h[i];
    }
    rx->plen = n;
    rx->got = 0;

    switch (rx->opcode) {
    case WS_OP_CLOSE:
    case WS_OP_PING:
    case WS_OP_PONG:
        if (!rx->fin || n > 125)
            return WS_ERR_CONTROL;
        break;
    case WS_OP_TEXT:
    case WS_OP_BINARY:
        if (rx->msg_opcode != 0)
            return WS_ERR_SEQUENCE;  /* the previous message was not finished */
        if (n > WS_MAX_MSG)
            return WS_ERR_TOOBIG;
        rx->msg_opcode = rx->opcode;
        rx->msg_len = 0;
        break;
    case WS_OP_CONT:
        if (rx->msg_opcode == 0)
            return WS_ERR_SEQUENCE;
        if (n > WS_MAX_MSG - rx->msg_len)
            return WS_ERR_TOOBIG;
        break;
    default:
        return WS_ERR_OPCODE;
    }
    return 0;
}

/* Payload complete: hand over what is finished. */
static void rx_frame_done(ws_rx_t *rx, ws_msg_fn fn, void *ctx)
{
    if (rx->opcode >= WS_OP_CLOSE) {
        rx->ctrl[rx->plen] = 0;
        fn(ctx, rx->opcode, rx->ctrl, (size_t)rx->plen);
    } else {
        rx->msg_len += (size_t)rx->plen;
        if (rx->fin) {
            int op = rx->msg_opcode;
            size_t n = rx->msg_len;
            rx->msg[n] = 0;
            rx->msg_opcode = 0;
            rx->msg_len = 0;
            fn(ctx, op, rx->msg, n);
        }
    }
    rx_new_frame(rx);
}

int ws_rx_feed(ws_rx_t *rx, const uint8_t *data, size_t len, ws_msg_fn fn, void *ctx)
{
    size_t i = 0;
    while (i < len) {
        if (rx->hdr_len < rx->hdr_need) {
            rx->hdr[rx->hdr_len++] = data[i++];
            if (rx->hdr_len == 2) {
                if (rx->hdr[0] & 0x70)
                    return WS_ERR_RSV;
                if (rx->hdr[1] & 0x80)
                    return WS_ERR_MASKED;
                uint8_t n7 = rx->hdr[1] & 0x7F;
                rx->hdr_need = n7 == 126 ? 4 : n7 == 127 ? 10 : 2;
            }
            if (rx->hdr_len == rx->hdr_need) {
                int r = rx_header_done(rx);
                if (r < 0)
                    return r;
                if (rx->plen == 0)
                    rx_frame_done(rx, fn, ctx);
            }
            continue;
        }

        /* payload: copied straight to its place */
        size_t n = len - i;
        if ((uint64_t)n > rx->plen - rx->got)
            n = (size_t)(rx->plen - rx->got);
        uint8_t *dst = rx->opcode >= WS_OP_CLOSE ? rx->ctrl + rx->got
                                                  : rx->msg + rx->msg_len + rx->got;
        memcpy(dst, data + i, n);
        rx->got += n;
        i += n;
        if (rx->got == rx->plen)
            rx_frame_done(rx, fn, ctx);
    }
    return 0;
}

/* --- sending --------------------------------------------------------------------- */

int ws_build_frame(uint8_t *out, size_t cap, int opcode, const uint8_t *payload,
                   size_t len, const uint8_t mask[4])
{
    size_t h = len < 126 ? 2 : len <= 0xFFFF ? 4 : 10;
    if (cap < h + 4 + len)
        return WS_ERR_NOROOM;

    out[0] = (uint8_t)(0x80 | (opcode & 0x0F));
    if (len < 126) {
        out[1] = (uint8_t)(0x80 | len);
    } else if (len <= 0xFFFF) {
        out[1] = 0x80 | 126;
        out[2] = (uint8_t)(len >> 8);
        out[3] = (uint8_t)len;
    } else {
        out[1] = 0x80 | 127;
        for (int i = 0; i < 8; i++)
            out[2 + i] = (uint8_t)((uint64_t)len >> (56 - 8 * i));
    }
    memcpy(out + h, mask, 4);
    for (size_t i = 0; i < len; i++)
        out[h + 4 + i] = payload[i] ^ mask[i & 3];
    return (int)(h + 4 + len);
}
