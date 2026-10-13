/* Mission control on the Pi: the screens before the stream (Android's, in
 * the style of mission control's pages, drawn through draw.h). The word
 * "Cardputer" appears nowhere (user's rule): the crew may be a Cardputer, an
 * Android device or another Pi. Keys are drawn as those of the device in use
 * (labels.h): keyboard keys, Xbox A B Y, PlayStation symbols. */
#ifndef UI_H
#define UI_H
#include <stdbool.h>
#include <stdint.h>

#include "addr.h"
#include "net.h"

/* Why connecting failed */
enum {
    NET_REASON_NONE,
    NET_REASON_UNREACHABLE,  /* nothing answers (or not within 30 s) */
    NET_REASON_REFUSED,      /* a device answers, but nothing listens on the port */
    NET_REASON_NOT_GAME,     /* the port answers, but not as the game's WebSocket */
    NET_REASON_NO_SOCKET,    /* no socket left */
    NET_REASON_LOST,         /* connected, then lost */
};

enum {
    UI_NO_NETWORK,   /* the Pi has no network */
    UI_SEARCH,       /* looking for the crew on the local network */
    UI_NONE,         /* search over, nothing found */
    UI_LIST,         /* several crews found: choose */
    UI_ADDRESS,      /* typed address */
    UI_CONNECTING,   /* checking an address, or connecting */
    UI_ERROR,
};

typedef struct {
    int      screen;
    int      lang;           /* 0 French, 1 English */
    int      style;          /* keys drawn as: 0 keyboard, 1 Xbox, 2 PlayStation */
    uint32_t now, since;
    char     net_name[64];   /* Wi-Fi name, or "Ethernet" */
    uint8_t  self[4];
    int      progress;       /* search, 0..100 */
    found_t  found[FINDER_MAX];
    int      nfound;
    int      sel;            /* LIST */
    addr_entry_t entry;      /* ADDRESS */
    uint8_t  target[4];      /* CONNECTING, ERROR */
    int      reason;         /* ERROR: NET_REASON_... */
} ui_t;

void ui_draw(const ui_t *u);

/* Over the stream, bottom left as the page's "#etat": "Connexion à
 * l'équipage…" while reconnecting, and a warning when the crew's game is of
 * another version (pages refused). */
void ui_overlay(int lang, bool reconnecting, bool old_game);

/* While the stream is shown: how to leave it, bottom right, for a few
 * seconds after it starts. */
void ui_stream_hint(int lang, int style);

#endif
