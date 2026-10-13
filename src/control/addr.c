/* See addr.h. */
#include "addr.h"

#include <stdio.h>
#include <string.h>

static uint32_t u32(const uint8_t a[4])
{
    return (uint32_t)a[0] << 24 | (uint32_t)a[1] << 16 | (uint32_t)a[2] << 8 | a[3];
}

int addr_check(const uint8_t target[4], const uint8_t self[4])
{
    uint32_t t = u32(target), s = u32(self), m = 0xFFFFFF00u;
    if (t == 0 || t == 0xFFFFFFFFu || target[0] == 127 || target[0] >= 224 || target[0] == 0)
        return ADDR_INVALID;
    if (t == s)
        return ADDR_SELF;
    if ((t & m) != (s & m))
        return ADDR_OTHER_NET;
    if ((t & ~m) == 0 || (t & ~m) == ~m)
        return ADDR_NET_BCAST;
    return ADDR_OK;
}

void addr_format(const uint8_t ip[4], char out[16])
{
    snprintf(out, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

bool ip_parse(const char *s, size_t len, uint8_t out[4])
{
    int part = 0;
    size_t i = 0;
    while (part < 4) {
        size_t start = i;
        unsigned v = 0;
        while (i < len && s[i] >= '0' && s[i] <= '9' && i - start < 3)
            v = v * 10 + (unsigned)(s[i++] - '0');
        size_t digits = i - start;
        if (digits == 0 || v > 255 || (digits > 1 && s[start] == '0'))
            return false;
        out[part++] = (uint8_t)v;
        if (part < 4) {
            if (i >= len || s[i] != '.')
                return false;
            i++;
        }
    }
    return i == len;
}

void entry_init(addr_entry_t *e, const uint8_t self[4], const uint8_t *saved)
{
    memset(e, 0, sizeof(*e));
    if (saved && addr_check(saved, self) == ADDR_OK) {
        memcpy(e->ip, saved, 4);
    } else {
        memcpy(e->ip, self, 3);
        e->ip[3] = 1;
    }
    e->field = 3;
}

static void apply(addr_entry_t *e, unsigned key)
{
    switch (key) {
    case KEY_LEFT:  e->field = (e->field + 3) % 4; break;
    case KEY_RIGHT: e->field = (e->field + 1) % 4; break;
    case KEY_UP:    e->ip[e->field] = (uint8_t)(e->ip[e->field] + 1); break;
    case KEY_DOWN:  e->ip[e->field] = (uint8_t)(e->ip[e->field] - 1); break;
    case KEY_R:     e->ip[e->field] = (uint8_t)((e->ip[e->field] + 10) % 256); break;
    case KEY_L:     e->ip[e->field] = (uint8_t)((e->ip[e->field] + 246) % 256); break;
    default:        break;
    }
}

void entry_keys(addr_entry_t *e, unsigned keys, uint32_t now)
{
    unsigned pressed = keys & ~e->held;
    if (pressed) {
        /* a new press counts once and the repeat starts from it */
        for (unsigned k = KEY_LEFT; k <= KEY_R; k <<= 1)
            if (pressed & k)
                apply(e, k);
        e->next_repeat = now + ENTRY_REPEAT_DELAY;
    } else if (keys && (int32_t)(now - e->next_repeat) >= 0) {
        for (unsigned k = KEY_LEFT; k <= KEY_R; k <<= 1)
            if (keys & k)
                apply(e, k);
        e->next_repeat = now + ENTRY_REPEAT_EVERY;
    }
    e->held = keys;
}
