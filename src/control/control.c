/* Mission control on the Pi: the Pi as the crew's screen, in place of a web
 * browser. The protocol, the pages, the banner and the sound are in this
 * folder's pure code (session.c and what it uses).
 *
 *   NO_NETWORK  the Pi has no network (lost after the game's Wi-Fi screens)
 *   SEARCH      looking for the crew on the local network; one found:
 *               connect; several: LIST; none: NONE
 *   ADDRESS     typed address (fallback), checked, then CONNECTING
 *   CONNECTING  up to 30 s
 *   ERROR       why, and what to do
 *   STREAM      screen copy, pages and sound; lost connection or 30 s of
 *               silence: reconnects by itself, the picture stays
 *
 * Everything runs on the display thread, except the search (net.c) and the
 * sound (control_mix, audio thread): the session is shared with the sound
 * under a lock. */
#include "control.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "addr.h"
#include "banner.h"
#include "draw.h"
#include "net.h"
#include "sdl_draw.h"
#include "session.h"
#include "sound.h"
#include "ui.h"

#define READ_MAX_PER_FRAME (64 * 1024)
#define RETRY_MS           1000
#define WAIT_MS            30000
#define HINT_MS            5000     /* how to leave the stream, shown at its start */

enum { ST_NO_NETWORK, ST_SEARCH, ST_NONE, ST_LIST, ST_ADDRESS, ST_PROBE, ST_CONNECTING, ST_ERROR, ST_STREAM };

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;   /* everything below */
static session_t ses;
static banner_t banner;   /* the end screen's banner (message B) */
static sound_t  sound;    /* notes and rumble (T, S, R); goes on from one connection to the next */

/* Messages B, T, S, R of the session: the banner and the sound */
static void media(void *ctx, const msg_t *m, const clocksync_t *clk, uint32_t now)
{
    (void)ctx;
    if (m->kind == 'B')
        banner_update(&banner, m, now);
    else
        sound_message(&sound, m, clk);
}
static ui_t ui;
static struct {
    bool    has_ip;
    uint8_t ip[4];   /* last crew address that answered */
    int     lang;    /* last language of the crew: the screens before the stream */
} cfg;
static char cfgPath[512];
static int state = ST_NO_NETWORK;
static int targetPort = PORT_CARDPUTER;
static bool linked, connecting;
static uint32_t retryAt, streamSince;
static uint8_t rxbuf[8192];
static bool quitRequested;

/* network given by main.cpp */
static char netName[64];
static uint8_t netIp[4];
static bool netKnown, netChanged;

/* buttons: presses since the last frame, and held ones */
static unsigned pressed, held;

/* sound */
static bool audioAnchored;
static int audioLatencyMs;

static uint32_t now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000);
}

static bool down(int b)
{
    return (pressed & (1u << b)) != 0;
}

/* --- settings: control.conf, "ip=a.b.c.d" and "lang=fr|en" lines ---------------- */

static void cfg_load(void)
{
    FILE *f = fopen(cfgPath, "r");
    if (!f)
        return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strcspn(line, "\r\n");
        line[n] = 0;
        if (strncmp(line, "ip=", 3) == 0)
            cfg.has_ip = ip_parse(line + 3, n - 3, cfg.ip);
        else if (strcmp(line, "lang=fr") == 0)
            cfg.lang = 0;
        else if (strcmp(line, "lang=en") == 0)
            cfg.lang = 1;
    }
    fclose(f);
}

static void cfg_save(void)
{
    FILE *f = fopen(cfgPath, "w");
    if (!f)
        return;
    if (cfg.has_ip)
        fprintf(f, "ip=%u.%u.%u.%u\n", cfg.ip[0], cfg.ip[1], cfg.ip[2], cfg.ip[3]);
    fprintf(f, "lang=%s\n", cfg.lang ? "en" : "fr");
    fclose(f);
}

static void cfg_follow(void)
{
    bool dirty = false;
    if (ses.state == SES_OPEN && (!cfg.has_ip || memcmp(cfg.ip, ui.target, 4) != 0)) {
        memcpy(cfg.ip, ui.target, 4);   /* an address that answered */
        cfg.has_ip = true;
        dirty = true;
    }
    if (ses.lang_known && ses.lang != cfg.lang) {
        cfg.lang = ses.lang;
        dirty = true;
    }
    if (dirty)
        cfg_save();
}

/* --- connection ----------------------------------------------------------------- */

