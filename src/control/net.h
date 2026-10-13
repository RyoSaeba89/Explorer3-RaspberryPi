/* Mission control (Explorer3-Android's, unchanged): the connection to the
 * crew, and the search for the crew on the local network (automatic search,
 * typed address as a fallback).
 *
 * The crew is a Cardputer or a Pi (stream on port 81, page on 80) or an
 * Android device (8081 and 8080: Android forbids ports below 1024 without
 * root). */
#ifndef NET_H
#define NET_H
#include <stdbool.h>
#include <stdint.h>

#define PORT_CARDPUTER 81
#define PORT_ANDROID   8081
#define LINK_CLOSED    (-1)

/* --- one connection, non-blocking (GL thread) --- */

/* Starts connecting; 0, or -errno. */
int link_start(const uint8_t ip[4], int port);
/* 1 connected, 0 still trying, -errno failed. */
int link_poll(void);
/* Bytes read (0: nothing yet), LINK_CLOSED, or -errno. */
int link_read(uint8_t *buf, int cap);
/* 0, or -errno. */
int link_write(const void *buf, int len);
void link_close(void);

/* --- search, on its own thread --- */

#define FINDER_MAX 16

typedef struct {
    uint8_t ip[4];
    int     port;   /* PORT_CARDPUTER or PORT_ANDROID */
} found_t;

enum { FINDER_IDLE, FINDER_RUNNING, FINDER_DONE };

/* Looks for the crew on the 254 addresses of self's /24 network, or only at
 * ip when one is given (typed address). Ignored while a search runs. */
void finder_start(const uint8_t self[4], const uint8_t *only);
/* State, and the devices found so far (sorted by address). */
int finder_poll(found_t *out, int *count);
/* 0..100 */
int finder_progress(void);

#endif
