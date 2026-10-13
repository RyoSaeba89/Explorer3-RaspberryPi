/* See ui.h. Layout and colours follow mission control's pages (panel.c),
 * with the Pi's keys. */
#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "text.h"
#include "texts.h"

#define C_BG      0x06061a
#define C_TEAM    0x50c8ff
#define C_TITLE   0xffa028
#define C_TEXT    0xeeeeee
#define C_DIM     0x8c96af
#define C_WARN    0xf03c32
#define C_FIELD   0x202642
#define C_BORDER  0x465a8c
#define C_SEL     0xffe146
#define C_STATUS  0xff8800   /* #etat of the page */

#define VH        (CANVAS_H / 100.0f)
#define S_BODY    DRAW_MIN_SCALE
#define S_BIG     3.0f
#define TEXT_W    880.0f
#define LINE_H    (GLYPH_H * S_BODY + 4)
#define GAP       (2.2f * VH)
#define TRACK     (0.25f * GLYPH_W * S_BODY)
#define CX        (CANVAS_W / 2)

/* --- texts ------------------------------------------------------------------- */

enum {
    T_TEAM, T_NET_TITLE, T_NO_NETWORK,
    T_SEARCH_TITLE, T_SEARCH_ON, T_NONE, T_NONE_HELP, T_LIST_TITLE, T_CREW,
    T_ADDR_TITLE, T_ADDR_HELP, T_ADDR_OTHER_NET, T_ADDR_INVALID, T_ADDR_IS_SELF, T_ADDR_NET_BCAST,
    T_CONN_TITLE, T_CONN_WAIT, T_ERR_TITLE, T_ERR_UNREACHABLE, T_ERR_REFUSED, T_ERR_NOT_GAME,
    T_ERR_NO_SOCKET, T_LOST, T_OLD_GAME,
    /* button hints */
    T_K_QUIT, T_K_BACK, T_K_SEARCH_AGAIN, T_K_TYPE, T_K_CONNECT, T_K_CHOOSE, T_K_NUMBER, T_K_TEN,
    T_K_RETRY, T_K_CANCEL, T_K_LEAVE,
    T_COUNT
};

