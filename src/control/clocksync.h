/* The crew's clock seen from this device. Pure.
 *
 * As the web page does: for each message, note local time - crew time, and
 * keep the smallest of the last 40, the measure least delayed by the network.
 * A crew time then becomes a local time by a mere addition; sounds play at
 * that time + SOUND_LAT_MS.
 *
 * All times are 32-bit milliseconds that wrap (the crew's millis()):
 * compare by signed difference, never with < directly. */
#ifndef CLOCKSYNC_H
#define CLOCKSYNC_H

#include <stdbool.h>
#include <stdint.h>

#define CLOCK_WINDOW 40

/* The page's LAT: a note due at crew time a sounds at local time
 * a + offset + SOUND_LAT_MS. */
#define SOUND_LAT_MS 150

typedef struct {
    uint32_t offs[CLOCK_WINDOW];
    int      count;
    int      next;
    uint32_t off;   /* current minimum: local - crew */
} clocksync_t;

/* At each new connection: the crew may have restarted. */
void clocksync_reset(clocksync_t *c);

/* A message sent at crew time dev_ms arrived at local time local_ms. */
void clocksync_sample(clocksync_t *c, uint32_t local_ms, uint32_t dev_ms);

/* Crew time -> local time. */
uint32_t clocksync_to_local(const clocksync_t *c, uint32_t dev_ms);

#endif
