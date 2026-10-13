/* See panel.h. */
#include "panel.h"

#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "gamedata.h"
#include "text.h"
#include "texts.h"

/* The page's unit: 1vh = 1 % of the screen's height. */
#define VH (CANVAS_H / 100.0f)

/* Text is larger than on the page (DRAW_MIN_SCALE: 24 units instead of
 * 3.6vh = 19.4); what goes with it (keypad cells, Morse marks, loudspeaker)
 * grows as much. */
#define S_BODY    DRAW_MIN_SCALE
#define KT        (GLYPH_H * S_BODY / (3.6f * VH))

/* Colours of the page's CSS. */
#define C_BG      0x06061a
#define C_TEAM    0x50c8ff
#define C_TITLE   0xffa028
#define C_O2      0x50c8ff
#define C_O2_LOW  0xf03c32
#define C_TEXT    0xeeeeee   /* body */
#define C_MINE    0xffe146   /* .moi */
#define C_WARN    0xf03c32   /* .av */
#define C_DIM     0x8c96af   /* .dim */
#define C_CASE    0x202642
#define C_BORDER  0x465a8c
#define C_LETTER  0xffe146
#define C_SYMBOL  0xebeef5
#define C_TYPED   0x50ff78
#define C_MARK    0xffe146   /* Morse marks */
#define C_CELL    0x1e2234
#define C_LIT     0xffa028   /* lit picross cell */
#define C_CURSOR  0xffe146   /* picross cursor (td.cur) */
#define C_OK      0x46e164
#define C_TRAIL   0x8c460f
#define C_BAD     0x8c1919
#define C_HP      0x969aa0
#define C_HP_OFF  0x505a6e
#define C_HP_ON   0xffe146

/* Sizes (960 x 540 canvas), as close as can be to the page's vh. */
#define S_TITLE   DRAW_MIN_SCALE    /* 4vh */
#define S_O2      2.5f              /* 6vh x KT */
#define S_LETTER  2.5f              /* 7vh, the keypad's letters */
#define TEXT_W    880.0f
#define LINE_H    (GLYPH_H * S_BODY + 4)
#define GAP_TAB   (2.0f * VH)       /* #tab > * + * */
#define GAP_BODY  (2.2f * VH)       /* #corps > * + * */
#define TRACK     (0.25f * GLYPH_W * S_BODY)   /* letter-spacing .25em of #eq */

#define FLASH_MS   500
#define FLASH_PEAK 200              /* 40 % of 0.5 s */
#define FLASH_MAX  (2.0f * VH)

/* --- state ----------------------------------------------------------------------- */

void panel_init(panel_t *p)
{
    memset(p, 0, sizeof(*p));
    p->page = PAGE_SCREEN;
    p->maze_bad = -1;
    p->pic_cursor = -1;
}

bool panel_shown(const panel_t *p)
{
    return p->page != PAGE_SCREEN;
}

void panel_update(panel_t *p, const msg_t *k, const clocksync_t *clk, uint32_t now)
{
    p->page = k->page;
    if (!page_timed(k->page)) {
        p->err_known = false;   /* lastErr = -1 in the page */
        p->flashing = false;
        return;
    }
    p->end_local = clocksync_to_local(clk, k->now) + k->remaining;
    if (k->page == PAGE_KEYPAD) {
        p->typed = k->typed;
        p->ncells = k->ncells;
        memcpy(p->cells, k->cells, sizeof(p->cells));
    } else if (k->page == PAGE_PICROSS) {
        memcpy(p->pic_grid, k->pic_grid, sizeof(p->pic_grid));
        memcpy(p->pic_rows, k->pic_rows, sizeof(p->pic_rows));
        memcpy(p->pic_cols, k->pic_cols, sizeof(p->pic_cols));
        p->pic_cursor = k->pic_cursor;
    } else if (k->page == PAGE_MAZE) {
        memcpy(p->maze, k->maze, sizeof(p->maze));
        memcpy(p->maze_trail, k->maze_trail, sizeof(p->maze_trail));
        p->maze_pos = k->maze_pos;
        p->maze_bad = k->maze_bad;
    }
    if (p->err_known && k->errors != p->last_err) {
        p->flashing = true;
        p->flash_start = now;
    }
    p->last_err = k->errors;
    p->err_known = true;
}