static const char *const TEXTS[T_COUNT][2] = {
    [T_TEAM]           = {"CENTRE DE CONTRÔLE", "MISSION CONTROL"},
    [T_NET_TITLE]      = {"RÉSEAU", "NETWORK"},
    [T_NO_NETWORK]     = {"Pas de réseau : revenez au menu, puis Multijoueur pour choisir un Wi-Fi.",
                          "No network: go back to the menu, then Multiplayer to choose a Wi-Fi."},
    [T_SEARCH_TITLE]   = {"RECHERCHE DE L'ÉQUIPAGE", "LOOKING FOR THE CREW"},
    [T_SEARCH_ON]      = {"Réseau : ", "Network: "},
    [T_NONE]           = {"Aucun équipage trouvé.", "No crew found."},
    [T_NONE_HELP]      = {"Sur l'appareil de l'équipage : Multijoueur, sur le même réseau. Son adresse "
                          "s'affiche sur l'écran CONNECTER L'ÉCRAN.",
                          "On the crew's device: Multiplayer, on the same network. Its address shows "
                          "on the CONNECT THE SCREEN screen."},
    [T_LIST_TITLE]     = {"ÉQUIPAGES TROUVÉS", "CREWS FOUND"},
    [T_CREW]           = {"Équipage ", "Crew "},
    [T_ADDR_TITLE]     = {"ADRESSE DE L'ÉQUIPAGE", "CREW ADDRESS"},
    [T_ADDR_HELP]      = {"Elle s'affiche sur l'écran CONNECTER L'ÉCRAN de l'équipage.",
                          "It shows on the crew's CONNECT THE SCREEN screen."},
    [T_ADDR_OTHER_NET] = {"Attention : cette adresse n'est pas sur le réseau de cet appareil.",
                          "Warning: this address is not on this device's network."},
    [T_ADDR_INVALID]   = {"Adresse impossible.", "Impossible address."},
    [T_ADDR_IS_SELF]   = {"C'est l'adresse de cet appareil.", "That is this device's own address."},
    [T_ADDR_NET_BCAST] = {"Adresse réservée du réseau : impossible.", "Reserved network address: impossible."},
    [T_CONN_TITLE]     = {"CONNEXION", "CONNECTING"},
    [T_CONN_WAIT]      = {"Connexion à l'équipage…", "Connecting to the crew…"},
    [T_ERR_TITLE]      = {"CONNEXION IMPOSSIBLE", "CONNECTION FAILED"},
    [T_ERR_UNREACHABLE] = {"Équipage injoignable. Est-il allumé, en mode Multijoueur, sur le même réseau ? "
                           "L'adresse est-elle la bonne ?",
                           "Crew unreachable. Is it on, in Multiplayer mode, on the same network? "
                           "Is the address right?"},
    [T_ERR_REFUSED]    = {"Un appareil répond à cette adresse, mais le mode Multijoueur n'y est pas lancé.",
                          "A device answers at this address, but Multiplayer mode is not running on it."},
    [T_ERR_NOT_GAME]   = {"L'appareil à cette adresse n'est pas un équipage Explorer 3.",
                          "The device at this address is not an Explorer 3 crew."},
    [T_ERR_NO_SOCKET]  = {"Plus de connexion disponible : relancer le jeu.",
                          "No connection left: restart the game."},
    [T_LOST]           = {"Connexion perdue.", "Connection lost."},
    [T_OLD_GAME]       = {"Jeu d'une autre version sur l'équipage : mettre les deux appareils à jour.",
                          "The crew's game is another version: update both devices."},
    [T_K_QUIT]         = {"Retour au menu", "Back to menu"},
    [T_K_BACK]         = {"Retour", "Back"},
    [T_K_SEARCH_AGAIN] = {"Chercher encore", "Search again"},
    [T_K_TYPE]         = {"Saisir l'adresse", "Type the address"},
    [T_K_CONNECT]      = {"Se connecter", "Connect"},
    [T_K_CHOOSE]       = {"Choisir", "Choose"},
    [T_K_NUMBER]       = {"Nombre, ±1", "Number, ±1"},
    [T_K_TEN]          = {"±10", "±10"},
    [T_K_RETRY]        = {"Réessayer", "Try again"},
    [T_K_CANCEL]       = {"Annuler", "Cancel"},
    [T_K_LEAVE]        = {"Changer d'équipage", "Change crew"},
};

static const char *t(int id, int lang)
{
    return TEXTS[id][lang ? 1 : 0];
}

/* --- buttons ------------------------------------------------------------------ */

enum { B_OK, B_BACK, B_KB, B_DPAD, B_LR };

#define ICON  26.0f   /* button pictures, in canvas units */

/* Keyboard keys, by language (0 French, 1 English) */
static const char *key_name(int b, int lang)
{
    switch (b) {
    case B_OK:   return lang ? "Enter" : "Entrée";
    case B_BACK: return lang ? "Esc" : "Échap";
    case B_KB:   return "Tab";
    default:     return "-  +";   /* B_LR: -10, +10 */
    }
}

static const char *lr_name(int style)
{
    return style == 1 ? "LB RB" : "L1 R1";
}

/* A key of the keyboard, or L / R of a gamepad: a light key with its name */
static float keycap(float x, float y, const char *name)
{
    float w = text_width(1.25f, name) + 12;
    draw_fill(x, y + 2, w, ICON - 4, rgb(C_DIM));
    draw_text(x + 6, y + 3, 1.25f, rgb(C_BG), 1, name);
    return w;
}

/* Width of a button's picture */
static float button_width(int b, int style, int lang)
{
    if (b == B_DPAD)
        return ICON;
    if (style == 0)
        return text_width(1.25f, key_name(b, lang)) + 12;
    if (b == B_LR)
        return text_width(1.25f, lr_name(style)) + 12;
    return ICON;
}

