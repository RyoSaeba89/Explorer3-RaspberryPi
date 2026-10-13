/* See sound.h. */
#include "sound.h"

#include <math.h>
#include <string.h>

#define RAMP        (SOUND_RATE * 3 / 1000)     /* 3 ms */
#define CHUNK_MAX   1024                        /* rendered in pieces of this size */
#define RUMBLE_STEP (SOUND_RATE / 8000)         /* 6 output samples per noise sample */

static float sine_table[1025];
static bool sine_ready;

static void make_sine(void)
{
    if (sine_ready)
        return;
    for (int i = 0; i <= 1024; i++)
        sine_table[i] = sinf((float)i * 6.28318530718f / 1024.0f);
    sine_ready = true;
}

static inline float sine(uint32_t phase)
{
    uint32_t i = phase >> 22;                       /* 10 bits: 1024 steps */
    float f = (float)(phase & 0x3FFFFF) * (1.0f / 4194304.0f);
    return sine_table[i] + (sine_table[i + 1] - sine_table[i]) * f;
}

/* xorshift32: the noise need not be the page's, only of the same kind
 * (Math.random() is not reproducible anyway). */
static uint32_t rng = 0x2545F491u;

static float frand(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(rng >> 8) * (1.0f / 16777216.0f);   /* [0, 1) */
}

/* makeNoise() of the page: brown noise, a few crackles, cross-faded to loop
 * without a click, normalised to 0.92. */
static void make_noise(float *out)
{
    enum { N = SOUND_NOISE, F = 400 };
    static float tmp[N + F];
    float v = 0, peak = 1;
    for (int i = 0; i < N + F; i++) {
        v += (frand() * 2 - 1) * 0.15f;
        v *= 0.97f;
        if (frand() < 1.0f / 400)
            v += (frand() * 2 - 1) * 0.8f;
        tmp[i] = v;
        if (fabsf(v) > peak)
            peak = fabsf(v);
    }
    for (int i = 0; i < F; i++) {
        float a = (float)i / F;
        tmp[i] = tmp[i] * a + tmp[N + i] * (1 - a);
    }
    for (int i = 0; i < N; i++)
        out[i] = tmp[i] / peak * 0.92f;
}

void sound_init(sound_t *s)
{
    memset(s, 0, sizeof(*s));
    make_sine();
    make_noise(s->noise);
}

void sound_anchor(sound_t *s, uint32_t now_ms)
{
    s->anchor_pos = s->pos;
    s->anchor_ms = now_ms;
}

/* Local time -> sample, never before the next one to render (when() of the
 * page: max(currentTime, ...)). */
static int64_t sample_at(const sound_t *s, uint32_t local_ms)
{
    int64_t n = s->anchor_pos + (int64_t)(int32_t)(local_ms - s->anchor_ms) * SOUND_RATE / 1000;
    return n < s->pos ? s->pos : n;
}

/* Cuts what still sounds on the channel from sample at: o.stop(max(t,
 * start)) of the page. */
static void stop_channel(sound_t *s, int ch, int64_t at)
{
    for (int i = 0; i < SOUND_VOICES; i++) {
        sound_note_t *n = &s->notes[ch][i];
        if (n->end > at)
            n->end = at > n->start ? at : n->start;
    }
    if (ch == 3 && s->rumble_end > at)
        s->rumble_end = at > s->rumble_start ? at : s->rumble_start;
}

/* env() of the page: the rumble's envelope, ms since its start */
static float envelope(float ms)
{
    if (ms < 1500)
        return ms / 1500;
    if (ms < 5000)
        return 1;
    if (ms < 6500)
        return 1 - (ms - 5000) / 1500;
    return 0;
}