static void copy_str(char *dst, size_t cap, msg_str_t s)
{
    size_t n = s.n < cap - 1 ? s.n : cap - 1;
    memcpy(dst, s.p, n);
    dst[n] = 0;
}

void panel_theme(panel_t *p, const msg_t *m)
{
    p->theme = m->theme;
    if (!m->theme)
        return;
    memcpy(p->palette, m->palette, sizeof(p->palette));
    memset(p->pics, 0, sizeof(p->pics));
    for (int k = 0; k < m->npics; k++)
        for (int i = 0; i < THEME_SIZE * THEME_SIZE; i++)
            p->pics[k][i] = (uint8_t)msg_hex(m->pics.p[k * THEME_SIZE * THEME_SIZE + i]);
    memcpy(p->ranks, m->ranks, sizeof(p->ranks));
    for (int k = 0; k < THEME_PICS; k++)
        copy_str(p->names[k], sizeof(p->names[k]), m->names[k]);
    copy_str(p->hint, sizeof(p->hint), m->hint);
}

void panel_sound(panel_t *p, const msg_t *m, const clocksync_t *clk)
{
    if (m->kind == 'T' && m->ch == MSG_CH_MORSE) {
        if (p->nbips == PANEL_BIPS) {
            memmove(p->bips, p->bips + 1, sizeof(p->bips[0]) * (PANEL_BIPS - 1));
            p->nbips--;
        }
        uint32_t t = clocksync_to_local(clk, m->at) + SOUND_LAT_MS;
        p->bips[p->nbips].start = t;
        p->bips[p->nbips].end = t + m->dur;
        p->nbips++;
    } else if (m->kind == 'S' && (m->ch == MSG_CH_MORSE || m->ch == MSG_CH_ALL)) {
        p->nbips = 0;
    }
}

static bool speaker_on(const panel_t *p, uint32_t now)
{
    for (int i = 0; i < p->nbips; i++)
        if ((int32_t)(now - p->bips[i].start) >= 0 && (int32_t)(now - p->bips[i].end) < 0)
            return true;
    return false;
}

static uint32_t remaining(const panel_t *p, uint32_t now)
{
    int32_t r = (int32_t)(p->end_local - now);
    return r > 0 ? (uint32_t)r : 0;
}

/* Thickness of the red error frame at time now (0 outside the flash). */
static float flash(const panel_t *p, uint32_t now)
{
    if (!p->flashing)
        return 0;
    uint32_t t = now - p->flash_start;
    if (t >= FLASH_MS)
        return 0;
    if (t < FLASH_PEAK)
        return FLASH_MAX * (float)t / FLASH_PEAK;
    return FLASH_MAX * (float)(FLASH_MS - t) / (FLASH_MS - FLASH_PEAK);
}

/* Clues of a line of 5 cells, as lineClues() of the game and indices() of
 * the page: a single 0 for an empty line. */
static pic_clues_t line_clues(const bool cells[PIC_N])
{
    pic_clues_t c = {0, {0}};
    int run = 0;
    for (int i = 0; i <= PIC_N; i++) {
        if (i < PIC_N && cells[i]) {
            run++;
        } else if (run) {
            if (c.n < PIC_MAX_CLUES)
                c.v[c.n++] = (uint8_t)run;
            run = 0;
        }
    }
    if (c.n == 0)
        c.n = 1;
    return c;
}

/* Picross: row (row = true) or column i matches its clues (green on the page). */
static bool pic_ok(const panel_t *p, bool row, int i)
{
    bool cells[PIC_N];
    for (int j = 0; j < PIC_N; j++)
        cells[j] = row ? p->pic_grid[i * PIC_N + j] : p->pic_grid[j * PIC_N + i];
    pic_clues_t got = line_clues(cells);
    const pic_clues_t *want = row ? &p->pic_rows[i] : &p->pic_cols[i];
    return got.n == want->n && memcmp(got.v, want->v, got.n) == 0;
}

/* --- drawing
 *
 * Each block is used twice: first to measure (paint = false, nothing drawn),
 * then to draw at its place, the page centred vertically as the #tab
 * flexbox. Each block returns its height. */

typedef struct {
    const panel_t *p;
    uint32_t       now;
    int            lang;
    bool           paint;
} ctx_t;

