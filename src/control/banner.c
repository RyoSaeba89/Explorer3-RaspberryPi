/* See banner.h. */
#include "banner.h"

#include <math.h>
#include <string.h>

#include "draw.h"
#include "text.h"

#define C_BAND    0x101429   /* the game's C_PANEL */
#define C_EDGE    0x42598c   /* C_BORDER */
#define C_INFO    0xefeff7   /* C_TEXT */
#define ROW       (CANVAS_H / MSG_SCREEN_H)   /* one crew row on the canvas */
#define S_INFO    2.5f       /* 68 % of the banner, as on the page */

void banner_init(banner_t *b)
{
    memset(b, 0, sizeof(*b));
}

void banner_update(banner_t *b, const msg_t *m, uint32_t now)
{
    char utf8[BANNER_MAX + 1];
    memcpy(utf8, m->text, m->text_len);
    utf8[m->text_len] = 0;
    b->len = (int)text_to_cp437(utf8, b->text, sizeof(b->text));
    b->on = b->len > 0;
    b->y = m->band_y;
    b->h = m->band_h;
    b->start = now;
}

void banner_draw(const banner_t *b, uint32_t now)
{
    if (!b->on)
        return;
    float y = (float)b->y * ROW, h = (float)b->h * ROW, w = CANVAS_W, edge = ROW;
    draw_fill(0, y, w, h, rgb(C_BAND));
    draw_fill(0, y, w, edge, rgb(C_EDGE));
    draw_fill(0, y + h - edge, w, edge, rgb(C_EDGE));

    /* x = w - (t x 45 px/s) mod (w + length), as placerBande() */
    float gw = GLYPH_W * S_INFO, gh = GLYPH_H * S_INFO;
    float loop = w + (float)b->len * gw + w / 2;
    float moved = (float)((uint64_t)(now - b->start) * BANNER_SPEED) * ROW / 1000.0f;
    float x = w - (moved - loop * (float)(uint64_t)(moved / loop));

    /* only the visible glyphs, clipped to the banner's edges */
    int first = x < 0 ? (int)(-x / gw) : 0;
    int last = (int)ceilf((w - x) / gw);   /* glyphs starting before the right edge */
    if (last > b->len)
        last = b->len;
    if (first >= last)
        return;
    draw_clip(0, y + edge, w, h - 2 * edge);
    draw_glyphs(x + (float)first * gw, y + (h - gh) / 2, S_INFO, rgb(C_INFO), b->text + first,
                last - first);
    draw_noclip();
}
