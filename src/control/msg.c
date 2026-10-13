/* See msg.h. */
#include "msg.h"

#include <stdio.h>
#include <string.h>

/* Split into comma-separated fields, nothing changed. The longest message
 * split this way is page 4: 9 fields. */
#define MAX_FIELDS 10

typedef msg_str_t field_t;

static int split(const char *s, size_t len, field_t *f)
{
    int count = 0;
    size_t start = 0;
    for (size_t i = 0; i <= len; i++) {
        if (i == len || s[i] == ',') {
            if (count == MAX_FIELDS)
                return -1;
            f[count].p = s + start;
            f[count].n = i - start;
            count++;
            start = i + 1;
        }
    }
    return count;
}

/* Unsigned decimal integer, digits only, bounded by max. */
static bool num(const field_t *f, uint32_t max, uint32_t *out)
{
    if (f->n == 0 || f->n > 10)
        return false;
    uint64_t v = 0;
    for (size_t i = 0; i < f->n; i++) {
        char c = f->p[i];
        if (c < '0' || c > '9')
            return false;
        v = v * 10 + (uint64_t)(c - '0');
    }
    if (v > max)
        return false;
    *out = (uint32_t)v;
    return true;
}

int msg_hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Exactly n digits 0/1. */
static bool bits(const field_t *f, size_t n, bool *out)
{
    if (f->n != n)
        return false;
    for (size_t i = 0; i < n; i++) {
        if (f->p[i] != '0' && f->p[i] != '1')
            return false;
        out[i] = f->p[i] == '1';
    }
    return true;
}

/* Picross clues: 5 groups separated by '/', 1 to 3 digits 0..5 separated by
 * '.' in each group (cluesText() in the game). */
static int clues(const field_t *f, pic_clues_t out[PIC_N])
{
    size_t i = 0;
    for (int g = 0; g < PIC_N; g++) {
        out[g].n = 0;
        for (;;) {
            if (i >= f->n || f->p[i] < '0' || f->p[i] > '9')
                return MSG_ERR_SYNTAX;
            int v = f->p[i++] - '0';
            if (v > PIC_N)
                return MSG_ERR_RANGE;
            if (out[g].n == PIC_MAX_CLUES)
                return MSG_ERR_RANGE;
            out[g].v[out[g].n++] = (uint8_t)v;
            if (i < f->n && f->p[i] == '.') {
                i++;
                continue;
            }
            break;
        }
        if (g < PIC_N - 1) {
            if (i >= f->n || f->p[i] != '/')
                return MSG_ERR_SYNTAX;
            i++;
        }
    }
    return i == f->n ? 0 : MSG_ERR_SYNTAX;
}

static int parse_keypad(const field_t *f, msg_t *m)
{
    uint32_t v;
    if (!num(&f[5], UINT32_MAX, &v))
        return MSG_ERR_SYNTAX;
    if (v > 4)
        return MSG_ERR_RANGE;
    m->typed = (uint8_t)v;
    if (f[6].n % 2 != 0 || f[6].n > 2 * PANEL_CELLS)
        return MSG_ERR_SYNTAX;
    m->ncells = (int)(f[6].n / 2);
    for (int i = 0; i < m->ncells; i++) {
        char c = f[6].p[2 * i];
        int h = msg_hex(f[6].p[2 * i + 1]);
        if (c < 'A' || c > 'Z' || h < 0)
            return MSG_ERR_SYNTAX;
        if (h >= PANEL_SYMBOLS)
            return MSG_ERR_RANGE;
        m->cells[i].letter = c;
        m->cells[i].sym = (uint8_t)h;
    }
    return 0;
}

static int parse_maze(const field_t *f, msg_t *m)
{
    uint32_t v;
    if (f[5].n != MAZE_CELLS)
        return MSG_ERR_SYNTAX;
    for (int i = 0; i < MAZE_CELLS; i++) {
        char c = f[5].p[i];
        if (c != 'E' && c != 'X' && (c < '1' || c > '8'))
            return MSG_ERR_SYNTAX;
        m->maze[i] = c;
    }
    if (!num(&f[6], UINT32_MAX, &v))
        return MSG_ERR_SYNTAX;
    if (v >= MAZE_CELLS)
        return MSG_ERR_RANGE;
    m->maze_pos = (int)v;
    if (!bits(&f[7], MAZE_CELLS, m->maze_trail))
        return MSG_ERR_SYNTAX;
    if (f[8].n == 2 && f[8].p[0] == '-' && f[8].p[1] == '1') {
        m->maze_bad = -1;
    } else {
        if (!num(&f[8], UINT32_MAX, &v))
            return MSG_ERR_SYNTAX;
        if (v >= MAZE_CELLS)
            return MSG_ERR_RANGE;
        m->maze_bad = (int)v;
    }
    return 0;
}