#define CX (CANVAS_W / 2)

static float para_text(const ctx_t *c, float y, const char *text, uint32_t color)
{
    if (c->paint)
        draw_text_centered(CX, y, TEXT_W, S_BODY, LINE_H, rgb(color), 0, text);
    return text_height(TEXT_W, S_BODY, LINE_H, text);
}

static float para(const ctx_t *c, float y, int id, uint32_t color)
{
    return para_text(c, y, tx(id, c->lang), color);
}

/* "CENTRE DE CONTRÔLE", letters spaced out (letter-spacing of #eq). */
static float team(const ctx_t *c, float y)
{
    if (c->paint) {
        char cp[64];
        int n = (int)text_to_cp437(tx(TX_TEAM, c->lang), cp, sizeof(cp));
        float step = GLYPH_W * S_BODY + TRACK;
        float x = CX - (n * step - TRACK) / 2;
        for (int i = 0; i < n; i++)
            draw_glyphs(x + (float)i * step, y, S_BODY, rgb(C_TEAM), cp + i, 1);
    }
    return GLYPH_H * S_BODY;
}

static float title(const ctx_t *c, float y, int id)
{
    if (c->paint)
        draw_text_centered(CX, y, TEXT_W, S_TITLE, GLYPH_H * S_TITLE + 4, rgb(C_TITLE), 1,
                           tx(id, c->lang));
    return text_height(TEXT_W, S_TITLE, GLYPH_H * S_TITLE + 4, tx(id, c->lang));
}

static float o2(const ctx_t *c, float y)
{
    if (c->paint) {
        char t[12];
        uint32_t left = remaining(c->p, c->now);
        panel_o2_text(left, t);
        draw_text_centered(CX, y, CANVAS_W, S_O2, 0, rgb(left < 60000 ? C_O2_LOW : C_O2), 1, t);
    }
    return GLYPH_H * S_O2;
}

/* --- page C: the coded keypad's table --------------------------------------------- */

#define CASE_PAD_X (4.0f * VH)
#define CASE_PAD_Y (0.8f * VH)
#define SYM_SIZE   (7.0f * VH)
#define SYM_GAP    (3.0f * VH)
#define GRID_GAP   (1.6f * VH)
#define CASE_LINE  3.0f
#define BOX        (3.5f * VH)
#define BOX_GAP    (0.6f * VH)
#define CASE_W     (CASE_PAD_X + GLYPH_W * S_LETTER + SYM_GAP + SYM_SIZE + CASE_PAD_X)
#define CASE_H     (2 * CASE_PAD_Y + GLYPH_H * S_LETTER)
#define GRID_W     (3 * CASE_W + 2 * GRID_GAP)

/* Symbol n in pixel art, as symbole() of the page: neighbouring pixels of a
 * row make one rectangle. */
static void draw_symbol(int n, float x, float y, float size, rgba_t c)
{
    float px = size / GAME_SYMBOL_SIZE;
    for (int r = 0; r < GAME_SYMBOL_SIZE; r++) {
        for (int col = 0; col < GAME_SYMBOL_SIZE;) {
            if (!game_symbol_pixel(n, r, col)) {
                col++;
                continue;
            }
            int e = col;
            while (e < GAME_SYMBOL_SIZE && game_symbol_pixel(n, r, e))
                e++;
            draw_fill(x + col * px, y + r * px, (float)(e - col) * px, px, c);
            col = e;
        }
    }
}

static float keypad_grid(const ctx_t *c, float y)
{
    const panel_t *p = c->p;
    int rows = (p->ncells + 2) / 3;
    if (c->paint) {
        float gx = CX - GRID_W / 2;
        for (int i = 0; i < p->ncells; i++) {
            float x = gx + (float)(i % 3) * (CASE_W + GRID_GAP);
            float cy = y + (float)(i / 3) * (CASE_H + GRID_GAP);
            draw_fill(x, cy, CASE_W, CASE_H, rgb(C_CASE));
            draw_frame(x, cy, CASE_W, CASE_H, CASE_LINE, rgb(C_BORDER));
            char letter = p->cells[i].letter;
            draw_glyphs(x + CASE_PAD_X, cy + CASE_PAD_Y, S_LETTER, rgb(C_LETTER), &letter, 1);
            draw_glyphs(x + CASE_PAD_X + S_LETTER * 0.75f, cy + CASE_PAD_Y, S_LETTER, rgb(C_LETTER),
                        &letter, 1);
            if (p->cells[i].sym < GAME_SYMBOLS)
                draw_symbol(p->cells[i].sym, x + CASE_PAD_X + GLYPH_W * S_LETTER + SYM_GAP,
                            cy + (CASE_H - SYM_SIZE) / 2, SYM_SIZE, rgb(C_SYMBOL));
        }
    }
    return rows > 0 ? rows * CASE_H + (rows - 1) * GRID_GAP : 0;
}

