/* See sdl_draw.h. Canvas units are turned into window pixels here; SDL
 * batches the rectangles and glyphs itself. */
#include "sdl_draw.h"

#include <string.h>

#include "draw.h"
#include "rows.h"

extern unsigned char console_font_8x16[];  /* font_8x16.c: CP437, libogc2's */

#define FONT_UP 4                 /* font texture enlarged 4 times: smoothing only softens edges */
#define FONT_TW (16 * 8 * FONT_UP)
#define FONT_TH (16 * 16 * FONT_UP)

static SDL_Renderer *ren;
static SDL_Texture *fontTex, *screenTex, *largeTex;
static int surfW, surfH;          /* window */
static float vx, vy, vw, vh;      /* canvas on the window, in pixels */
static float s = 1;               /* pixels per canvas unit */
static int k = 1;                 /* whole enlargement of the screen copy */
static int largeK;                /* k of the current largeTex (0: none) */
static int maxTex = 2048;

void sd_init(SDL_Renderer *r)
{
    ren = r;
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(ren, &info) == 0 && info.max_texture_width > 0)
        maxTex = info.max_texture_width;

    /* Font: 16 x 16 glyphs of 8 x 16 points, each point FONT_UP x FONT_UP,
     * white with the glyph as alpha (coloured by SDL_SetTextureColorMod) */
    static uint32_t pixels[FONT_TW * FONT_TH];
    memset(pixels, 0, sizeof(pixels));
    for (int g = 0; g < 256; g++) {
        int gx = (g % 16) * 8, gy = (g / 16) * 16;
        for (int row = 0; row < 16; row++) {
            unsigned char bits = console_font_8x16[g * 16 + row];
            for (int c = 0; c < 8; c++) {
                if (!(bits & (0x80 >> c)))
                    continue;
                for (int dy = 0; dy < FONT_UP; dy++)
                    for (int dx = 0; dx < FONT_UP; dx++)
                        pixels[((gy + row) * FONT_UP + dy) * FONT_TW + (gx + c) * FONT_UP + dx] = 0xFFFFFFFFu;
            }
        }
    }
    for (int i = 0; i < FONT_TW * FONT_TH; i++)
        if (!pixels[i])
            pixels[i] = 0x00FFFFFFu;   /* transparent white: no dark fringe when smoothed */
    fontTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, FONT_TW, FONT_TH);
    SDL_UpdateTexture(fontTex, NULL, pixels, FONT_TW * 4);
    SDL_SetTextureBlendMode(fontTex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(fontTex, SDL_ScaleModeLinear);

    screenTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, SCR_W, SCR_H);
    SDL_SetTextureScaleMode(screenTex, SDL_ScaleModeNearest);
    largeTex = NULL;
    largeK = 0;
}

/* --- draw.h ------------------------------------------------------------------ */

void draw_fill(float x, float y, float w, float h, rgba_t c)
{
    if (w <= 0 || h <= 0)
        return;
    SDL_FRect r = {vx + x * s, vy + y * s, w * s, h * s};
    SDL_SetRenderDrawBlendMode(ren, c.a == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_RenderFillRectF(ren, &r);
}

void draw_glyphs(float x, float y, float scale, rgba_t c, const char *cp437, int len)
{
    SDL_SetTextureColorMod(fontTex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(fontTex, c.a);
    float gw = GLYPH_W * scale * s, gh = GLYPH_H * scale * s;
    float px = vx + x * s, py = vy + y * s;
    for (int i = 0; i < len; i++, px += gw) {
        unsigned char g = (unsigned char)cp437[i];
        if (g == ' ')
            continue;
        SDL_Rect src = {(g % 16) * 8 * FONT_UP, (g / 16) * 16 * FONT_UP, 8 * FONT_UP, 16 * FONT_UP};
        SDL_FRect dst = {px, py, gw, gh};
        SDL_RenderCopyF(ren, fontTex, &src, &dst);
    }
}

void draw_clip(float x, float y, float w, float h)
{
    SDL_Rect r = {(int)(vx + x * s), (int)(vy + y * s), (int)(w * s + 0.5f), (int)(h * s + 0.5f)};
    SDL_RenderSetClipRect(ren, &r);
}

void draw_noclip(void)
{
    SDL_RenderSetClipRect(ren, NULL);
}

/* --- frame ------------------------------------------------------------------- */

void sd_begin(int w, int h)
{
    if (w != surfW || h != surfH) {
        surfW = w;
        surfH = h;
        s = (float)w / CANVAS_W < (float)h / CANVAS_H ? (float)w / CANVAS_W : (float)h / CANVAS_H;
        vw = (float)(int)(CANVAS_W * s + 0.5f);
        vh = (float)(int)(CANVAS_H * s + 0.5f);
        vx = (float)(int)((w - vw) / 2);
        vy = (float)(int)((h - vh) / 2);
        k = (int)vw / SCR_W > 0 ? (int)vw / SCR_W : 1;
        while (k > 1 && (SCR_W * k > maxTex || SCR_H * k > maxTex))
            k--;
    }
    SDL_SetRenderTarget(ren, NULL);
    SDL_RenderSetClipRect(ren, NULL);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
}

void sd_screen(const uint16_t *pixels, bool changed)
{
    if (largeK != k) {   /* (re)make the enlarged texture for this size */
        if (largeTex)
            SDL_DestroyTexture(largeTex);
        largeTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, SCR_W * k, SCR_H * k);
        if (largeTex)
            SDL_SetTextureScaleMode(largeTex, SDL_ScaleModeLinear);
        largeK = k;
        changed = true;
    }
    if (changed)
        SDL_UpdateTexture(screenTex, NULL, pixels, SCR_W * 2);
    SDL_FRect dst = {vx, vy, vw, vh};
    if (largeTex) {
        if (changed && SDL_SetRenderTarget(ren, largeTex) == 0) {
            SDL_RenderCopy(ren, screenTex, NULL, NULL);
            SDL_SetRenderTarget(ren, NULL);
        }
        SDL_RenderCopyF(ren, largeTex, NULL, &dst);
    } else {
        SDL_RenderCopyF(ren, screenTex, NULL, &dst);
    }
}

void sd_reset(void)
{
    largeK = 0;
}

void sd_end(void)
{
    SDL_RenderSetClipRect(ren, NULL);
    SDL_RenderPresent(ren);
}
