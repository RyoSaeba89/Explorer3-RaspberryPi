/* See text.h. */
#include "text.h"

#include <stdint.h>

/* Unicode -> CP437 for what the French and English texts use. */
static const struct {
    uint16_t u;
    uint8_t  c;
} MAP[] = {
    {0x00C7, 0x80}, {0x00FC, 0x81}, {0x00E9, 0x82}, {0x00E2, 0x83}, {0x00E4, 0x84},
    {0x00E0, 0x85}, {0x00E7, 0x87}, {0x00EA, 0x88}, {0x00EB, 0x89}, {0x00E8, 0x8A},
    {0x00EF, 0x8B}, {0x00EE, 0x8C}, {0x00C4, 0x8E}, {0x00C9, 0x90}, {0x00F4, 0x93},
    {0x00F6, 0x94}, {0x00FB, 0x96}, {0x00F9, 0x97}, {0x00D6, 0x99}, {0x00DC, 0x9A},
    {0x00F1, 0xA4}, {0x00D1, 0xA5}, {0x00AB, 0xAE}, {0x00BB, 0xAF}, {0x00B0, 0xF8},
    {0x00B7, 0xFA}, {0x00B1, 0xF1}, {0x00A0, ' '},
    /* no CP437 equivalent: the plain letter */
    {0x00C0, 'A'}, {0x00C2, 'A'}, {0x00C8, 'E'}, {0x00CA, 'E'}, {0x00CB, 'E'},
    {0x00CE, 'I'}, {0x00CF, 'I'}, {0x00D4, 'O'}, {0x00D9, 'U'}, {0x00DB, 'U'},
    {0x0152, 'O'}, {0x0153, 'o'},
    /* typographic punctuation */
    {0x2019, '\''}, {0x2018, '\''}, {0x201C, '"'}, {0x201D, '"'}, {0x2013, '-'},
    {0x2014, '-'}, {0x2212, '-'}, {0x202F, ' '}, {0x2009, ' '},
};

size_t text_to_cp437(const char *utf8, char *out, size_t cap)
{
    const uint8_t *p = (const uint8_t *)utf8;
    size_t n = 0;
    if (cap == 0)
        return 0;
    while (*p && n + 1 < cap) {
        uint32_t u;
        int extra;
        if (*p < 0x80) {
            u = *p++;
            extra = 0;
        } else if ((*p & 0xE0) == 0xC0) {
            u = *p++ & 0x1F;
            extra = 1;
        } else if ((*p & 0xF0) == 0xE0) {
            u = *p++ & 0x0F;
            extra = 2;
        } else if ((*p & 0xF8) == 0xF0) {
            u = *p++ & 0x07;
            extra = 3;
        } else {
            p++;          /* stray continuation byte */
            out[n++] = '?';
            continue;
        }
        int ok = 1;
        for (int i = 0; i < extra; i++) {
            if ((*p & 0xC0) != 0x80) {
                ok = 0;   /* cut sequence: the next byte is not eaten */
                break;
            }
            u = u << 6 | (*p++ & 0x3F);
        }
        if (!ok) {
            out[n++] = '?';
            continue;
        }
        if (u < 0x80) {
            out[n++] = (char)u;
        } else if (u == 0x2026) {
            for (int i = 0; i < 3 && n + 1 < cap; i++)
                out[n++] = '.';
        } else {
            char c = '?';
            for (size_t i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++)
                if (MAP[i].u == u) {
                    c = (char)MAP[i].c;
                    break;
                }
            out[n++] = c;
        }
    }
    out[n] = 0;
    return n;
}

/* A space where the line may break: not before a lone double punctuation
 * (" :", " !", " ?", " ;" of French), which would start the next line alone.
 * The game does the same (wrapped() in main.cpp). */
static int breakable(const char *s, int len, int k)
{
    if (s[k] != ' ')
        return 0;
    if (k + 1 < len && (s[k + 1] == ':' || s[k + 1] == '!' || s[k + 1] == '?' || s[k + 1] == ';') &&
        (k + 2 >= len || s[k + 2] == ' '))
        return 0;
    return 1;
}

int text_wrap(const char *s, int len, int max_chars, text_line_t *lines, int max_lines)
{
    int count = 0, i = 0;
    if (max_chars < 1)
        max_chars = 1;
    while (i < len && count < max_lines) {
        while (i < len && s[i] == ' ')   /* no space at the start of a line */
            i++;
        if (i >= len)
            break;
        int end = i + max_chars;
        if (end >= len) {
            end = len;
        } else {
            int cut = end;
            while (cut > i && !breakable(s, len, cut))
                cut--;
            if (cut > i)
                end = cut;     /* break at the space; otherwise cut the word */
        }
        int e = end;
        while (e > i && s[e - 1] == ' ')
            e--;
        lines[count].start = i;
        lines[count].len = e - i;
        count++;
        i = end;
    }
    return count;
}