/* "Symboles tapés" + 4 boxes */
static float keypad_typed(const ctx_t *c, float y)
{
    float h = GLYPH_H * S_BODY;
    if (c->paint) {
        const char *label = tx(TX_PANEL_TYPED, c->lang);
        float label_w = text_width(S_BODY, label);
        float row_w = label_w + GLYPH_W * S_BODY + 4 * (BOX + 2 * BOX_GAP);
        float x = CX - row_w / 2;
        draw_text(x, y, S_BODY, rgb(C_TEXT), 0, label);
        x += label_w + GLYPH_W * S_BODY;
        float by = y + (h - BOX) / 2;
        for (int i = 0; i < 4; i++) {
            float bx = x + BOX_GAP + (float)i * (BOX + 2 * BOX_GAP);
            if (i < c->p->typed)
                draw_fill(bx, by, BOX, BOX, rgb(C_TYPED));
            else
                draw_frame(bx, by, BOX, BOX, 2.0f, rgb(C_TYPED));
        }
    }
    return h;
}

/* --- page 1: Morse code, 7 columns ------------------------------------------------ */

#define MK_H     (1.3f * VH * KT)    /* height of a dash, diameter of a dot */
#define MK_DASH  (4.0f * VH * KT)
#define MK_GAP   (0.8f * VH * KT)
#define AB_COLS  7
#define AB_COLGAP (3.5f * VH)
#define AB_ROWGAP (1.4f * VH)

static float morse_width(int i)
{
    float w = GLYPH_W * S_BODY;
    for (const char *m = game_morse(i); *m; m++)
        w += MK_GAP + (*m == '.' ? MK_H : MK_DASH);
    return w;
}

static void morse_item(int i, float x, float y)
{
    char l = (char)('A' + i);
    draw_glyphs(x, y, S_BODY, rgb(C_SYMBOL), &l, 1);
    draw_glyphs(x + S_BODY * 0.75f, y, S_BODY, rgb(C_SYMBOL), &l, 1);
    x += GLYPH_W * S_BODY;
    float my = y + (GLYPH_H * S_BODY - MK_H) / 2, r = MK_H / 2;
    for (const char *m = game_morse(i); *m; m++) {
        x += MK_GAP;
        float w = *m == '.' ? MK_H : MK_DASH;
        draw_disc(x + r, my + r, r, rgb(C_MARK));
        if (w > MK_H) {
            draw_fill(x + r, my, w - MK_H, MK_H, rgb(C_MARK));
            draw_disc(x + w - r, my + r, r, rgb(C_MARK));
        }
        x += w;
    }
}

static float alphabet(const ctx_t *c, float y)
{
    int rows = (26 + AB_COLS - 1) / AB_COLS;
    float row_h = GLYPH_H * S_BODY;
    if (c->paint) {
        float colw[AB_COLS] = {0}, total = (AB_COLS - 1) * AB_COLGAP;
        for (int i = 0; i < 26; i++) {
            float w = morse_width(i);
            if (w > colw[i % AB_COLS])
                colw[i % AB_COLS] = w;
        }
        for (int k = 0; k < AB_COLS; k++)
            total += colw[k];
        for (int i = 0; i < 26; i++) {
            float x = CX - total / 2;
            for (int k = 0; k < i % AB_COLS; k++)
                x += colw[k] + AB_COLGAP;
            morse_item(i, x, y + (float)(i / AB_COLS) * (row_h + AB_ROWGAP));
        }
    }
    return rows * row_h + (rows - 1) * AB_ROWGAP;
}

/* --- page 2: picross --------------------------------------------------------------- */