static int parse_panel(const field_t *f, int n, msg_t *m)
{
    if (f[2].n != 1)
        return MSG_ERR_SYNTAX;
    m->page = f[2].p[0];
    if (m->page == PAGE_SCREEN || m->page == PAGE_RULES)
        return n == 3 ? 0 : MSG_ERR_SYNTAX;
    if (!page_timed(m->page))
        return MSG_ERR_RANGE;   /* unknown page: a newer game */
    if (n < 6 || !num(&f[3], UINT32_MAX, &m->remaining) || !num(&f[4], UINT32_MAX, &m->errors))
        return MSG_ERR_SYNTAX;
    switch (m->page) {
    case PAGE_LAMP:
    case PAGE_SOUND:
        return n == 6 && f[5].n == 0 ? 0 : MSG_ERR_SYNTAX;
    case PAGE_PICROSS: {
        /* no cursor (9th field) before game v2.2.2 */
        if ((n != 8 && n != 9) || !bits(&f[5], PIC_N * PIC_N, m->pic_grid))
            return MSG_ERR_SYNTAX;
        m->pic_cursor = -1;
        if (n == 9) {
            uint32_t v;
            if (!num(&f[8], UINT32_MAX, &v))
                return MSG_ERR_SYNTAX;
            if (v >= PIC_N * PIC_N)
                return MSG_ERR_RANGE;
            m->pic_cursor = (int)v;
        }
        int r = clues(&f[6], m->pic_rows);
        return r ? r : clues(&f[7], m->pic_cols);
    }
    case PAGE_MAZE:
        return n == 9 ? parse_maze(f, m) : MSG_ERR_SYNTAX;
    default:   /* PAGE_KEYPAD */
        return n == 7 ? parse_keypad(f, m) : MSG_ERR_SYNTAX;
    }
}

/* The first k fields after "X,", the rest in *rest. false if there are
 * fewer than k + 1 fields. */
static bool leading(const char *s, size_t len, field_t *f, int k, field_t *rest)
{
    size_t start = 2;
    for (int i = 0; i < k; i++) {
        size_t e = start;
        while (e < len && s[e] != ',')
            e++;
        if (e >= len)
            return false;
        f[i].p = s + start;
        f[i].n = e - start;
        start = e + 1;
    }
    rest->p = s + start;
    rest->n = len - start;
    return true;
}

/* B,now,y,h,text: the text is everything after the 4th comma. */
static int parse_banner(const char *s, size_t len, msg_t *m)
{
    field_t f[3], text;
    if (!leading(s, len, f, 3, &text))
        return MSG_ERR_SYNTAX;
    uint32_t y, h;
    if (!num(&f[0], UINT32_MAX, &m->now) || !num(&f[1], UINT32_MAX, &y) || !num(&f[2], UINT32_MAX, &h))
        return MSG_ERR_SYNTAX;
    m->text = text.p;
    m->text_len = text.n;
    if (y + (uint64_t)h > MSG_SCREEN_H || m->text_len > BANNER_MAX || (m->text_len && h == 0))
        return MSG_ERR_RANGE;
    m->band_y = (uint8_t)y;
    m->band_h = (uint8_t)h;
    return 0;
}

/* M,now  or  M,now,palette,pictures,ranks,name1..name8,hint (theme() of the
 * page: the hint is everything after the 13th comma). */
