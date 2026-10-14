// Turns keys and gamepad buttons into Cardputer key presses. The display
// thread queues codes; the game thread reads them in
// Keyboard_Class::updateKeysState(), so the on-screen keyboard and the
// game's state (bridge.h) are only ever touched by the game thread.
//
// Keyboard: the Cardputer's keys, as in the PC simulator (arrows = ; . , /,
// Escape = `), Ctrl = Fn.
// Gamepad (as on Android): the on-screen keyboard opens with the top face
// button where the screen expects a letter (Morse puzzles, quiz), and by
// itself for the start-up code, the coded keypad and the Wi-Fi screens where
// a name or password is typed. It closes after an answer, when the screen
// changes, with the back button, and as soon as the keyboard is used.
#include "input.h"

#include <M5Cardputer.h>

#include <deque>
#include <mutex>

#include "bridge.h"
#include "labels.h"
#include "onscreen_keyboard.h"

namespace {

std::mutex codeLock;
std::deque<int> codes;          // from the display thread
std::deque<KeysState> presses;  // for the game, game thread only

uint32_t lastScreen = 0;
bool dismissed = false;  // keyboard closed by the player on this screen: no reopening by itself

KeysState character(char c) {
    KeysState k;
    k.word.push_back(c);
    k.shift = c == '`';  // typed as a character, not the ESC key (the game checks shift)
    return k;
}

KeysState special(bool KeysState::*field) {
    KeysState k;
    k.*field = true;
    return k;
}

KeysState space() {
    KeysState k = special(&KeysState::space);
    k.word.push_back(' ');
    return k;
}

KeysState arrow(char c) {
    KeysState k;
    k.word.push_back(c);
    return k;
}

bool answers(bridge::Entry m) {
    return m == bridge::Entry::Letter || m == bridge::Entry::Quiz;
}

bool isKeyboard(int code) {
    return code >= input::KEY_ENTER;
}

void closeByPlayer() {
    osk::close();
    dismissed = true;
}

// The open on-screen keyboard takes the buttons it uses; true if it did
bool keyboardButton(int code) {
    if (!osk::isOpen()) {
        return false;
    }
    bridge::Entry m = osk::mode();
    switch (code) {
        case input::UP: osk::move(0, -1); return true;
        case input::DOWN: osk::move(0, 1); return true;
        case input::LEFT: osk::move(-1, 0); return true;
        case input::RIGHT: osk::move(1, 0); return true;
        case input::CONFIRM: {
            int c = osk::selected();
            if (c == osk::ERASE) {
                if (answers(m)) {
                    closeByPlayer();  // nothing to erase: the arrow closes
                } else {
                    presses.push_back(special(&KeysState::del));
                }
            } else if (c == osk::OK) {
                presses.push_back(special(&KeysState::enter));  // closes when the code is accepted
            } else if (c == osk::SHIFT || c == osk::SYMBOLS) {
                osk::press(c);  // another page of characters
            } else if (c == ' ') {
                presses.push_back(space());
            } else {
                presses.push_back(character((char)c));
                if (answers(m)) {
                    osk::close();  // one letter answers
                }
            }
            return true;
        }
        case input::SPACE:
            if (answers(m)) {
                return false;  // replays the Morse code while choosing
            }
            presses.push_back(special(&KeysState::del));
            return true;
        case input::BACK:
        case input::KEYBOARD:
            closeByPlayer();
            return true;
        default:
            return false;  // help and pause reach the game
    }
}

void handle(int code) {
    if (isKeyboard(code) && osk::isOpen()) {
        osk::close();  // typing on the keyboard: no need for the on-screen one
    }
    if (keyboardButton(code)) {
        return;
    }
    bool typing = bridge::entry() == bridge::Entry::Text;  // arrows would type ; . , /
    switch (code) {
        case input::UP:
        case input::KEY_UP:
            if (!typing) presses.push_back(arrow(';'));
            break;
        case input::DOWN:
        case input::KEY_DOWN:
            if (!typing) presses.push_back(arrow('.'));
            break;
        case input::LEFT:
        case input::KEY_LEFT:
            if (!typing) presses.push_back(arrow(','));
            break;
        case input::RIGHT:
        case input::KEY_RIGHT:
            if (!typing) presses.push_back(arrow('/'));
            break;
        case input::CONFIRM:
        case input::KEY_ENTER:
            presses.push_back(special(&KeysState::enter));
            break;
        case input::BACK:
        case input::KEY_ESC:
            presses.push_back(arrow('`'));
            break;
        case input::KEYBOARD:
            dismissed = false;
            osk::open(bridge::entry());  // nothing opens where nothing is typed
            break;
        case input::SPACE:
            // Left face button: SPACE, R (refresh) on the Wi-Fi list, erase when typing
            if (typing) {
                presses.push_back(special(&KeysState::del));
            } else {
                presses.push_back(bridge::wifiList() ? arrow('r') : space());
            }
            break;
        case input::KEY_SPACE:
            presses.push_back(space());
            break;
        case input::HELP:
        case input::KEY_TAB:
            presses.push_back(special(&KeysState::tab));
            break;
        case input::KEY_DEL: presses.push_back(special(&KeysState::del)); break;
        case input::KEY_CTRL: presses.push_back(special(&KeysState::fn)); break;
        case input::PAUSE:
            // The game pauses after Fn pressed 3 times within 800 ms each
            for (int i = 0; i < 3; i++) {
                presses.push_back(special(&KeysState::fn));
            }
            break;
        default:
            if (code > ' ' && code < 127) {
                presses.push_back(character((char)code));
            }
            break;
    }
}

// The on-screen keyboard follows the screen: closed on a new screen or when
// the screen no longer takes this kind of entry, opened by itself where the
// gamepad has to type.
void followScreen() {
    uint32_t screen = bridge::screenId();
    if (screen != lastScreen) {
        lastScreen = screen;
        dismissed = false;
        osk::close();
    }
    bridge::Entry e = bridge::entry();
    if (osk::isOpen() && osk::mode() != e) {
        osk::close();
    }
    bool pad = labels::style() != labels::KEYBOARD;
    bool byItself = e == bridge::Entry::Code || e == bridge::Entry::Digits || e == bridge::Entry::Text;
    if (!osk::isOpen() && !dismissed && pad && byItself) {
        osk::open(e);
    }
}

}  // namespace

namespace input {

void push(int code) {
    std::lock_guard<std::mutex> g(codeLock);
    codes.push_back(code);
}

}  // namespace input

// One press per loop turn, followed by a "released" turn, like the real keyboard
void Keyboard_Class::updateKeysState() {
    std::deque<int> todo;
    {
        std::lock_guard<std::mutex> g(codeLock);
        todo.swap(codes);
    }
    for (int code : todo) {
        handle(code);
    }
    followScreen();
    if (_release) {
        _release = false;
        _change = true;
        _pressed = false;
        _state = KeysState();
    } else if (!presses.empty()) {
        _state = presses.front();
        presses.pop_front();
        _change = _pressed = true;
        _release = true;
    } else {
        _change = _pressed = false;
    }
}