/* Why link_start() / link_poll() failed: -errno */
static int reason_connect(int err)
{
    if (err == -ECONNRESET || err == -ECONNREFUSED)
        return NET_REASON_REFUSED;    /* RST to the SYN: the device is there */
    if (err == -ENOMEM || err == -ENOBUFS || err == -ENFILE || err == -EMFILE)
        return NET_REASON_NO_SOCKET;
    return NET_REASON_UNREACHABLE;
}

/* Why a session ended */
static int reason_session(int err)
{
    switch (err) {
    case WS_ERR_HS_TOOLONG:
    case WS_ERR_HS_STATUS:
    case WS_ERR_HS_UPGRADE:
    case WS_ERR_HS_ACCEPT:
    case SES_ERR_HS_LATE:
        return NET_REASON_NOT_GAME;
    default:
        return NET_REASON_LOST;
    }
}

static void nonce16(uint8_t out[16])
{
    uint32_t x = now_ms() ^ (uint32_t)rand() ^ 0x9E3779B9u;
    for (int i = 0; i < 16; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        out[i] = (uint8_t)(x >> 8);
    }
}

static bool begin_session(uint32_t now)
{
    char req[256], host[16];
    uint8_t nonce[16];
    addr_format(ui.target, host);
    nonce16(nonce);
    int n = session_begin(&ses, now, host, (unsigned)targetPort, nonce, req, sizeof(req));
    banner_init(&banner);   /* the crew sends its banner again */
    if (n < 0 || link_write(req, n) < 0) {
        link_close();
        return false;
    }
    return true;
}

static void leave_stream(void)
{
    if (linked) {
        uint8_t bye[16];
        int n = session_close_frame(&ses, bye, sizeof(bye));
        if (n > 0)
            link_write(bye, n);
    }
    link_close();
    linked = connecting = false;
    ses.state = SES_IDLE;
}

static void start_search(uint32_t now)
{
    finder_start(ui.self, NULL);
    ui.since = now;
    state = ST_SEARCH;
}

static void connect_to(const uint8_t ip[4], int port, uint32_t now)
{
    memcpy(ui.target, ip, 4);
    targetPort = port;
    ui.since = now;
    ui.reason = NET_REASON_NONE;
    int r = link_start(ip, port);
    if (r < 0) {
        ui.reason = reason_connect(r);
        state = ST_ERROR;
        return;
    }
    state = ST_CONNECTING;
}

static void to_address(uint32_t now)
{
    entry_init(&ui.entry, ui.self, cfg.has_ip ? cfg.ip : NULL);
    ui.since = now;
    state = ST_ADDRESS;
}

static unsigned entry_keys_of(unsigned h)
{
    unsigned k = 0;
    if (h & (1u << CB_LEFT))  k |= KEY_LEFT;
    if (h & (1u << CB_RIGHT)) k |= KEY_RIGHT;
    if (h & (1u << CB_UP))    k |= KEY_UP;
    if (h & (1u << CB_DOWN))  k |= KEY_DOWN;
    if (h & (1u << CB_L))     k |= KEY_L;
    if (h & (1u << CB_R))     k |= KEY_R;
    return k;
}

/* --- states --------------------------------------------------------------------- */