#define PIC_TD     (8.0f * VH)
#define PIC_LINE   2.0f
#define PIC_CUR    (1.0f * VH)   /* box-shadow inset 1vh of td.cur */
#define PIC_PAD_X  (1.6f * VH)
#define PIC_PAD_Y  (0.6f * VH)
#define PIC_LH     (GLYPH_H * S_BODY * 1.15f)

static float picross(const ctx_t *c, float y)
{
    const panel_t *p = c->p;
    int head_lines = 1, row_chars = 1;
    for (int i = 0; i < PIC_N; i++) {
        if (p->pic_cols[i].n > head_lines)
            head_lines = p->pic_cols[i].n;
        if (2 * p->pic_rows[i].n - 1 > row_chars)
            row_chars = 2 * p->pic_rows[i].n - 1;
    }
    float head_h = 2 * PIC_PAD_Y + head_lines * PIC_LH;
    float side_w = 2 * PIC_PAD_X + row_chars * GLYPH_W * S_BODY;
    float h = head_h + PIC_N * PIC_TD;
    if (!c->paint)
        return h;

    float x0 = CX - (side_w + PIC_N * PIC_TD) / 2, gx = x0 + side_w, gy = y + head_h;
    float gw = GLYPH_W * S_BODY, gh = GLYPH_H * S_BODY;
    for (int i = 0; i < PIC_N; i++) {
        /* column: digits stacked, at the bottom */
        const pic_clues_t *cl = &p->pic_cols[i];
        rgba_t col = rgb(pic_ok(p, false, i) ? C_OK : C_SYMBOL);
        for (int k = 0; k < cl->n; k++) {
            char d = (char)('0' + cl->v[k]);
            float ty = gy - PIC_PAD_Y - (float)(cl->n - k) * PIC_LH + (PIC_LH - gh) / 2;
            float tx_ = gx + (float)i * PIC_TD + (PIC_TD - gw) / 2;
            draw_glyphs(tx_, ty, S_BODY, col, &d, 1);
            draw_glyphs(tx_ + S_BODY * 0.75f, ty, S_BODY, col, &d, 1);
        }
        /* row: digits separated by spaces, on the right */
        cl = &p->pic_rows[i];
        col = rgb(pic_ok(p, true, i) ? C_OK : C_SYMBOL);
        char s[2 * PIC_MAX_CLUES];
        int n = 0;
        for (int k = 0; k < cl->n; k++) {
            if (k)
                s[n++] = ' ';
            s[n++] = (char)('0' + cl->v[k]);
        }
        float tx_ = gx - PIC_PAD_X - (float)n * gw, ty = gy + (float)i * PIC_TD + (PIC_TD - gh) / 2;
        draw_glyphs(tx_, ty, S_BODY, col, s, n);
        draw_glyphs(tx_ + S_BODY * 0.75f, ty, S_BODY, col, s, n);
    }
    for (int r = 0; r < PIC_N; r++)
        for (int k = 0; k < PIC_N; k++) {
            float x = gx + (float)k * PIC_TD, cy = gy + (float)r * PIC_TD;
            draw_fill(x, cy, PIC_TD, PIC_TD, rgb(p->pic_grid[r * PIC_N + k] ? C_LIT : C_CELL));
            draw_frame(x, cy, PIC_TD, PIC_TD, PIC_LINE, rgb(C_BORDER));
        }
    if (p->pic_cursor >= 0) {
        /* yellow frame inside the border, as the crew's cursor */
        float x = gx + (float)(p->pic_cursor % PIC_N) * PIC_TD + PIC_LINE;
        float cy = gy + (float)(p->pic_cursor / PIC_N) * PIC_TD + PIC_LINE;
        draw_frame(x, cy, PIC_TD - 2 * PIC_LINE, PIC_TD - 2 * PIC_LINE, PIC_CUR, rgb(C_CURSOR));
    }
    return h;
}

/* --- page 3: loudspeaker (SVG #hp of the page, 64 x 48) ------------------------------ */

#define HP_W (16.0f * VH * KT)
#define HP_H (12.0f * VH * KT)

