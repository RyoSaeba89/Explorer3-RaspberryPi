// On-screen keyboard for the gamepad, drawn over the game's screen. Its keys
// depend on what the screen expects (bridge::Entry). Game thread only.
#pragma once
#include <cstdint>

#include "bridge.h"

namespace osk {

constexpr int ERASE = -1;    // erases, or closes the keyboard when there is nothing to erase
constexpr int OK = -2;
constexpr int SHIFT = -3;    // text: lower case <-> capitals
constexpr int SYMBOLS = -4;  // text: letters <-> symbols

bool isOpen();
bridge::Entry mode();
void open(bridge::Entry mode);  // the cursor always starts on the arrow key (user's choice)
void close();
void move(int dx, int dy);
int selected();  // character, ERASE, OK, SHIFT or SYMBOLS
void press(int key);  // SHIFT or SYMBOLS: another page of characters

// Draws the keyboard over the picture (RGB565, display::W x display::H)
void draw(uint16_t *picture);

}  // namespace osk
