/* The crew's text messages (netTask() in the game's diffusion.cpp). Pure.
 * Each one starts with the crew's clock (millis()) when it was sent:
 *
 *   P,now                            clock only, every 250 ms
 *   T,now,at,frequency,duration,ch   note to play at time at
 *   S,now,at,ch                      stop a channel (255: all)
 *   R,now,at                         take-off rumble
 *   K,now,page[,...]                 mission control's page (below)
 *   L,now,fr|en                      language
 *   B,now,y,h,text                   end screen's banner on rows y to y+h-1;
 *                                    empty text: no banner. The text may hold
 *                                    commas: everything after the 4th comma.
 *   M,now[,theme]                    maze theme (below); "M,now": the planets
 *
 * Pages (updatePanel() in the game's main.cpp):
 *
 *   K,m,0                            copy of the crew's screen
 *   K,m,R                            mission control's rules
 *   K,m,1,left,errors,               puzzle 1: Morse code
 *   K,m,2,left,errors,grid,rows,cols,cursor
 *                                    puzzle 2: picross; grid = 25 digits 0/1
 *                                    row by row, clues "3.1/1.1.1/..." (lines
 *                                    by '/', numbers by '.'), crew's cursor
 *                                    0..24 (missing before game v2.2.2)
 *   K,m,3,left,errors,               puzzle 3: Morse loudspeaker
 *   K,m,4,left,errors,grid,position,trail,wrong
 *                                    puzzle 4: 4 x 5 maze; grid = 20 chars
 *                                    E X 1..8, position 0..19, trail = 20
 *                                    digits 0/1, wrong cell 0..19 or -1
 *   K,m,C,left,errors,typed,table    coded keypad; table = letter + symbol
 *                                    number in hex (0 to b), sorted
 *
 * Maze theme (mazeThemeText() in main.cpp):
 *
 *   M,m,palette,pictures,ranks,name1,...,name8,hint
 *     palette   colours 1 to f of the pictures, 6 hex digits each
 *     pictures  16 x 16 pictures, 256 hex digits each (0 transparent)
 *     ranks     8 hex digits: the picture of cells 1 to 8
 *     names     no comma; hint: the rest (commas allowed)
 *
 * The game's 5 channels: 0 O2 alarm, 1 effects, 2 Morse, 3 rumble, 4 take-off
 * whistle. */
#ifndef MSG_H
#define MSG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MSG_CHANNELS   5
#define MSG_CH_ALL     255
#define MSG_CH_MORSE   2
#define MSG_SCREEN_H   135
#define PANEL_CELLS    9
#define PANEL_SYMBOLS  12

/* Picross (puzzle 2): 5 x 5, at most 3 numbers per row or column. */
#define PIC_N          5
#define PIC_MAX_CLUES  3

/* Maze (puzzle 4): MAZE[] of main.cpp, 4 columns x 5 rows. */
#define MAZE_W         4
#define MAZE_H         5
#define MAZE_CELLS     (MAZE_W * MAZE_H)

/* bannerText[600] in diffusion.cpp, final zero included. */
#define BANNER_MAX     599

/* Maze theme: 15 colours, 8 pictures of 16 x 16 at most. */
#define THEME_COLOURS  15
#define THEME_PICS     8
#define THEME_SIZE     16

/* Mission control's pages: the character sent by the game. */
enum {
    PAGE_SCREEN  = '0',
    PAGE_RULES   = 'R',
    PAGE_LAMP    = '1',
    PAGE_PICROSS = '2',
    PAGE_SOUND   = '3',
    PAGE_MAZE    = '4',
    PAGE_KEYPAD  = 'C',
};

enum {
    MSG_ERR_KIND   = -1,  /* unknown first letter */
    MSG_ERR_SYNTAX = -2,  /* field missing or extra, or not a number */
    MSG_ERR_RANGE  = -3,  /* number out of the protocol's bounds */
};

typedef struct {
    char    letter;  /* 'A'..'Z' */
    uint8_t sym;     /* 0..PANEL_SYMBOLS-1 */
} panel_cell_t;

typedef struct {
    uint8_t n;                   /* 1..PIC_MAX_CLUES */
    uint8_t v[PIC_MAX_CLUES];    /* 0..PIC_N; a single 0 = empty line */
} pic_clues_t;

/* A piece of the received message (valid while it is), UTF-8, not ended by
 * a zero. */
typedef struct {
    const char *p;
    size_t      n;
} msg_str_t;

typedef struct {
    char     kind;   /* 'P' 'T' 'S' 'R' 'K' 'L' 'B' 'M' */
    uint32_t now;    /* crew's clock when sent (ms) */

    /* T, S, R */
    uint32_t at;
    uint16_t freq;   /* T */
    uint16_t dur;    /* T, ms */
    uint8_t  ch;     /* T: 0..4; S: 0..4 or MSG_CH_ALL */

    /* K: page, then for the pages with a countdown (1 to 4, C) */
    char     page;       /* PAGE_... */
    uint32_t remaining;  /* ms of O2 left at time now */
    uint32_t errors;     /* the game's error count */

    /* K, page C */
    uint8_t      typed;  /* symbols already typed, 0..4 */
    int          ncells;
    panel_cell_t cells[PANEL_CELLS];

    /* K, page 2 */
    bool        pic_grid[PIC_N * PIC_N];   /* row by row */
    pic_clues_t pic_rows[PIC_N];
    pic_clues_t pic_cols[PIC_N];
    int         pic_cursor;                /* -1 (old game) or 0..24 */

    /* K, page 4 */
    char   maze[MAZE_CELLS];        /* 'E' 'X' '1'..'8' */
    int    maze_pos;                /* 0..MAZE_CELLS-1 */
    bool   maze_trail[MAZE_CELLS];
    int    maze_bad;                /* -1 or 0..MAZE_CELLS-1 */

    /* L */
    uint8_t lang;    /* 0 French, 1 English */

    /* B: text_len == 0: no banner */
    uint8_t     band_y;
    uint8_t     band_h;
    const char *text;
    size_t      text_len;

    /* M: theme false = the planets */
    bool      theme;
    uint32_t  palette[THEME_COLOURS];
    int       npics;
    msg_str_t pics;                 /* npics x 256 hex digits */
    uint8_t   ranks[THEME_PICS];    /* < npics, or 0xff: no picture */
    msg_str_t names[THEME_PICS];
    msg_str_t hint;
} msg_t;

/* s: the text message, len bytes (not necessarily ended by a zero).
 * Returns 0 or an error; a refused message must be ignored, not the
 * connection closed: one unreadable message puts nothing out of step. */
int msg_parse(const char *s, size_t len, msg_t *m);

/* true if the page has a countdown (1 to 4 and C). */
bool page_timed(char page);

/* "O2 mm:ss" as on the page: seconds rounded up. out: 12 bytes. */
void panel_o2_text(uint32_t remaining_ms, char out[12]);

/* Value of a hex digit, -1 if it is not one (lower case, as the game sends). */
int msg_hex(char c);

#endif
