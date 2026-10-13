/* See clocksync.h. */
#include "clocksync.h"

void clocksync_reset(clocksync_t *c)
{
    c->count = 0;
    c->next = 0;
    c->off = 0;
}

void clocksync_sample(clocksync_t *c, uint32_t local_ms, uint32_t dev_ms)
{
    c->offs[c->next] = local_ms - dev_ms;
    c->next = (c->next + 1) % CLOCK_WINDOW;
    if (c->count < CLOCK_WINDOW)
        c->count++;

    /* minimum of the window, by signed difference */
    uint32_t m = c->offs[0];
    for (int i = 1; i < c->count; i++)
        if ((int32_t)(c->offs[i] - m) < 0)
            m = c->offs[i];
    c->off = m;
}

uint32_t clocksync_to_local(const clocksync_t *c, uint32_t dev_ms)
{
    return dev_ms + c->off;
}
