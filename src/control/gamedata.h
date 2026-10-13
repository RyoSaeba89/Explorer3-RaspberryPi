/* The game's own data, for mission control's pages, always those of the
 * game version built with this program: on the Pi, read from the game
 * compiled into the same program (src/bridge.inc); on Android, from its
 * tables copied at build time (gamedata.c). Both check the sizes below. */
#ifndef GAMEDATA_H
#define GAMEDATA_H

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_ASTRO_W  9     /* ASTRO[] of main.cpp */
#define GAME_ASTRO_H  12
#define GAME_SYMBOLS  12    /* SYMBOLS[] of main.cpp: the coded keypad */
#define GAME_SYMBOL_SIZE 12

/* MORSE[] of main.cpp: '.' and '-' of letter 'A' + letter. */
const char *game_morse(int letter);

/* ASTRO[] of main.cpp, row 0..GAME_ASTRO_H-1: W white, B visor, C glint,
 * G grey, D dark grey, O orange, '.' transparent. */
const char *game_astro_row(int row);

/* Pixel (row, col) of symbol n is lit. */
int game_symbol_pixel(int n, int row, int col);

#ifdef __cplusplus
}
#endif

#endif