static int parse_theme(const char *s, size_t len, msg_t *m)
{
    field_t f[12], hint;
    if (!leading(s, len, f, 12, &hint)) {   /* as the page: not all fields, the planets */
        const char *comma = memchr(s + 2, ',', len - 2);
        field_t now = {s + 2, comma ? (size_t)(comma - (s + 2)) : len - 2};
        return num(&now, UINT32_MAX, &m->now) ? 0 : MSG_ERR_SYNTAX;
    }
    if (!num(&f[0], UINT32_MAX, &m->now))
        return MSG_ERR_SYNTAX;

    /* palette: 6 hex digits per colour */
    if (f[1].n % 6 != 0 || f[1].n > 6 * THEME_COLOURS)
        return MSG_ERR_SYNTAX;
    for (size_t i = 0; i < f[1].n; i++)
        if (msg_hex(f[1].p[i]) < 0)
            return MSG_ERR_SYNTAX;
    for (size_t c = 0; c < f[1].n / 6; c++) {
        uint32_t v = 0;
        for (int i = 0; i < 6; i++)
            v = v << 4 | (uint32_t)msg_hex(f[1].p[6 * c + i]);
        m->palette[c] = v;
    }

    /* pictures: 256 hex digits each */
    size_t pic = THEME_SIZE * THEME_SIZE;
    if (f[2].n % pic != 0 || f[2].n > pic * THEME_PICS)
        return MSG_ERR_SYNTAX;
    for (size_t i = 0; i < f[2].n; i++)
        if (msg_hex(f[2].p[i]) < 0)
            return MSG_ERR_SYNTAX;
    m->npics = (int)(f[2].n / pic);
    m->pics = f[2];

    /* ranks: one hex digit per cell 1..8; a missing picture draws nothing */
    if (f[3].n != THEME_PICS)
        return MSG_ERR_SYNTAX;
    for (int i = 0; i < THEME_PICS; i++) {
        int r = msg_hex(f[3].p[i]);
        if (r < 0)
            return MSG_ERR_SYNTAX;
        m->ranks[i] = r < m->npics ? (uint8_t)r : 0xff;
    }
    for (int i = 0; i < THEME_PICS; i++)
        m->names[i] = f[4 + i];
    m->hint = hint;
    m->theme = true;
    return 0;
}

int msg_parse(const char *s, size_t len, msg_t *m)
{
    field_t f[MAX_FIELDS];
    memset(m, 0, sizeof(*m));
    if (len < 1 || s[0] == 0 || strchr("PTSRKLBM", s[0]) == NULL)
        return MSG_ERR_KIND;
    if (len < 2 || s[1] != ',')
        return MSG_ERR_SYNTAX;
    m->kind = s[0];
    if (m->kind == 'B')
        return parse_banner(s, len, m);
    if (m->kind == 'M')
        return parse_theme(s, len, m);

    int n = split(s, len, f);
    if (n < 2)
        return MSG_ERR_SYNTAX;
    if (!num(&f[1], UINT32_MAX, &m->now))
        return MSG_ERR_SYNTAX;

    uint32_t v;
    switch (m->kind) {
    case 'P':
        return n == 2 ? 0 : MSG_ERR_SYNTAX;

    case 'T':
        if (n != 6 || !num(&f[2], UINT32_MAX, &m->at))
            return MSG_ERR_SYNTAX;
        if (!num(&f[3], UINT16_MAX, &v))
            return MSG_ERR_SYNTAX;
        m->freq = (uint16_t)v;
        if (!num(&f[4], UINT16_MAX, &v))
            return MSG_ERR_SYNTAX;
        m->dur = (uint16_t)v;
        if (!num(&f[5], UINT32_MAX, &v))
            return MSG_ERR_SYNTAX;
        if (v >= MSG_CHANNELS)
            return MSG_ERR_RANGE;
        m->ch = (uint8_t)v;
        return 0;

    case 'S':
        if (n != 4 || !num(&f[2], UINT32_MAX, &m->at) || !num(&f[3], UINT32_MAX, &v))
            return MSG_ERR_SYNTAX;
        if (v >= MSG_CHANNELS && v != MSG_CH_ALL)
            return MSG_ERR_RANGE;
        m->ch = (uint8_t)v;
        return 0;

    case 'R':
        if (n != 3 || !num(&f[2], UINT32_MAX, &m->at))
            return MSG_ERR_SYNTAX;
        return 0;

    case 'L':
        if (n != 3 || f[2].n != 2)
            return MSG_ERR_SYNTAX;
        if (memcmp(f[2].p, "fr", 2) == 0)
            m->lang = 0;
        else if (memcmp(f[2].p, "en", 2) == 0)
            m->lang = 1;
        else
            return MSG_ERR_RANGE;
        return 0;

    default:   /* 'K' */
        if (n < 3)
            return MSG_ERR_SYNTAX;
        return parse_panel(f, n, m);
    }
}

bool page_timed(char page)
{
    return page == PAGE_LAMP || page == PAGE_PICROSS || page == PAGE_SOUND ||
           page == PAGE_MAZE || page == PAGE_KEYPAD;
}

void panel_o2_text(uint32_t remaining_ms, char out[12])
{
    uint32_t s = remaining_ms / 1000 + (remaining_ms % 1000 != 0);
    snprintf(out, 12, "O2 %02u:%02u", (unsigned)(s / 60 % 100), (unsigned)(s % 60));
}
