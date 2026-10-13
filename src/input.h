// Keys and gamepad buttons sent by the display thread (main.cpp), turned into
// Cardputer key presses on the game thread (input.cpp).
#pragma once

namespace input {

enum Code {
    // Gamepad, by role (see main.cpp for the physical buttons)
    UP = 1,
    DOWN,
    LEFT,
    RIGHT,
    CONFIRM,   // ENTER
    BACK,      // ESC (` key)
    KEYBOARD,  // opens / closes the on-screen keyboard
    SPACE,     // SPACE (play the Morse code); erases in a code keyboard
    HELP,      // TAB
    PAUSE,     // Fn pressed 3 times
    // Keyboard keys
    KEY_ENTER,
    KEY_ESC,
    KEY_DEL,
    KEY_TAB,
    KEY_SPACE,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_CTRL,  // the Cardputer's Fn (user's choice: Ctrl 3 times pauses)
    // From 33 on: a printable character typed on the keyboard
};

// Any thread: queues a key or button
void push(int code);

}  // namespace input
