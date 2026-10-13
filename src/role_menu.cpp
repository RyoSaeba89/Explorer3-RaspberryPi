// See role_menu.h. Drawn like the game's language menu (drawLang and
// drawChoices of main.cpp): same colours, font, boxes and footer.
#include "role_menu.h"

#include <M5GFX.h>

#include <atomic>

#include "display.h"
#include "labels.h"

namespace mirror {
void stopServer();  // mirror_pi.cpp
}

namespace role {
namespace {

bool shown = false;
int sel = 0;
std::atomic<bool> wantControl(false);
std::atomic<bool> isControl(false);
lgfx::LGFX_Sprite sprite;

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// The game's colours (main.cpp)
const uint16_t C_BLACK = 0x0000;
const uint16_t C_SPACE = rgb(6, 6, 22);
const uint16_t C_PANEL = rgb(18, 22, 40);
const uint16_t C_PANEL2 = rgb(32, 38, 66);
const uint16_t C_BORDER = rgb(70, 90, 140);
const uint16_t C_TEXT = rgb(235, 238, 245);
const uint16_t C_DIM = rgb(140, 150, 175);
const uint16_t C_ORANGE = rgb(255, 160, 40);
const uint16_t C_YELLOW = rgb(255, 225, 70);

const char *const TITLE[2] = {"MULTIJOUEUR", "MULTIPLAYER"};
const char *const NAMES[2][2] = {{"Équipage", "Crew"}, {"Centre de contrôle", "Mission control"}};
const char *const INFOS[2][2] = {{"Cet appareil joue, sur Mars", "This device plays, on Mars"},
                                 {"Cet appareil sert d'écran", "This device is the screen"}};

// Footer: arrows drawn as triangles, then the keys of the device in use
const char *const CHOOSE[2] = {" : choisir", ": select"};
const char *const OK_TEXT[2] = {" : ok", ": confirm"};
const char *const BACK_TEXT[2] = {" : retour", ": back"};

void text(const char *s, int x, int y, uint16_t col, textdatum_t datum = top_left) {
    sprite.setTextDatum(datum);
    sprite.setTextColor(col);
    sprite.drawString(s, x, y);
}

void stars() {
    uint32_t seed = 3;
    for (int i = 0; i < 45; i++) {  // fixed sky, like the game's
        seed = seed * 1103515245u + 12345u;
        int x = (seed >> 8) % display::W;
        seed = seed * 1103515245u + 12345u;
        int y = (seed >> 8) % display::H;
        uint8_t b = 60 + (seed >> 20) % 140;
        sprite.drawPixel(x, y, rgb(b, b, b));
    }
}

// "key : action" groups along the bottom: key in orange, action dimmed
void footer(int lang) {
    const char *OK_KEY = labels::ok(lang);
    const char *BACK_KEY = labels::back(lang);
    sprite.fillRect(0, display::H - 14, display::W, 14, C_PANEL);
    sprite.drawFastHLine(0, display::H - 15, display::W, C_BORDER);
    int y = display::H - 13;
    int wChoose = 18 + sprite.textWidth(CHOOSE[lang]);
    int wOk = sprite.textWidth(OK_KEY) + sprite.textWidth(OK_TEXT[lang]);
    int wBack = sprite.textWidth(BACK_KEY) + sprite.textWidth(BACK_TEXT[lang]);
    int gap = 14;
    int x = (display::W - (wChoose + wOk + wBack + 2 * gap)) / 2;
    sprite.fillTriangle(x, y + 8, x + 6, y + 8, x + 3, y + 3, C_ORANGE);       // up
    sprite.fillTriangle(x + 9, y + 4, x + 15, y + 4, x + 12, y + 9, C_ORANGE);  // down
    text(CHOOSE[lang], x + 18, y, C_DIM);
    x += wChoose + gap;
    text(OK_KEY, x, y, C_ORANGE);
    text(OK_TEXT[lang], x + sprite.textWidth(OK_KEY), y, C_DIM);
    x += wOk + gap;
    text(BACK_KEY, x, y, C_ORANGE);
    text(BACK_TEXT[lang], x + sprite.textWidth(BACK_KEY), y, C_DIM);
}

}  // namespace

bool isOpen() {
    return shown;
}

void open() {
    shown = true;
    sel = CREW;
}

void close() {
    shown = false;
}

void move(int dy) {
    sel = (sel + dy + 2) % 2;
}

Choice selected() {
    return sel == CONTROL ? CONTROL : CREW;
}

void draw(uint16_t *picture, int lang) {
    if (!shown) {
        return;
    }
    lang = lang ? 1 : 0;
    if (sprite.width() == 0) {
        sprite.setColorDepth(16);
        sprite.createSprite(display::W, display::H);
        sprite.setFont(&fonts::efontJA_12);
        sprite.setTextWrap(false);
    }
    sprite.fillSprite(C_SPACE);
    stars();
    text(TITLE[lang], display::W / 2 + 2, 9, C_BLACK, top_center);  // shadow
    text(TITLE[lang], display::W / 2 + 1, 8, C_ORANGE, top_center);
    for (int i = 0; i < 2; i++) {  // drawChoices(…, 2, sel, 32, 32, 8)
        const int top = 32, h = 32, gap = 8;
        int y = top + i * (h + gap);
        bool on = i == sel;
        sprite.fillRoundRect(16, y, display::W - 32, h, 5, on ? C_PANEL2 : C_PANEL);
        sprite.drawRoundRect(16, y, display::W - 32, h, 5, on ? C_YELLOW : C_BORDER);
        if (on) {
            sprite.fillTriangle(24, y + h / 2 - 6, 24, y + h / 2 + 6, 31, y + h / 2, C_YELLOW);
        }
        int ty = (h - 26) / 2 + 1;
        text(NAMES[i][lang], 38, y + ty, on ? C_YELLOW : C_TEXT);
        text(INFOS[i][lang], 38, y + ty + 13, C_DIM);
    }
    footer(lang);
    const uint8_t *src = (const uint8_t *)sprite.getBuffer();  // high byte first
    for (int i = 0; i < display::W * display::H; i++, src += 2) {
        picture[i] = (uint16_t)(src[0] << 8 | src[1]);
    }
}

bool controlRole() {
    return isControl;
}

void chooseControl() {
    isControl = true;
    mirror::stopServer();  // a crew's page server from an earlier game: off
}

void endControl() {
    isControl = false;
}

void requestControl() {
    wantControl = true;
}

bool controlRequested() {
    return wantControl.exchange(false);
}

}  // namespace role
