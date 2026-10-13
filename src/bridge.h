// Read-only view of the game's state for the Pi's input layer. Implemented
// in bridge.inc, which tools/prepare.sh appends to the copy of main.cpp: in
// the same translation unit, the game's internal variables are visible
// without changing a line of the game. Game thread only.
#pragma once
#include <cstdint>

namespace bridge {

// What the current screen expects the player to type
enum class Entry {
    None,
    Letter,  // one letter answers (Morse puzzles)
    Quiz,    // one of the 4 letters shown answers (quizLetter)
    Code,    // start-up code: letters, erase, confirm
    Digits,  // coded keypad (multiplayer): 1 to 9, erase, confirm
    Text,    // network name or Wi-Fi password: any character, erase, confirm
};

Entry entry();

// Letter of the quiz answer i (0 to 3), drawn by the game for each game
char quizLetter(int i);

// Changes whenever the game moves to another screen
uint32_t screenId();

// The game's Wi-Fi list (refresh with the gamepad's left button)
bool wifiList();

// "Connect the screen": the network is up (as mission control, the control
// screen takes over there)
bool atAddress();

// Mode menu with "Multiplayer" selected: confirming first asks which role
// this Pi plays (role_menu.h)
bool multiplayerSelected();

// Game language: 0 French, 1 English
int language();

}  // namespace bridge
