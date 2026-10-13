// M5Cardputer reduced to what the game uses (Android's). Same as the PC
// simulator's header (sim/include/M5Cardputer.h), except the speaker, which
// really plays here (src/audio.cpp). The screen is M5GFX on a memory panel
// (display_m5gfx.cpp), the keyboard comes from the Pi's keyboard and gamepads
// (input.cpp).
#pragma once
#include <Arduino.h>
#include <M5GFX.h>

#include <memory>
#include <vector>

namespace m5 {
using board_t = m5gfx::board_t;
}

struct Point2D_t {
    int x = 0;
    int y = 0;
    bool operator==(const Point2D_t &o) const { return x == o.x && y == o.y; }
};

class KeyboardReader {
public:
    virtual ~KeyboardReader() = default;
    virtual void begin() {}
    virtual void update() {}

protected:
    std::vector<Point2D_t> _key_list;
};

struct KeysState {
    std::vector<char> word;
    bool tab = false, fn = false, shift = false, ctrl = false, opt = false, alt = false;
    bool del = false, enter = false, space = false;
};

class Keyboard_Class {
public:
    void begin() {}
    void begin(std::unique_ptr<KeyboardReader>) {}
    void updateKeyList() {}
    void updateKeysState();  // input.cpp: one press per loop turn, then a release
    bool isChange() const { return _change; }
    bool isPressed() const { return _pressed; }
    KeysState keysState() const { return _state; }

private:
    KeysState _state;
    bool _change = false;
    bool _pressed = false;
    bool _release = false;
};

// Same calls as M5Unified's Speaker_Class, as used by the game (audio.cpp)
class Speaker_Class {
public:
    bool begin();
    void setVolume(uint8_t volume);
    bool tone(float frequency, uint32_t duration, int channel = -1, bool stop_current = true);
    void stop();
    void stop(uint8_t channel);
    bool isPlaying(uint8_t channel) const;
    bool playRaw(const int16_t *data, size_t len, uint32_t rate, bool stereo, uint32_t repeat, int channel,
                 bool stop_current);
    void setChannelVolume(uint8_t channel, uint8_t volume);
};

struct M5Cardputer_Class {
    M5GFX Display;
    Keyboard_Class Keyboard;
    Speaker_Class Speaker;
    template <typename C>
    void begin(const C &, bool) { Display.init(); }
    void update() {}
};
extern M5Cardputer_Class M5Cardputer;

struct M5_Class {
    struct config_t {};
    config_t config() const { return {}; }
    m5::board_t getBoard() const { return m5::board_t::board_M5Cardputer; }
};
extern M5_Class M5;
