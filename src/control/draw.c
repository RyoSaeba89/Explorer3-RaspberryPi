/* See draw.h. */
#include "draw.h"

#include <math.h>

#include "text.h"

#define MAX_TEXT  512
#define MAX_LINES 10

void draw_frame(float x, float y, float w, float h, float t, rgba_t c)
{
    draw_fill(x, y, w, t, c);
    draw_fill(x, y + h - t, w, t, c);
    draw_fill(x, y + t, t, h - 2 * t, c);
    draw_fill(x + w - t, y + t, t, h - 2 * t, c);
}

static int layout(float max_w, float scale, const char *utf8, char *cp, text_line_t *lines)
{
    int len = (int)text_to_cp437(utf8, cp, MAX_TEXT);
    int max_chars = (int)(max_w / (GLYPH_W * scale));
    return text_wrap(cp, len, max_chars, lines, MAX_LINES);
}

float draw_text_centered(float cx, float y, float max_w, float scale, float line_h,
                         rgba_t c, int bold, const char *utf8)
{
    char cp[MAX_TEXT];
    text_line_t lines[MAX_LINES];
    int n = layout(max_w, scale, utf8, cp, lines);
    for (int i = 0; i < n; i++) {
        float w = (float)lines[i].len * GLYPH_W * scale;
        float x = cx - w / 2;
        float ly = y + (float)i * line_h;
        draw_glyphs(x, ly, scale, c, cp + lines[i].start, lines[i].len);
        if (bold)
            draw_glyphs(x + scale * 0.75f, ly, scale, c, cp + lines[i].start, lines[i].len);
    }
    return n > 0 ? (float)(n - 1) * line_h + GLYPH_H * scale : 0;
}

float text_height(float max_w, float scale, float line_h, const char *utf8)
{
    char cp[MAX_TEXT];
    text_line_t lines[MAX_LINES];
    int n = layout(max_w, scale, utf8, cp, lines);
    return n > 0 ? (float)(n - 1) * line_h + GLYPH_H * scale : 0;
}

float text_width(float scale, const char *utf8)
{
    char cp[MAX_TEXT];
    return (float)text_to_cp437(utf8, cp, MAX_TEXT) * GLYPH_W * scale;
}

void draw_text(float x, float y, float scale, rgba_t c, int bold, const char *utf8)
{
    char cp[MAX_TEXT];
    int n = (int)text_to_cp437(utf8, cp, MAX_TEXT);
    draw_glyphs(x, y, scale, c, cp, n);
    if (bold)
        draw_glyphs(x + scale * 0.75f, y, scale, c, cp, n);
}

/* --- shapes, in bands of one canvas unit ----------------------------------------- */

void draw_ellipse(float cx, float cy, float rx, float ry, rgba_t c)
{
    if (rx <= 0 || ry <= 0)
        return;
    for (float y = cy - ry; y < cy + ry; y += 1.0f) {
        float h = y + 1.0f < cy + ry ? 1.0f : cy + ry - y;
        float m = (y + h / 2 - cy) / ry;          /* middle of the band */
        float k = 1 - m * m;
        if (k <= 0)
            continue;
        float w = rx * sqrtf(k);
        draw_fill(cx - w, y, 2 * w, h, c);
    }
}

void draw_disc(float cx, float cy, float r, rgba_t c)
{
    draw_ellipse(cx, cy, r, r, c);
}

void draw_ring(float cx, float cy, float rx, float ry, float t, rgba_t c)
{
    float ox = rx + t / 2, oy = ry + t / 2, ix = rx - t / 2, iy = ry - t / 2;
    for (float y = cy - oy; y < cy + oy; y += 1.0f) {
        float h = y + 1.0f < cy + oy ? 1.0f : cy + oy - y;
        float d = y + h / 2 - cy;
        float ko = 1 - (d / oy) * (d / oy);
        if (ko <= 0)
            continue;
        float wo = ox * sqrtf(ko);
        float ki = iy > 0 ? 1 - (d / iy) * (d / iy) : 0;
        if (ki <= 0 || ix <= 0) {
            draw_fill(cx - wo, y, 2 * wo, h, c);
            continue;
        }
        float wi = ix * sqrtf(ki);
        draw_fill(cx - wo, y, wo - wi, h, c);
        draw_fill(cx + wi, y, wo - wi, h, c);
    }
}

void draw_convex(const float *xy, int n, rgba_t c)
{
    float top = xy[1], bottom = xy[1];
    for (int i = 1; i < n; i++) {
        top = fminf(top, xy[2 * i + 1]);
        bottom = fmaxf(bottom, xy[2 * i + 1]);
    }
    for (float y = top; y < bottom; y += 1.0f) {
        float h = y + 1.0f < bottom ? 1.0f : bottom - y;
        float m = y + h / 2, lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < n; i++) {
            float x0 = xy[2 * i], y0 = xy[2 * i + 1];
            float x1 = xy[2 * ((i + 1) % n)], y1 = xy[2 * ((i + 1) % n) + 1];
            if ((m < y0) == (m < y1))
                continue;                          /* the edge misses the band */
            float x = x0 + (m - y0) * (x1 - x0) / (y1 - y0);
            lo = fminf(lo, x);
            hi = fmaxf(hi, x);
        }
        if (hi > lo)
            draw_fill(lo, y, hi - lo, h, c);
    }
}

void draw_curve(float x0, float y0, float qx, float qy, float x1, float y1, float t, rgba_t c)
{
    float len = hypotf(qx - x0, qy - y0) + hypotf(x1 - qx, y1 - qy);
    int steps = (int)(len / (t * 0.35f)) + 1;      /* discs close enough */
    for (int i = 0; i <= steps; i++) {
        float u = (float)i / (float)steps, v = 1 - u;
        draw_disc(v * v * x0 + 2 * u * v * qx + u * u * x1, v * v * y0 + 2 * u * v * qy + u * u * y1,
                  t / 2, c);
    }
}
