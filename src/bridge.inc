
// ---------------------------------------------------------------- Raspberry Pi
// Appended to the copy of main.cpp by tools/prepare.sh (see src/bridge.h).
// Only reads the game's state, and gives tr() the text of the device in use.
#include "bridge.h"
#include "control/gamedata.h"
#include "labels.h"
#include "textes_xbox.h"
#include "textes_ps.h"

#include <unordered_map>

namespace {

// tr(): the same text in the version of the device in use (labels.h). The
// three versions of textes.h declare the same texts in the same order;
// texts_table.inc (tools/prepare.sh) lists them side by side.
struct Versions {
    const char *const *v[3];
};

std::unordered_map<const void *, Versions> buildVersions() {
    std::unordered_map<const void *, Versions> m;
    auto add = [&](const char *const *k, const char *const *x, const char *const *p) { m[k] = {{k, x, p}}; };
#include "texts_table.inc"
    return m;
}

const char *padText(Tx &t, int l) {
    static const std::unordered_map<const void *, Versions> versions = buildVersions();
    auto it = versions.find(&t[0]);
    if (it == versions.end()) {
        return t[l];
    }
    return it->second.v[labels::style()][l];
}

}  // namespace

namespace bridge {

Entry entry() {
    if (paused || helpOpen) {
        return Entry::None;
    }
    if (state == St::SsidEntry || state == St::Password) {
        return Entry::Text;
    }
    if (state == St::Puzzle) {
        switch (puzzleKind()) {
            case Pz::Lamp:
            case Pz::Sound: return Entry::Letter;
            case Pz::Quiz: return Entry::Quiz;
            default: return Entry::None;
        }
    }
    if (state == St::Computer) {
        if (multiMode) {
            return keypadShown(millis()) ? Entry::Digits : Entry::None;
        }
        return termVisible(millis()) >= TERM_COUNT ? Entry::Code : Entry::None;
    }
    return Entry::None;
}

char quizLetter(int i) {
    return quizLabel[i];
}

bool wifiList() {
    return state == St::WifiList;
}

bool atAddress() {
    return state == St::Address;
}

bool multiplayerSelected() {
    return state == St::Mode && modeSel == 1 && !paused;
}

int language() {
    return lang;
}

uint32_t screenId() {
    return stateStart + (uint32_t)state * 7919u + (paused ? 1u : 0u);
}

}  // namespace bridge

// Mission control draws the game's own pictures (control/gamedata.h): the
// same as on the crew's screen and in the web page's /sym.js.
static_assert(ASTRO_W == GAME_ASTRO_W && ASTRO_H == GAME_ASTRO_H, "control/gamedata.h: astronaut size");
static_assert(SYM_COUNT == GAME_SYMBOLS, "control/gamedata.h: number of symbols");
static_assert(sizeof(MORSE) / sizeof(MORSE[0]) == 26, "control/gamedata.h: Morse code");

extern "C" {

const char *game_morse(int letter) {
    return letter >= 0 && letter < 26 ? MORSE[letter] : "";
}

const char *game_astro_row(int row) {
    return ASTRO[row];
}

int game_symbol_pixel(int n, int row, int col) {
    return SYMBOLS[n][row][col] == 'X';
}

}  // extern "C"