static float speaker(const ctx_t *c, float y)
{
    if (c->paint) {
        float s = HP_W / 64, x0 = CX - HP_W / 2;
#define PX(v) (x0 + (v) * s)
#define PY(v) (y + (v) * s)
        draw_fill(PX(6), PY(17), 10 * s, 14 * s, rgb(C_HP));
        float cone[] = {PX(16), PY(17), PX(30), PY(5), PX(30), PY(43), PX(16), PY(31)};
        draw_convex(cone, 4, rgb(C_HP));
        rgba_t w = rgb(speaker_on(c->p, c->now) ? C_HP_ON : C_HP_OFF);
        draw_curve(PX(38), PY(17), PX(43), PY(24), PX(38), PY(31), 4 * s, w);
        draw_curve(PX(45), PY(11), PX(55), PY(24), PX(45), PY(37), 4 * s, w);
        draw_curve(PX(52), PY(5), PX(67), PY(24), PX(52), PY(43), 4 * s, w);
#undef PX
#undef PY
    }
    return HP_H;
}

/* --- page 4: the maze, 4 x 5 ------------------------------------------------------- */

#define MZ_W     (22.0f * VH)     /* least width of a cell */
#define MZ_H     (9.0f * VH)      /* least height of a cell */
#define MZ_LINE  2.0f
#define MZ_ICON  (4.5f * VH)      /* planet, picture, hatch: .pl */
#define MZ_ICGAP (0.4f * VH)
#define MZ_PAD   2.0f
#define MZ_SIDE  (1.6f * VH)      /* room on each side of a name */
#define AS_PX    (8.0f * VH / GAME_ASTRO_H)   /* .as: 6vh x 8vh */
#define AE_PX    (6.0f * VH / GAME_ASTRO_H)   /* .ae: 4.5vh x 6vh */

/* PCOUL of the page: Mercury to Neptune. */
static const uint32_t PLANET_COLOR[8] = {
    0xa0a0a8, 0xe8c878, 0x3c78dc, 0xd2502d, 0xd8a878, 0xe6cc8c, 0x8cdce6, 0x3c5ad2,
};

/* ACOUL of the page: the astronaut's colours. */
static uint32_t astro_color(char c)
{
    switch (c) {
    case 'W': return 0xffffff;
    case 'B': return 0x2858dc;
    case 'C': return 0x50c8ff;
    case 'G': return 0x969aa0;
    case 'D': return 0x50505a;
    case 'O': return 0xffa028;
    default:  return 0;
    }
}

static void astro(float x, float y, float px)
{
    for (int r = 0; r < GAME_ASTRO_H; r++) {
        const char *row = game_astro_row(r);
        for (int k = 0; k < GAME_ASTRO_W && row[k]; k++)
            if (astro_color(row[k]))
                draw_fill(x + (float)k * px, y + (float)r * px, px, px, rgb(astro_color(row[k])));
    }
}

/* planete() of the page: SVG 20 x 20 at (x, y), size wide. */
static void planet(int k, float x, float y, float size)
{
    float s = size / 20;
#define PX(v) (x + (v) * s)
#define PY(v) (y + (v) * s)
    draw_disc(PX(10), PY(10), (k == 5 ? 5.5f : 7.0f) * s, rgb(PLANET_COLOR[k]));
    if (k == 2) {   /* the Earth's continent */
        float land[] = {PX(6), PY(8), PX(8.5f), PY(6.4f), PX(11), PY(8), PX(13.2f), PY(10.4f),
                        PX(14), PY(12), PX(10.5f), PY(13.2f), PX(8), PY(11)};
        draw_convex(land, 7, rgb(0x46b450));
    }
    if (k == 4) {   /* Jupiter's bands */
        draw_fill(PX(3.5f), PY(7.25f), 13 * s, 1.5f * s, rgb(0xa0643c));
        draw_fill(PX(3.5f), PY(11.25f), 13 * s, 1.5f * s, rgb(0xa0643c));
    }
    if (k == 5)     /* Saturn's ring */
        draw_ring(PX(10), PY(10), 9.5f * s, 3 * s, 1.5f * s, rgb(0xc8b070));
#undef PX
#undef PY
}

/* dessin() of the page: the theme's 16 x 16 picture of cell k + 1, runs of
 * one colour in a row drawn as one rectangle. */
