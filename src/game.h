// The Cardputer game on its own thread (game.cpp), seen from the display
// thread (main.cpp).
#pragma once
#include <cstdint>
#include <string>

namespace game {

// Once: reads the settings file, then runs setup() and loop() forever
void start(const std::string &settingsFile);

// Copies the latest picture (240 x 135 RGB565 values) into dst if it is
// newer than "seen"; returns the picture's number
int picture(uint16_t *dst, int seen);

// Game language: 0 French, 1 English
int language();

}  // namespace game
