// The Cardputer's screen becomes a 240 x 135 RGB565 memory area that the
// display thread shows (as on Android). Replaces the "desktop" part of
// M5GFX.cpp, written for SDL; the rest of M5GFX is compiled unchanged.
#include "display.h"

#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_FrameBufferBase.hpp>

namespace {

// Memory panel. The Cardputer has a 135 x 240 panel that the game rotates
// (setRotation(1)); here the memory is already landscape and
// offset_rotation = 3 cancels that rotation: picture row y is memory row y.
struct MemoryPanel : public lgfx::Panel_FrameBufferBase {
    MemoryPanel() {
        auto c = config();
        c.memory_width = c.panel_width = display::W;
        c.memory_height = c.panel_height = display::H;
        c.offset_rotation = 3;
        config(c);
        for (int y = 0; y < display::H; y++) {
            rows[y] = pixels + y * display::W * 2;
        }
        _lines_buffer = rows;
    }

    // No bus and no pins: only the state set by Panel_FrameBufferBase::init()
    bool init(bool) override {
        _range_mod.top = INT16_MAX;
        _range_mod.left = INT16_MAX;
        _range_mod.right = 0;
        _range_mod.bottom = 0;
        setRotation(_rotation);
        return true;
    }

    uint8_t pixels[display::W * display::H * 2] = {};  // RGB565, high byte first
    uint8_t *rows[display::H];
};

MemoryPanel memoryPanel;

}  // namespace

namespace display {

void copy(uint16_t *dst) {
    const uint8_t *p = memoryPanel.pixels;
    for (int i = 0; i < W * H; i++, p += 2) {
        dst[i] = (uint16_t)(p[0] << 8 | p[1]);
    }
}

}  // namespace display

namespace m5gfx {

M5GFX *M5GFX::_instance = nullptr;

M5GFX::M5GFX(void) : LGFX_Device() {
    if (_instance == nullptr) _instance = this;
}

bool M5GFX::init_impl(bool use_reset, bool use_clear) {
    _board = autodetect(use_reset, board_t::board_M5Cardputer);
    return LGFX_Device::init_impl(use_reset, use_clear);
}

board_t M5GFX::autodetect(bool, board_t board) {
    panel(&memoryPanel);
    return board;
}

// Copied from M5GFX.cpp (outside its SDL part)
void M5GFX::progressBar(int x, int y, int w, int h, uint8_t val) {
    drawRect(x, y, w, h, 0x09F1);
    fillRect(x + 1, y + 1, w * (((float)val) / 100.0f), h - 1, 0x09F1);
}

void M5GFX::pushState(void) {
    DisplayState s;
    s.gfxFont = _font;
    s.style = _text_style;
    s.metrics = _font_metrics;
    s.cursor_x = _cursor_x;
    s.cursor_y = _cursor_y;
    _displayStateStack.push_back(s);
}

void M5GFX::popState(void) {
    if (_displayStateStack.empty()) return;
    DisplayState s = _displayStateStack.back();
    _displayStateStack.pop_back();
    _font = s.gfxFont;
    _text_style = s.style;
    _font_metrics = s.metrics;
    _cursor_x = s.cursor_x;
    _cursor_y = s.cursor_y;
}

}  // namespace m5gfx
