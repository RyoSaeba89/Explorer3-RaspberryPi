// After "Multiplayer" in the game's mode menu, which role this Pi plays (as
// on Android): the crew (the game goes on as on the Cardputer) or mission
// control. Either way the game's own Wi-Fi screens come next (user's choice:
// network "like the Cardputer"); as mission control, once the network is up
// (the game's "connect the screen" step), the control screen takes over
// (input.cpp, main.cpp) and the game's page server stays off.
// Drawn over the game's screen in the game's own style, like the on-screen
// keyboard. Game thread only, except controlRole() and controlRequested().
#pragma once
#include <cstdint>

namespace role {

enum Choice { CREW = 0, CONTROL = 1 };

bool isOpen();
void open();   // the cursor starts on the crew
void close();
void move(int dy);
Choice selected();

// Draws the menu over the whole picture (RGB565, display::W x display::H)
void draw(uint16_t *picture, int lang);

// This Pi plays mission control (chosen in the menu, until endControl())
bool controlRole();
void chooseControl();

// The network is up: the control screen takes over (any thread; true once)
void requestControl();
bool controlRequested();

// Back from the control screen: the crew role again (main thread)
void endControl();

}  // namespace role