/* Draws a button's picture at (x, y), top-left; returns its width. */
static float button(float x, float y, int b, int style, int lang)
{
    float cx = x + ICON / 2, cy = y + ICON / 2;
    if (b == B_DPAD) {   /* the arrows, or the gamepad's cross */
        draw_fill(cx - 4, y + 1, 8, ICON - 2, rgb(C_TEXT));
        draw_fill(x + 1, cy - 4, ICON - 2, 8, rgb(C_TEXT));
        return ICON;
    }
    if (style == 0)
        return keycap(x, y, key_name(b, lang));
    if (b == B_LR)
        return keycap(x, y, lr_name(style));
    if (style == 1) {   /* Xbox: A green, B red, Y yellow */
        static const uint32_t col[3] = {0x5cb85c, 0xe0393e, 0xf6c40f};
        static const char *const let[3] = {"A", "B", "Y"};
        draw_disc(cx, cy, ICON / 2, rgb(col[b]));
        draw_text(cx - GLYPH_W * 1.25f / 2, cy - GLYPH_H * 1.25f / 2, 1.25f, rgb(0x000000), 1, let[b]);
        return ICON;
    }
    draw_disc(cx, cy, ICON / 2, rgb(0x1e1e24));   /* PlayStation: cross, circle, triangle */
    if (b == B_OK) {
        draw_curve(cx - 7, cy - 7, cx, cy, cx + 7, cy + 7, 3, rgb(0x7ca8e8));
        draw_curve(cx + 7, cy - 7, cx, cy, cx - 7, cy + 7, 3, rgb(0x7ca8e8));
    } else if (b == B_BACK) {
        draw_ring(cx, cy, 7, 7, 3, rgb(0xe8584f));
    } else {
        draw_curve(cx, cy - 8, cx - 3.5f, cy - 1, cx - 7, cy + 6, 3, rgb(0x3fc4a0));
        draw_curve(cx, cy - 8, cx + 3.5f, cy - 1, cx + 7, cy + 6, 3, rgb(0x3fc4a0));
        draw_curve(cx - 7, cy + 6, cx, cy + 6, cx + 7, cy + 6, 3, rgb(0x3fc4a0));
    }
    return ICON;
}

typedef struct {
    int button;
    int text;
} hint_t;

/* "picture text   picture text" centred along the bottom */
static void hints(const ui_t *u, const hint_t *h, int n)
{
    const float gap = 40, inner = 10;
    float total = 0;
    for (int i = 0; i < n; i++) {
        float bw = button_width(h[i].button, u->style, u->lang);
        total += bw + inner + text_width(S_BODY, t(h[i].text, u->lang)) + (i ? gap : 0);
    }
    float x = CX - total / 2, y = CANVAS_H - 60;
    draw_fill(0, y - 14, CANVAS_W, 1, rgb(C_BORDER));
    for (int i = 0; i < n; i++) {
        x += button(x, y, h[i].button, u->style, u->lang) + inner;
        draw_text(x, y + (ICON - GLYPH_H * S_BODY) / 2, S_BODY, rgb(C_DIM), 0, t(h[i].text, u->lang));
        x += text_width(S_BODY, t(h[i].text, u->lang)) + gap;
    }
}

/* --- pieces ------------------------------------------------------------------- */

static float para(float y, const char *text, uint32_t color)
{
    draw_text_centered(CX, y, TEXT_W, S_BODY, LINE_H, rgb(color), 0, text);
    return text_height(TEXT_W, S_BODY, LINE_H, text);
}

static float header(const ui_t *u, int title)
{
    float y = 4 * VH;
    char cp[64];
    int n = (int)text_to_cp437(t(T_TEAM, u->lang), cp, sizeof(cp));
    float step = GLYPH_W * S_BODY + TRACK;
    float x = CX - (n * step - TRACK) / 2;
    for (int i = 0; i < n; i++)
        draw_glyphs(x + (float)i * step, y, S_BODY, rgb(C_TEAM), cp + i, 1);
    y += GLYPH_H * S_BODY + GAP;
    draw_text_centered(CX, y, TEXT_W, S_BODY, LINE_H, rgb(C_TITLE), 1, t(title, u->lang));
    return y + text_height(TEXT_W, S_BODY, LINE_H, t(title, u->lang)) + 2 * GAP;
}

static float big(float y, const char *text)
{
    draw_text_centered(CX, y, CANVAS_W, S_BIG, 0, rgb(C_SEL), 1, text);
    return GLYPH_H * S_BIG;
}

