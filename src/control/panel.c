/* See panel.h. The pages' state, from the messages; panel_draw.c draws
 * them on the devices that show the web page's layout. */
#include "panel.h"

#include <string.h>

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

bool panel_speaker_on(const panel_t *p, uint32_t now)
{
    for (int i = 0; i < p->nbips; i++)
        if ((int32_t)(now - p->bips[i].start) >= 0 && (int32_t)(now - p->bips[i].end) < 0)
            return true;
    return false;
}

uint32_t panel_remaining(const panel_t *p, uint32_t now)
{
    int32_t r = (int32_t)(p->end_local - now);
    return r > 0 ? (uint32_t)r : 0;
}

int panel_flash_age(const panel_t *p, uint32_t now)
{
    if (!p->flashing)
        return -1;
    uint32_t t = now - p->flash_start;
    return t < PANEL_FLASH_MS ? (int)t : -1;
}

/* Clues of a line of 5 cells, as lineClues() of the game and indices() of
 * the page: a single 0 for an empty line. */
pic_clues_t panel_line_clues(const bool cells[PIC_N])
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
bool panel_pic_ok(const panel_t *p, bool row, int i)
{
    bool cells[PIC_N];
    for (int j = 0; j < PIC_N; j++)
        cells[j] = row ? p->pic_grid[i * PIC_N + j] : p->pic_grid[j * PIC_N + i];
    pic_clues_t got = panel_line_clues(cells);
    const pic_clues_t *want = row ? &p->pic_rows[i] : &p->pic_cols[i];
    return got.n == want->n && memcmp(got.v, want->v, got.n) == 0;
}