static void tone(sound_t *s, int64_t at, uint32_t dev_at, uint16_t freq, uint16_t dur, int ch)
{
    stop_channel(s, ch, at);
    float v = 1;
    if (ch == 4)   /* whistle: the rumble's envelope, at 70/255 */
        v = (s->rumble_known ? envelope((float)(int32_t)(dev_at - s->rumble_at)) : 1) * 70.0f / 255.0f;
    if (v <= 0 || dur == 0)
        return;
    /* a free voice, otherwise the one ending first */
    sound_note_t *slot = &s->notes[ch][0];
    for (int i = 0; i < SOUND_VOICES; i++) {
        sound_note_t *n = &s->notes[ch][i];
        if (n->end <= s->pos) {
            slot = n;
            break;
        }
        if (n->end < slot->end)
            slot = n;
    }
    slot->start = at;
    slot->end = slot->fade_end = at + (int64_t)dur * SOUND_RATE / 1000;
    slot->phase = 0;
    slot->step = (uint32_t)((double)freq * 4294967296.0 / SOUND_RATE);
    slot->gain = v;
}

void sound_message(sound_t *s, const msg_t *m, const clocksync_t *clk)
{
    if (m->kind != 'T' && m->kind != 'S' && m->kind != 'R')
        return;
    int64_t at = sample_at(s, clocksync_to_local(clk, m->at) + SOUND_LAT_MS);
    if (m->kind == 'T') {
        tone(s, at, m->at, m->freq, m->dur, m->ch);
    } else if (m->kind == 'S') {
        for (int c = 0; c < MSG_CHANNELS; c++)
            if (m->ch == MSG_CH_ALL || m->ch == c)
                stop_channel(s, c, at);
    } else {
        stop_channel(s, 3, at);   /* the rumble counts as channel 3 */
        s->rumble_known = true;
        s->rumble_at = m->at;
        s->rumble_start = at;
        s->rumble_end = at + (int64_t)SOUND_RATE * 66 / 10;   /* s.stop(t + 6.6) */
    }
}

static void render_chunk(sound_t *s, int16_t *out, int frames)
{
    static float acc[CHUNK_MAX];
    int64_t a = s->pos, b = s->pos + frames;
    memset(acc, 0, sizeof(float) * (size_t)frames);

    for (int c = 0; c < MSG_CHANNELS; c++)
        for (int k = 0; k < SOUND_VOICES; k++) {
            sound_note_t *n = &s->notes[c][k];
            if (n->end <= a || n->start >= b || n->end <= n->start)
                continue;
            int64_t from = n->start > a ? n->start : a, to = n->end < b ? n->end : b;
            for (int64_t i = from; i < to; i++) {
                /* 3 ms ramps; the end one follows the planned end, a cut
                 * (next note, S) stays sharp as o.stop() */
                int64_t rel = i - n->start, left = n->fade_end - i;
                float g = n->gain;
                if (rel < RAMP)
                    g *= (float)rel / RAMP;
                if (left < RAMP)
                    g *= (float)left / RAMP;
                acc[i - a] += g * sine(n->phase);
                n->phase += n->step;
            }
        }

    if (s->rumble_end > a && s->rumble_start < b) {
        int64_t from = s->rumble_start > a ? s->rumble_start : a;
        int64_t to = s->rumble_end < b ? s->rumble_end : b;
        for (int64_t i = from; i < to; i++) {
            int64_t rel = i - s->rumble_start;
            float ms = (float)rel * 1000.0f / SOUND_RATE;
            int64_t q = rel / RUMBLE_STEP;
            float f = (float)(rel % RUMBLE_STEP) / RUMBLE_STEP;
            float x0 = s->noise[q % SOUND_NOISE], x1 = s->noise[(q + 1) % SOUND_NOISE];
            acc[i - a] += envelope(ms) * (x0 + (x1 - x0) * f);
        }
    }

    for (int i = 0; i < frames; i++) {
        float v = acc[i] * SOUND_MASTER * 32767.0f;
        int16_t x = v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : (int16_t)v;
        out[2 * i] = x;
        out[2 * i + 1] = x;
    }
    s->pos = b;
}

void sound_render(sound_t *s, int16_t *out, int frames)
{
    while (frames > 0) {
        int n = frames < CHUNK_MAX ? frames : CHUNK_MAX;
        render_chunk(s, out, n);
        out += 2 * n;
        frames -= n;
    }
}
