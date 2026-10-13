/* Drawing on a 960 x 540 (16:9) logical canvas, which the platform scales to
 * the screen. Everything else only knows the canvas.
 *
 * draw_fill, draw_glyphs and the clipping come from the platform (sdl_draw.c
 * on the Pi, gl_draw.c on Android); the helpers of draw.c are pure and go
 * through them. */
#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CANVAS_W 960.0f
#define CANVAS_H 540.0f
#define GLYPH_W  8      /* a glyph at scale 1, in canvas units */
#define GLYPH_H  16

/* The body text's scale: 24 canvas units high, a little larger than the
 * web page's 3.6vh (19.4), for a TV seen from the sofa. */
#define DRAW_MIN_SCALE 1.5f

typedef struct {
    uint8_t r, g, b, a;
} rgba_t;

static inline rgba_t rgb(uint32_t hex)
{
    rgba_t c = {(uint8_t)(hex >> 16), (uint8_t)(hex >> 8), (uint8_t)hex, 255};
    return c;
}

/* --- from the platform --- */

/* Filled rectangle. */
void draw_fill(float x, float y, float w, float h, rgba_t c);

/* len CP437 glyphs of the 8x16 font (text.h), top-left corner at (x, y), each one
 * GLYPH_W*scale x GLYPH_H*scale. */
void draw_glyphs(float x, float y, float scale, rgba_t c, const char *cp437, int len);

/* What follows is clipped to this rectangle (the banner's overflow: hidden),
 * until draw_noclip(). */
void draw_clip(float x, float y, float w, float h);
void draw_noclip(void);

/* --- helpers (draw.c) --- */

/* Frame of thickness t, inside the rectangle. */
void draw_frame(float x, float y, float w, float h, float t, rgba_t c);

/* Filled shapes, in horizontal bands of one canvas unit: disc, ellipse,
 * elliptic ring of thickness t (centred on the ellipse rx, ry), convex
 * polygon (n vertices x, y). */
void draw_disc(float cx, float cy, float r, rgba_t c);
void draw_ellipse(float cx, float cy, float rx, float ry, rgba_t c);
void draw_ring(float cx, float cy, float rx, float ry, float t, rgba_t c);
void draw_convex(const float *xy, int n, rgba_t c);

/* Stroke of thickness t with round ends along a quadratic Bézier curve (the
 * SVG "q"). */
void draw_curve(float x0, float y0, float qx, float qy, float x1, float y1, float t, rgba_t c);

/* UTF-8 text centred on cx, split into lines max_w wide at most, line_h
 * apart. bold: thickened by one stroke. Returns the height used. */
float draw_text_centered(float cx, float y, float max_w, float scale, float line_h,
                         rgba_t c, int bold, const char *utf8);

/* Height the same text would take, nothing drawn. */
float text_height(float max_w, float scale, float line_h, const char *utf8);

/* Width of a UTF-8 text on one line. */
float text_width(float scale, const char *utf8);

/* UTF-8 text on one line, top-left corner at (x, y). */
void draw_text(float x, float y, float scale, rgba_t c, int bold, const char *utf8);

#ifdef __cplusplus
}
#endif

#endif