static void picture(const panel_t *p, int k, float x, float y, float size)
{
    int r = p->ranks[k];
    if (r >= THEME_PICS)
        return;
    const uint8_t *d = p->pics[r];
    float px = size / THEME_SIZE;
    for (int row = 0; row < THEME_SIZE; row++)
        for (int col = 0; col < THEME_SIZE;) {
            uint8_t v = d[row * THEME_SIZE + col];
            int e = col + 1;
            while (e < THEME_SIZE && d[row * THEME_SIZE + e] == v)
                e++;
            if (v)
                draw_fill(x + col * px, y + row * px, (float)(e - col) * px, px,
                          rgb(p->palette[(v - 1) % THEME_COLOURS]));
            col = e;
        }
}

/* TRAPPE of the page: the exit */
static void hatch(float x, float y, float size)
{
    float s = size / 20;
    draw_fill(x + 2 * s, y + 2 * s, 16 * s, 16 * s, rgb(0x145a28));
    draw_frame(x + 1.25f * s, y + 1.25f * s, 17.5f * s, 17.5f * s, 1.5f * s, rgb(C_OK));
    float tri[] = {x + 7 * s, y + 5 * s, x + 14 * s, y + 10 * s, x + 7 * s, y + 15 * s};
    draw_convex(tri, 3, rgb(C_OK));
}

/* Name shown in a cell */
static const char *cell_name(const ctx_t *c, char v)
{
    if (v == 'E')
        return tx(TX_ENTRANCE, c->lang);
    if (v == 'X')
        return tx(TX_EXIT, c->lang);
    int k = v - '1';
    return c->p->theme ? c->p->names[k] : tx(TX_PLANET_1 + k, c->lang);
}

/* Width of a column: the widest name fits, on one line (white-space: nowrap). */
static float maze_cell_w(const ctx_t *c)
{
    float w = MZ_W;
    for (int i = 0; i < MAZE_CELLS; i++) {
        float need = text_width(S_BODY, cell_name(c, c->p->maze[i])) + 2 * MZ_SIDE;
        if (need > w)
            w = need;
    }
    float most = (CANVAS_W - 16) / MAZE_W;
    return w < most ? w : most;
}

/* Height of a cell's content (without .as, laid over it). */
static float maze_content(const panel_t *p, int i)
{
    float text = GLYPH_H * S_BODY;
    if (p->maze[i] == 'E')
        return (i == p->maze_pos ? GAME_ASTRO_H * AE_PX + MZ_ICGAP : 0) + text;
    return MZ_ICON + MZ_ICGAP + text;
}

static float maze_row_h(const panel_t *p, int r)
{
    float h = MZ_H;
    for (int k = 0; k < MAZE_W; k++) {
        float need = maze_content(p, r * MAZE_W + k) + 2 * MZ_PAD;
        if (need > h)
            h = need;
    }
    return h;
}

static float maze(const ctx_t *c, float y)
{
    const panel_t *p = c->p;
    float h = 0;
    for (int r = 0; r < MAZE_H; r++)
        h += maze_row_h(p, r);
    if (!c->paint)
        return h;

    float cw = maze_cell_w(c);
    float x0 = CX - MAZE_W * cw / 2, cy = y;
    for (int r = 0; r < MAZE_H; r++) {
        float rh = maze_row_h(p, r);
        for (int k = 0; k < MAZE_W; k++) {
            int i = r * MAZE_W + k;
            char v = p->maze[i];
            float x = x0 + (float)k * cw;
            uint32_t bg = i == p->maze_bad ? C_BAD : p->maze_trail[i] ? C_TRAIL : C_CELL;
            draw_fill(x, cy, cw, rh, rgb(bg));
            draw_frame(x, cy, cw, rh, MZ_LINE, rgb(C_BORDER));

            /* content centred vertically (vertical-align: middle) */
            float ty = cy + (rh - maze_content(p, i)) / 2;
            uint32_t color = C_TEXT;
            if (v == 'E') {
                if (i == p->maze_pos) {
                    astro(x + (cw - GAME_ASTRO_W * AE_PX) / 2, ty, AE_PX);
                    ty += GAME_ASTRO_H * AE_PX + MZ_ICGAP;
                }
                color = C_DIM;
            } else if (v == 'X') {
                hatch(x + (cw - MZ_ICON) / 2, ty, MZ_ICON);
                ty += MZ_ICON + MZ_ICGAP;
                color = C_OK;
            } else {
                if (p->theme)
                    picture(p, v - '1', x + (cw - MZ_ICON) / 2, ty, MZ_ICON);
                else
                    planet(v - '1', x + (cw - MZ_ICON) / 2, ty, MZ_ICON);
                ty += MZ_ICON + MZ_ICGAP;
            }
            const char *name = cell_name(c, v);
            draw_text(x + (cw - text_width(S_BODY, name)) / 2, ty, S_BODY, rgb(color), 0, name);
            if (i == p->maze_pos && v != 'E')
                astro(x + 1.2f * VH, cy + 0.5f * VH, AS_PX);
        }
        cy += rh;
    }
    return h;
}

