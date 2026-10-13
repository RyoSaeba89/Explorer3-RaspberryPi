/* Text for libogc2's 8x16 font (font_8x16.c: 256 glyphs, code page 437).
 * Pure.
 *
 * Texts are written in UTF-8; they are converted once to CP437, one byte per
 * glyph, and everything else (width, line breaks, drawing) works on that
 * form. CP437 has é, è, à, ç and É, but neither À nor È: those become the
 * letter without accent. "…" (U+2026) becomes three dots. Any other
 * character outside the table becomes '?'. */
#ifndef TEXT_H
#define TEXT_H

#include <stddef.h>

/* Converts utf8 (ended by a zero); out gets at most cap-1 glyphs and a final
 * zero. Returns the number of glyphs written. */
size_t text_to_cp437(const char *utf8, char *out, size_t cap);

typedef struct {
    int start;  /* index of the first glyph */
    int len;    /* number of glyphs, break spaces left out */
} text_line_t;

/* Splits s (CP437, len glyphs) into lines of at most max_chars glyphs, at
 * spaces when possible, in the middle of a word too long otherwise. Returns
 * the number of lines (at most max_lines; the rest is lost). */
int text_wrap(const char *s, int len, int max_chars, text_line_t *lines, int max_lines);

#endif
