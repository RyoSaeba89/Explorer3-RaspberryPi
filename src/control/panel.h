/* Mission control's pages, as the web page shows them (panel(), majPage(),
 * horloge(), labyrinthe(), theme() and the #tab CSS of the game's
 * diffusion.cpp). Pure. panel.c holds the state; panel_draw.c draws it
 * through draw.h where the page's layout is used (Android, Pi).
 *
 * They replace the screen copy during the rules, the 4 puzzles and the coded
 * keypad (message K, see msg.h). For the puzzles and the keypad, the O2
 * countdown is worked out again at each frame from the O2 left and the
 * clock, to run smoothly between two messages (every 500 ms); it turns red
 * under a minute. When the error count changes the page flashes red for
 * 0.5 s (the page's "err" animation); not on the first message, nor after
 * the screen copy or the rules (lastErr = -1 in the page).
 *
 * The maze shows the planets, or the theme of the game in progress (message
 * M: pictures, names and hint). */
#ifndef PANEL_H
#define PANEL_H

#include <stdbool.h>
#include <stdint.h>

#include "clocksync.h"
#include "msg.h"

#define PANEL_BIPS 8       /* Morse beeps kept for the loudspeaker (page 3) */
#define PANEL_FLASH_MS 500  /* red error flash (the page's "err" animation) */

typedef struct {
    uint32_t start, end;   /* local times */
} panel_bip_t;

typedef struct {
    char         page;          /* PAGE_SCREEN until a K arrives */
    uint32_t     end_local;     /* local time the O2 runs out */
    bool         err_known;     /* false after a page without countdown */
    uint32_t     last_err;
    bool         flashing;
    uint32_t     flash_start;

    /* page C */
    uint8_t      typed;
    int          ncells;
    panel_cell_t cells[PANEL_CELLS];

    /* page 2 */
    bool         pic_grid[PIC_N * PIC_N];
    pic_clues_t  pic_rows[PIC_N];
    pic_clues_t  pic_cols[PIC_N];
    int          pic_cursor;   /* -1: none */

    /* page 4 */
    char         maze[MAZE_CELLS];
    int          maze_pos;
    bool         maze_trail[MAZE_CELLS];
    int          maze_bad;

    /* page 4, maze theme (message M); theme false: the planets */
    bool         theme;
    uint32_t     palette[THEME_COLOURS];
    uint8_t      pics[THEME_PICS][THEME_SIZE * THEME_SIZE];   /* 0 transparent, else colour + 1 */
    uint8_t      ranks[THEME_PICS];                           /* picture of cell 1..8, 0xff: none */
    char         names[THEME_PICS][96];                       /* UTF-8 */
    char         hint[512];

    /* page 3: the Morse channel's notes, followed even without sound */
    panel_bip_t  bips[PANEL_BIPS];
    int          nbips;
} panel_t;

/* No page, the planets. */
void panel_init(panel_t *p);

/* true if a page replaces the screen copy. */
bool panel_shown(const panel_t *p);

/* A message K arrived at local time now (clk already updated). */
void panel_update(panel_t *p, const msg_t *k, const clocksync_t *clk, uint32_t now);

/* A message M: the maze's theme, or the planets. */
void panel_theme(panel_t *p, const msg_t *m);

/* Messages T and S: the Morse channel's beeps light page 3's loudspeaker
 * (bip() of the page); a stop of channel 2 or of all clears them. */
void panel_sound(panel_t *p, const msg_t *m, const clocksync_t *clk);

/* O2 left at local time now (ms). */
uint32_t panel_remaining(const panel_t *p, uint32_t now);

/* Age of the red error flash (0 to PANEL_FLASH_MS - 1 ms), -1 outside it. */
int panel_flash_age(const panel_t *p, uint32_t now);

/* Page 3: the loudspeaker lights while a Morse beep sounds. */
bool panel_speaker_on(const panel_t *p, uint32_t now);

/* Clues of a line of 5 cells, as lineClues() of the game and indices() of
 * the page: a single 0 for an empty line. */
pic_clues_t panel_line_clues(const bool cells[PIC_N]);

/* Picross: row (row = true) or column i matches its clues (green). */
bool panel_pic_ok(const panel_t *p, bool row, int i);

/* panel_draw.c (devices drawing the web page's layout): draws the page over the whole canvas. lang: 0 French, 1 English.
 * Nothing for the screen copy. */
void panel_draw(const panel_t *p, uint32_t now, int lang);

#endif