/* The maze's hint: the theme's, or the planets' */
static float maze_hint(const ctx_t *c, float y)
{
    return para_text(c, y, c->p->theme ? c->p->hint : tx(TX_P4_HINT, c->lang), C_MINE);
}

/* --- pages ------------------------------------------------------------------------- */

typedef float (*block_fn)(const ctx_t *c, float y);

/* An element of #corps: a drawn block or a paragraph. */
typedef struct {
    block_fn fn;
    int      text;    /* otherwise: the text's id */
    uint32_t color;
} item_t;

#define BLOCK(f)    {f, -1, 0}
#define PARA(id, c) {NULL, id, c}

static float page_layout(const ctx_t *c, float y, int title_id, const item_t *items, int n)
{
    float y0 = y;
    y += team(c, y) + GAP_TAB;
    y += title(c, y, title_id) + GAP_TAB;
    if (page_timed(c->p->page))
        y += o2(c, y) + GAP_TAB;
    for (int i = 0; i < n; i++) {
        if (i)
            y += GAP_BODY;
        y += items[i].fn ? items[i].fn(c, y) : para(c, y, items[i].text, items[i].color);
    }
    return y - y0;
}

static void page_draw(const panel_t *p, uint32_t now, int lang, int title_id, const item_t *items, int n)
{
    ctx_t c = {p, now, lang, false};
    float h = page_layout(&c, 0, title_id, items, n);
    c.paint = true;
    page_layout(&c, (CANVAS_H - h) / 2, title_id, items, n);
}

void panel_draw(const panel_t *p, uint32_t now, int lang)
{
    static const item_t rules[] = {
        PARA(TX_RULES_1, C_MINE), PARA(TX_RULES_2, C_TEXT), PARA(TX_RULES_3, C_WARN),
        PARA(TX_RULES_4, C_DIM),
    };
    static const item_t lamp[] = {PARA(TX_P1_TEXT, C_TEXT), PARA(TX_P1_NOTE, C_DIM), BLOCK(alphabet)};
    static const item_t pic[] = {BLOCK(picross), PARA(TX_P2_TEXT, C_TEXT)};
    static const item_t sound[] = {BLOCK(speaker), PARA(TX_P3_TEXT, C_TEXT), PARA(TX_P3_NOTE, C_DIM)};
    const item_t laby[] = {
        BLOCK(maze), PARA(p->theme ? TX_P4_THEME_TEXT : TX_P4_TEXT, C_TEXT), BLOCK(maze_hint),
        PARA(TX_P4_WRONG, C_DIM),
    };
    static const item_t keypad[] = {BLOCK(keypad_grid), BLOCK(keypad_typed), PARA(TX_PANEL_HINT, C_DIM)};

    if (!panel_shown(p))
        return;
    draw_fill(0, 0, CANVAS_W, CANVAS_H, rgb(C_BG));
    switch (p->page) {
    case PAGE_RULES:   page_draw(p, now, lang, TX_RULES_TITLE, rules, 4); break;
    case PAGE_LAMP:    page_draw(p, now, lang, TX_P1_TITLE, lamp, 3); break;
    case PAGE_PICROSS: page_draw(p, now, lang, TX_P2_TITLE, pic, 2); break;
    case PAGE_SOUND:   page_draw(p, now, lang, TX_P3_TITLE, sound, 3); break;
    case PAGE_MAZE:    page_draw(p, now, lang, TX_P4_TITLE, laby, 4); break;
    default:           page_draw(p, now, lang, TX_PANEL_TITLE, keypad, 3); break;
    }

    /* error: red inner frame that thickens, then fades */
    float f = flash(p, now);
    if (f > 0.5f)
        draw_frame(0, 0, CANVAS_W, CANVAS_H, f, rgb(C_WARN));
}