/* Text ending with "…": the dots move */
static float waiting(const ui_t *u, float y, int id)
{
    char line[160];
    snprintf(line, sizeof(line), "%s", t(id, u->lang));
    size_t n = strlen(line);
    int dots = (int)((u->now - u->since) / 400 % 4);
    if (n >= 3 && strcmp(line + n - 3, "\xE2\x80\xA6") == 0) {
        line[n - 3] = 0;
        for (int i = 0; i < 3; i++)
            strcat(line, i < dots ? "." : " ");
    }
    return para(y, line, C_TEXT);
}

static void network_line(const ui_t *u, float y)
{
    char ip[16], line[128];
    addr_format(u->self, ip);
    snprintf(line, sizeof(line), "%s%s (%s)", t(T_SEARCH_ON, u->lang), u->net_name, ip);
    para(y, line, C_DIM);
}

#define F_PAD_X  12.0f
#define F_PAD_Y  8.0f
#define F_W      (3 * GLYPH_W * S_BIG + 2 * F_PAD_X)
#define F_H      (GLYPH_H * S_BIG + 2 * F_PAD_Y)
#define F_DOT    (GLYPH_W * S_BIG)

/* The 4 numbers of the address, the chosen one framed in yellow */
static float fields(const addr_entry_t *e, float y)
{
    float x = CX - (4 * F_W + 3 * F_DOT) / 2;
    for (int i = 0; i < 4; i++) {
        bool sel = i == e->field;
        draw_fill(x, y, F_W, F_H, rgb(C_FIELD));
        draw_frame(x, y, F_W, F_H, sel ? 4.0f : 2.0f, rgb(sel ? C_SEL : C_BORDER));
        char n[4];
        snprintf(n, sizeof(n), "%u", e->ip[i]);
        float w = (float)strlen(n) * GLYPH_W * S_BIG;
        draw_text(x + (F_W - w) / 2, y + F_PAD_Y, S_BIG, rgb(sel ? C_SEL : C_TEXT), 1, n);
        x += F_W;
        if (i < 3) {
            draw_text(x, y + F_PAD_Y, S_BIG, rgb(C_TEXT), 1, ".");
            x += F_DOT;
        }
    }
    return F_H;
}

static int reason_text(int reason)
{
    switch (reason) {
    case NET_REASON_REFUSED:   return T_ERR_REFUSED;
    case NET_REASON_NOT_GAME:  return T_ERR_NOT_GAME;
    case NET_REASON_NO_SOCKET: return T_ERR_NO_SOCKET;
    case NET_REASON_LOST:      return T_LOST;
    default:                   return T_ERR_UNREACHABLE;
    }
}

/* --- screens ------------------------------------------------------------------ */

