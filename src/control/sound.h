/* The game's sound, synthesised as the web page does with Web Audio
 * (tone(), stop(), rumble(), makeNoise() in diffusion.cpp). Pure: returns
 * samples, main.cpp plays them.
 *
 *   - Each note (message T) sounds at time at + offset + SOUND_LAT_MS: sine,
 *     3 ms ramps at the start and the end; a new note cuts those of its
 *     channel still sounding. Arriving late: it starts at once, whole
 *     (when() of the page).
 *   - S: cuts a channel (255: all) at the given time, sharply.
 *   - R: take-off rumble, 1 s of brown noise at 8 kHz looped, volume 0 -> 1
 *     in 1.5 s, full until 5 s, 0 at 6.5 s; it counts as channel 3. The
 *     whistle (channel 4) follows the same envelope at 70/255.
 *   - Master volume 0.3.
 *
 * Samples are numbered from the start; sound_anchor() tells at which local
 * time the next sample rendered sounds. */
#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>
#include <stdint.h>

#include "clocksync.h"
#include "msg.h"

#define SOUND_RATE     48000   /* samples per second (audio::RATE) */
#define SOUND_VOICES   6       /* notes per channel at the same time */
#define SOUND_NOISE    8000    /* 1 s of noise at 8 kHz, as the page */
#define SOUND_MASTER   0.3f

typedef struct {
    int64_t  start, end;   /* samples; end <= start: free */
    int64_t  fade_end;     /* planned end (end ramp); end may come sooner */
    uint32_t phase, step;  /* 32-bit phase accumulator */
    float    gain;
} sound_note_t;

typedef struct {
    int64_t      pos;          /* next sample to render */
    int64_t      anchor_pos;   /* sample that sounds at anchor_ms */
    uint32_t     anchor_ms;
    sound_note_t notes[MSG_CHANNELS][SOUND_VOICES];
    int64_t      rumble_start, rumble_end;
    bool         rumble_known;  /* the page's rumbleAt: crew time of the last R */
    uint32_t     rumble_at;
    float        noise[SOUND_NOISE];
} sound_t;

/* Once: silence, the rumble's noise computed. */
void sound_init(sound_t *s);

/* The next sample rendered will sound at local time now_ms. */
void sound_anchor(sound_t *s, uint32_t now_ms);

/* Messages T, S and R (the others are ignored). */
void sound_message(sound_t *s, const msg_t *m, const clocksync_t *clk);

/* frames stereo samples (left, right), native 16 bits. */
void sound_render(sound_t *s, int16_t *out, int frames);

#endif