static void step(uint32_t now)
{
    if (netChanged) {
        netChanged = false;
        snprintf(ui.net_name, sizeof(ui.net_name), "%s", netName);
        memcpy(ui.self, netIp, 4);
        if (!netKnown && state != ST_STREAM) {
            state = ST_NO_NETWORK;
        } else if (netKnown && state == ST_NO_NETWORK) {
            start_search(now);
        }
    }
    switch (state) {
    case ST_NO_NETWORK:
        if (down(CB_BACK))
            quitRequested = true;
        break;

    case ST_SEARCH: {
        int st = finder_poll(ui.found, &ui.nfound);
        ui.progress = finder_progress();
        if (down(CB_KB)) {
            to_address(now);
        } else if (down(CB_BACK)) {
            quitRequested = true;
        } else if (st == FINDER_DONE) {
            if (ui.nfound == 1) {
                connect_to(ui.found[0].ip, ui.found[0].port, now);
            } else if (ui.nfound > 1) {
                ui.sel = 0;
                state = ST_LIST;
            } else {
                state = ST_NONE;
            }
        }
        break;
    }

    case ST_NONE:
        if (down(CB_OK))
            start_search(now);
        else if (down(CB_KB))
            to_address(now);
        else if (down(CB_BACK))
            quitRequested = true;
        break;

    case ST_LIST:
        if (down(CB_UP) && ui.sel > 0)
            ui.sel--;
        if (down(CB_DOWN) && ui.sel < ui.nfound - 1 && ui.sel < 5)
            ui.sel++;
        if (down(CB_OK))
            connect_to(ui.found[ui.sel].ip, ui.found[ui.sel].port, now);
        else if (down(CB_KB))
            to_address(now);
        else if (down(CB_BACK))
            quitRequested = true;
        break;

    case ST_ADDRESS:
        entry_keys(&ui.entry, entry_keys_of(held), now);
        if (down(CB_OK)) {
            int st = addr_check(ui.entry.ip, ui.self);
            if (st == ADDR_OK || st == ADDR_OTHER_NET) {   /* other network: warned, not forbidden */
                memcpy(ui.target, ui.entry.ip, 4);
                finder_start(ui.self, ui.entry.ip);         /* which port: Cardputer or Android */
                ui.since = now;
                state = ST_PROBE;
            }
        } else if (down(CB_BACK)) {
            start_search(now);
        }
        break;

    case ST_PROBE: {
        found_t f[FINDER_MAX];
        int n = 0;
        if (down(CB_BACK)) {
            to_address(now);
        } else if (finder_poll(f, &n) == FINDER_DONE) {
            if (n > 0) {
                connect_to(f[0].ip, f[0].port, now);
            } else {
                targetPort = 0;   /* port unknown: trying again probes again */
                ui.reason = NET_REASON_UNREACHABLE;
                state = ST_ERROR;
            }
        }
        break;
    }

    case ST_CONNECTING: {
        if (down(CB_BACK)) {
            link_close();
            start_search(now);
            break;
        }
        int r = link_poll();
        if (r > 0) {
            linked = true;
            connecting = false;
            if (!begin_session(now)) {
                linked = false;
                ui.reason = NET_REASON_LOST;
                state = ST_ERROR;
                break;
            }
            streamSince = now;
            state = ST_STREAM;
        } else if (r < 0) {
            ui.reason = reason_connect(r);
            state = ST_ERROR;
        } else if ((int32_t)(now - ui.since) > WAIT_MS) {
            link_close();
            ui.reason = NET_REASON_UNREACHABLE;
            state = ST_ERROR;
        }
        break;
    }

    case ST_ERROR:
        if (down(CB_OK) && targetPort) {
            connect_to(ui.target, targetPort, now);
        } else if (down(CB_OK)) {
            finder_start(ui.self, ui.target);
            ui.since = now;
            state = ST_PROBE;
        } else if (down(CB_BACK))
            start_search(now);
        break;

    default:   /* ST_STREAM */
        if (down(CB_BACK)) {
            leave_stream();
            start_search(now);
            break;
        }
        if (linked) {
            int total = 0;
            while (total < READ_MAX_PER_FRAME) {
                int n = link_read(rxbuf, sizeof(rxbuf));
                if (n == 0)
                    break;
                if (n < 0) {
                    session_lost(&ses, n == LINK_CLOSED ? SES_ERR_PEER : SES_ERR_IO);
                    break;
                }
                total += n;
                if (session_feed(&ses, rxbuf, (size_t)n, now) < 0)
                    break;
                if (ses.out_len) {   /* pong at once */
                    if (link_write(ses.out, (int)ses.out_len) < 0)
                        session_lost(&ses, SES_ERR_IO);
                    ses.out_len = 0;
                }
            }
            session_tick(&ses, now);
            if (ses.state == SES_CLOSED) {
                int reason = reason_session(ses.error);
                bool wasOpen = ses.frames > 0 || ses.texts > 0;
                link_close();
                linked = false;
                retryAt = now + RETRY_MS;
                if (!wasOpen && reason == NET_REASON_NOT_GAME) {
                    ui.reason = reason;   /* not the game: no point insisting */
                    state = ST_ERROR;
                }
            }
            break;
        }
        /* reconnection */
        if (!connecting) {
            if ((int32_t)(now - retryAt) >= 0 && link_start(ui.target, targetPort) == 0)
                connecting = true;
            break;
        }
        {
            int r = link_poll();
            if (r > 0) {
                connecting = false;
                linked = begin_session(now);
                if (!linked)
                    retryAt = now + RETRY_MS;
            } else if (r < 0) {
                connecting = false;
                retryAt = now + RETRY_MS;
            }
        }
        break;
    }
}