void ui_draw(const ui_t *u)
{
    draw_fill(0, 0, CANVAS_W, CANVAS_H, rgb(C_BG));
    float y;
    char ip[16];
    switch (u->screen) {
    case UI_NO_NETWORK: {
        y = header(u, T_NET_TITLE);
        para(y, t(T_NO_NETWORK, u->lang), C_TEXT);
        hint_t h[] = {{B_BACK, T_K_QUIT}};
        hints(u, h, 1);
        break;
    }
    case UI_SEARCH: {
        y = header(u, T_SEARCH_TITLE);
        network_line(u, y);
        y += LINE_H + 3 * GAP;
        float w = 600, x = CX - w / 2;
        draw_frame(x, y, w, 28, 2, rgb(C_BORDER));
        draw_fill(x + 4, y + 4, (w - 8) * (float)u->progress / 100.0f, 20, rgb(C_TEAM));
        hint_t h[] = {{B_KB, T_K_TYPE}, {B_BACK, T_K_QUIT}};
        hints(u, h, 2);
        break;
    }
    case UI_NONE: {
        y = header(u, T_SEARCH_TITLE);
        network_line(u, y);
        y += LINE_H + 2 * GAP;
        y += para(y, t(T_NONE, u->lang), C_SEL) + GAP;
        para(y, t(T_NONE_HELP, u->lang), C_TEXT);
        hint_t h[] = {{B_OK, T_K_SEARCH_AGAIN}, {B_KB, T_K_TYPE}, {B_BACK, T_K_QUIT}};
        hints(u, h, 3);
        break;
    }
    case UI_LIST: {
        y = header(u, T_LIST_TITLE);
        for (int i = 0; i < u->nfound && i < 6; i++) {
            bool sel = i == u->sel;
            float w = 520, x = CX - w / 2, h = GLYPH_H * 2 + 16;
            draw_fill(x, y, w, h, rgb(C_FIELD));
            draw_frame(x, y, w, h, sel ? 4.0f : 2.0f, rgb(sel ? C_SEL : C_BORDER));
            char line[48];
            addr_format(u->found[i].ip, ip);
            snprintf(line, sizeof(line), "%s%s", t(T_CREW, u->lang), ip);
            draw_text_centered(CX, y + 8, w, 2.0f, 0, rgb(sel ? C_SEL : C_TEXT), 1, line);
            y += h + 10;
        }
        hint_t h[] = {{B_DPAD, T_K_CHOOSE}, {B_OK, T_K_CONNECT}, {B_KB, T_K_TYPE}, {B_BACK, T_K_QUIT}};
        hints(u, h, 4);
        break;
    }
    case UI_ADDRESS: {
        y = header(u, T_ADDR_TITLE);
        y += para(y, t(T_ADDR_HELP, u->lang), C_TEXT) + 2 * GAP;
        y += fields(&u->entry, y) + 2 * GAP;
        int st = addr_check(u->entry.ip, u->self);
        int id = st == ADDR_OTHER_NET ? T_ADDR_OTHER_NET : st == ADDR_INVALID ? T_ADDR_INVALID
               : st == ADDR_SELF ? T_ADDR_IS_SELF : st == ADDR_NET_BCAST ? T_ADDR_NET_BCAST : -1;
        if (id >= 0)
            para(y, t(id, u->lang), st == ADDR_OTHER_NET ? C_SEL : C_WARN);
        hint_t h[] = {{B_DPAD, T_K_NUMBER}, {B_LR, T_K_TEN}, {B_OK, T_K_CONNECT}, {B_BACK, T_K_BACK}};
        hints(u, h, 4);
        break;
    }
    case UI_CONNECTING: {
        y = header(u, T_CONN_TITLE);
        addr_format(u->target, ip);
        y += big(y, ip) + 2 * GAP;
        waiting(u, y, T_CONN_WAIT);
        hint_t h[] = {{B_BACK, T_K_CANCEL}};
        hints(u, h, 1);
        break;
    }
    default: {   /* UI_ERROR */
        y = header(u, T_ERR_TITLE);
        addr_format(u->target, ip);
        y += big(y, ip) + 2 * GAP;
        para(y, t(reason_text(u->reason), u->lang), C_TEXT);
        hint_t h[] = {{B_OK, T_K_RETRY}, {B_BACK, T_K_BACK}};
        hints(u, h, 2);
        break;
    }
    }
}

void ui_overlay(int lang, bool reconnecting, bool old_game)
{
    float y = CANVAS_H - 16 - GLYPH_H * S_BODY;
    if (reconnecting) {
        draw_text(16, y, S_BODY, rgb(C_STATUS), 0, tx(TX_CONNECTING, lang));
        y -= LINE_H;
    }
    if (old_game)
        draw_text(16, y, S_BODY, rgb(C_WARN), 0, t(T_OLD_GAME, lang));
}

void ui_stream_hint(int lang, int style)
{
    ui_t u;
    memset(&u, 0, sizeof(u));
    u.lang = lang;
    u.style = style;
    float w = button_width(B_BACK, style, lang) + 10 + text_width(S_BODY, t(T_K_LEAVE, lang)) + 24;
    float x = CANVAS_W - w - 8, y = CANVAS_H - ICON - 22;
    draw_fill(x, y, w, ICON + 14, (rgba_t){6, 6, 26, 220});
    x += 12;
    x += button(x, y + 7, B_BACK, style, lang) + 10;
    draw_text(x, y + 7 + (ICON - GLYPH_H * S_BODY) / 2, S_BODY, rgb(C_DIM), 0, t(T_K_LEAVE, lang));
}
