/* WebSocket client (RFC 6455), protocol side. Pure: no network call; it is
 * given the bytes received and returns messages, and builds the bytes to
 * send, which the caller sends.
 *
 * The crew's server (the Cardputer's arduinoWebSockets, or the POSIX server
 * of the Pi and of Android):
 *   - wants GET, "Connection: Upgrade", "Upgrade: websocket", a key and
 *     version 13; it checks neither Origin nor sub-protocol;
 *   - sends each message in one unmasked frame (text, or binary of 12 KB at
 *     most for the picture);
 *   - pings every 5 s and drops a client that missed two pongs in a row.
 * Fragmented frames are reassembled all the same: the standard allows them. */
#ifndef WS_H
#define WS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Largest message accepted. The picture is 12 KB at most (FRAME_BUF in
 * diffusion.cpp), the maze theme about 3 KB; beyond, the stream is not the
 * expected one. */
#define WS_MAX_MSG (16 * 1024)

/* Opcodes (RFC 6455 section 5.2). */
enum {
    WS_OP_CONT   = 0x0,
    WS_OP_TEXT   = 0x1,
    WS_OP_BINARY = 0x2,
    WS_OP_CLOSE  = 0x8,
    WS_OP_PING   = 0x9,
    WS_OP_PONG   = 0xA,
};

/* Errors (always negative). Each one means: close the connection. */
enum {
    WS_ERR_NOROOM     = -1,  /* output buffer too small */
    WS_ERR_HS_TOOLONG = -2,  /* oversized response headers */
    WS_ERR_HS_STATUS  = -3,  /* no "101" */
    WS_ERR_HS_UPGRADE = -4,  /* Upgrade or Connection missing or wrong */
    WS_ERR_HS_ACCEPT  = -5,  /* Sec-WebSocket-Accept missing or wrong */
    WS_ERR_RSV        = -6,  /* RSV bits set: extension not negotiated */
    WS_ERR_MASKED     = -7,  /* masked frame from the server: forbidden */
    WS_ERR_OPCODE     = -8,  /* unknown opcode */
    WS_ERR_CONTROL    = -9,  /* fragmented control frame, or over 125 bytes */
    WS_ERR_TOOBIG     = -10, /* message over WS_MAX_MSG */
    WS_ERR_SEQUENCE   = -11, /* continuation without a start, or start without an end */
};

/* --- handshake ----------------------------------------------------------------- */

typedef struct {
    char   accept[29];   /* expected Sec-WebSocket-Accept (28 chars) */
    char   buf[1024];    /* response headers */
    size_t len;
} ws_hs_t;

/* Writes the GET request into out (with its final zero). nonce: 16 bytes
 * that change at each connection (no need for good entropy). Returns the
 * length, or WS_ERR_NOROOM. Prepares hs to read the response. */
int ws_hs_request(ws_hs_t *hs, char *out, size_t cap, const char *host,
                  unsigned port, const char *path, const uint8_t nonce[16]);

/* Bytes received after the request. Returns 0 until the empty line ending
 * the headers, a positive number when the handshake succeeded, an error
 * otherwise. On success, *consumed tells how many bytes of data were
 * headers: the rest, if any, is already WebSocket stream. */
int ws_hs_feed(ws_hs_t *hs, const uint8_t *data, size_t len, size_t *consumed);

/* --- reception ------------------------------------------------------------------- */

/* Called for each complete message: text or binary (WS_OP_TEXT,
 * WS_OP_BINARY), or control frame (PING, PONG, CLOSE). A text message is
 * followed by a zero (data[len] == 0). data is valid during the call only. */
typedef void (*ws_msg_fn)(void *ctx, int opcode, const uint8_t *data, size_t len);

typedef struct {
    /* frame in progress */
    uint8_t  hdr[14];
    size_t   hdr_len;
    size_t   hdr_need;
    int      opcode;
    bool     fin;
    uint64_t plen;
    uint64_t got;
    /* message being reassembled (text or binary) */
    int      msg_opcode;     /* 0: none */
    size_t   msg_len;
    uint8_t  msg[WS_MAX_MSG + 1];
    /* control frame (125 bytes at most, may come in between) */
    uint8_t  ctrl[126];
} ws_rx_t;

void ws_rx_init(ws_rx_t *rx);

/* Bytes of the stream. Returns 0, or an error: the connection must then be
 * closed, the stream is out of step. Complete messages go to fn as they come. */
int ws_rx_feed(ws_rx_t *rx, const uint8_t *data, size_t len, ws_msg_fn fn, void *ctx);

/* --- sending --------------------------------------------------------------------- */

/* A complete (FIN) frame, masked with mask as a client must. Returns its
 * length or WS_ERR_NOROOM. */
int ws_build_frame(uint8_t *out, size_t cap, int opcode, const uint8_t *payload,
                   size_t len, const uint8_t mask[4]);

#endif
