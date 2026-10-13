/* IPv4 addresses, and typing the crew's address with the arrows. Pure.
 *
 * An address is 4 bytes in the order it is written (192.168.1.42 ->
 * {192, 168, 1, 42}), the byte order of an s_addr in memory on any machine:
 * memcpy goes from one to the other.
 *
 * Entry: the first 3 numbers come from this device's address, the cursor is
 * on the last one. Left/right change the number, up/down do +1/-1, L and R
 * (- and + on the keyboard) -10/+10; holding a key repeats it. Numbers wrap
 * from 255 to 0. */
#ifndef ADDR_H
#define ADDR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* What a typed address is worth, seen from this device. */
enum {
    ADDR_OK,          /* on the same network */
    ADDR_OTHER_NET,   /* valid, but not on this device's network: warned */
    ADDR_INVALID,     /* 0.0.0.0, 255.255.255.255, 127.x, 224 and above */
    ADDR_SELF,        /* this device's own address */
    ADDR_NET_BCAST,   /* network or broadcast address (host part all 0 or all 1) */
};

/* Status of a typed address. self: this device's; the network is taken as /24. */
int addr_check(const uint8_t target[4], const uint8_t self[4]);

/* "192.168.1.42", out: 16 bytes. */
void addr_format(const uint8_t ip[4], char out[16]);

/* "a.b.c.d", four numbers from 0 to 255 without leading zero nor space.
 * false if it is not exactly that. */
bool ip_parse(const char *s, size_t len, uint8_t out[4]);

/* --- entry --------------------------------------------------------------------- */

enum {
    KEY_LEFT = 1 << 0,
    KEY_RIGHT = 1 << 1,
    KEY_UP = 1 << 2,
    KEY_DOWN = 1 << 3,
    KEY_L = 1 << 4,
    KEY_R = 1 << 5,
};

#define ENTRY_REPEAT_DELAY 400   /* ms before the first repeat */
#define ENTRY_REPEAT_EVERY 60    /* ms between two repeats */

typedef struct {
    uint8_t  ip[4];
    int      field;      /* 0..3 */
    unsigned held;       /* keys held (KEY_...) */
    uint32_t next_repeat;
} addr_entry_t;

/* self: this device's address. saved: the last address used, or NULL; taken as
 * it is if it is on the same network, otherwise only the first 3 numbers
 * come from self and the last is 1. Cursor on the last number. */
void entry_init(addr_entry_t *e, const uint8_t self[4], const uint8_t *saved);

/* Keys held at time now (KEY_...). Applies new presses and repeats. */
void entry_keys(addr_entry_t *e, unsigned keys, uint32_t now);

#endif
