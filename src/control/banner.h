/* The end screen's banner (message B). Pure: drawn through draw.h.
 *
 * On the victory screen the crew scrolls a text over rows y to y+h-1. Those
 * rows changed at every picture and the stream fell behind: the crew sends
 * the text once and stops sending those rows while it is shown. As the web
 * page does (banniere(), placerBande()), the banner is laid over the screen
 * copy with the same colours at the same place, and its text scrolled here
 * at 45 crew pixels per second from the message's arrival. An empty text
 * removes it (the rows come back in the stream). */
#ifndef BANNER_H
#define BANNER_H

#include <stdbool.h>
#include <stdint.h>

#include "msg.h"

#define BANNER_SPEED 45   /* crew pixels per second (WIN_INFO_SPEED) */

typedef struct {
    bool     on;
    int      y, h;                    /* rows of the crew's screen */
    char     text[BANNER_MAX + 1];    /* CP437 */
    int      len;
    uint32_t start;                   /* local time of arrival */
} banner_t;

void banner_init(banner_t *b);

/* A message B arrived at local time now. */
void banner_update(banner_t *b, const msg_t *m, uint32_t now);

/* Draws the banner over the screen copy (whole canvas = 240 x 135). */
void banner_draw(const banner_t *b, uint32_t now);

#endif