static void draw(uint32_t now)
{
    if (state == ST_STREAM) {
        if (panel_shown(&ses.panel)) {
            panel_draw(&ses.panel, now, ses.lang);
        } else {
            bool changed = false;
            for (size_t i = 0; i < sizeof(ses.dirty.bits) / sizeof(ses.dirty.bits[0]); i++)
                changed |= ses.dirty.bits[i] != 0;
            sd_screen(ses.screen, changed);
            memset(&ses.dirty, 0, sizeof(ses.dirty));
            banner_draw(&banner, now);
        }
        ui_overlay(ses.lang, !linked || ses.state == SES_HANDSHAKE, ses.bad_panels > 0);
        if ((int32_t)(now - streamSince) < HINT_MS)
            ui_stream_hint(ses.lang, ui.style);
        return;
    }
    ui.lang = ses.lang;
    ui.now = now;
    switch (state) {
    case ST_NO_NETWORK: ui.screen = UI_NO_NETWORK; break;
    case ST_SEARCH:     ui.screen = UI_SEARCH; break;
    case ST_NONE:       ui.screen = UI_NONE; break;
    case ST_LIST:       ui.screen = UI_LIST; break;
    case ST_ADDRESS:    ui.screen = UI_ADDRESS; break;
    case ST_ERROR:      ui.screen = UI_ERROR; break;
    default:            ui.screen = UI_CONNECTING; break;   /* PROBE, CONNECTING */
    }
    ui_draw(&ui);
}

/* --- main.cpp ------------------------------------------------------------------- */

void control_open(const char *settings, int lang)
{
    static bool started;
    pthread_mutex_lock(&lock);
    if (!started) {
        started = true;
        snprintf(cfgPath, sizeof(cfgPath), "%s", settings);
        cfg.lang = lang;
        cfg_load();
        session_init(&ses, media, NULL);
        banner_init(&banner);
        sound_init(&sound);
        audioAnchored = false;   /* the sound starts again from its first sample */
        memset(&ui, 0, sizeof(ui));
        srand(now_ms());
    }
    if (!ses.lang_known)
        ses.lang = lang;   /* the game's language, until the crew sends its own */
    quitRequested = false;
    pressed = held = 0;
    if (netKnown)
        start_search(now_ms());
    else
        state = ST_NO_NETWORK;
    pthread_mutex_unlock(&lock);
}

void control_style(int style)
{
    pthread_mutex_lock(&lock);
    ui.style = style;
    pthread_mutex_unlock(&lock);
}

void control_network(const char *name, const char *ip)
{
    uint8_t a[4] = {0};
    bool known = ip_parse(ip, strlen(ip), a);
    pthread_mutex_lock(&lock);
    if (known != netKnown || memcmp(a, netIp, 4) != 0 || strcmp(name, netName) != 0) {
        netKnown = known;
        memcpy(netIp, a, 4);
        snprintf(netName, sizeof(netName), "%s", name);
        netChanged = true;
    }
    pthread_mutex_unlock(&lock);
}

void control_button(int code, bool isDown)
{
    if (code < CB_UP || code > CB_R)
        return;
    pthread_mutex_lock(&lock);
    if (isDown) {
        pressed |= 1u << code;
        held |= 1u << code;
    } else {
        held &= ~(1u << code);
    }
    pthread_mutex_unlock(&lock);
}

bool control_frame(int w, int h)
{
    uint32_t now = now_ms();
    pthread_mutex_lock(&lock);
    step(now);
    pressed = 0;
    cfg_follow();
    sd_begin(w, h);
    draw(now);
    sd_end();
    bool quit = quitRequested;
    quitRequested = false;
    if (quit) {
        leave_stream();
        state = ST_NO_NETWORK;
    }
    pthread_mutex_unlock(&lock);
    return !quit;
}

void control_mix(int16_t *out, int frames, int latencyMs)
{
    pthread_mutex_lock(&lock);
    if (!audioAnchored || latencyMs != audioLatencyMs) {
        sound_anchor(&sound, now_ms() + (uint32_t)latencyMs);   /* this block sounds then */
        audioAnchored = true;
        audioLatencyMs = latencyMs;
    }
    sound_render(&sound, out, frames);
    pthread_mutex_unlock(&lock);
}
