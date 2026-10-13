/* SDL_Renderer implementation of draw.h (filled rectangles,
 * 8x16 font, clipping) plus the copy of a 240 x 135 screen, for mission
 * control and for the game itself. The 960 x 540 canvas is scaled to fit the
 * window, aspect kept, sharp at any resolution (as on Android). Display
 * thread only. */
#ifndef SDL_DRAW_H
#define SDL_DRAW_H
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Once: the renderer, and the font texture. */
void sd_init(SDL_Renderer *r);

/* Start a frame on a w x h window: black, canvas placed and scaled. */
void sd_begin(int w, int h);

/* A 240 x 135 screen (RGB565 values) over the whole canvas, enlarged a whole
 * number of times without smoothing, then smoothed to the final size: big
 * pixels stay square and sharp. changed: new pixels. */
void sd_screen(const uint16_t *pixels, bool changed);

/* The renderer lost its textures' contents (SDL_RENDER_TARGETS_RESET):
 * the enlarged screen is made again. */
void sd_reset(void);

/* Finish the frame (presents it). */
void sd_end(void);

#ifdef __cplusplus
}
#endif

#endif
