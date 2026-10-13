// The Cardputer's speaker (Android's). The game calls tone(), playRaw(),
// stop() and setChannelVolume() as on the Cardputer (M5Unified's
// Speaker_Class); here the channels are mixed into 16-bit samples that SDL's
// audio thread asks for (audio::mix, main.cpp).
//
// As on the Cardputer: 8 channels, each playing one sound at a time; a tone is
// a sine wave (M5Unified's default tone is a 16-sample sine); master volume and
// channel volumes 0 to 255.
#include "audio.h"

#include <M5Cardputer.h>

#include <cmath>
#include <mutex>

namespace {

constexpr int CHANNELS = 8;
constexpr float TONE_LEVEL = 0.22f;  // a few channels at once stay below clipping
constexpr double TWO_PI = 6.283185307179586;

struct Channel {
    // Tone
    double freq = 0;
    double phase = 0;
    uint32_t toneLeft = 0;  // samples still to play
    // Raw samples (take-off rumble)
    const int16_t *raw = nullptr;
    size_t rawLen = 0;
    double rawPos = 0;
    double rawStep = 0;
    uint32_t repeatsLeft = 0;
    uint8_t volume = 255;
};

std::mutex lock;
Channel channels[CHANNELS];
uint8_t master = 255;

int pick(int channel) {
    if (channel >= 0 && channel < CHANNELS) {
        return channel;
    }
    for (int i = 0; i < CHANNELS; i++) {  // channel -1: the first free one
        if (channels[i].toneLeft == 0 && channels[i].raw == nullptr) {
            return i;
        }
    }
    return 0;
}

bool busy(const Channel &c) {
    return c.toneLeft > 0 || c.raw != nullptr;
}

}  // namespace

bool Speaker_Class::begin() {
    return true;
}

void Speaker_Class::setVolume(uint8_t volume) {
    std::lock_guard<std::mutex> g(lock);
    master = volume;
}

bool Speaker_Class::tone(float frequency, uint32_t duration, int channel, bool stop_current) {
    std::lock_guard<std::mutex> g(lock);
    Channel &c = channels[pick(channel)];
    if (busy(c) && !stop_current) {
        return false;
    }
    c.raw = nullptr;
    c.freq = frequency;
    c.toneLeft = duration == UINT32_MAX ? UINT32_MAX : (uint32_t)((uint64_t)duration * audio::RATE / 1000);
    return true;
}

void Speaker_Class::stop() {
    std::lock_guard<std::mutex> g(lock);
    for (Channel &c : channels) {
        c.toneLeft = 0;
        c.raw = nullptr;
    }
}

void Speaker_Class::stop(uint8_t channel) {
    std::lock_guard<std::mutex> g(lock);
    if (channel < CHANNELS) {
        channels[channel].toneLeft = 0;
        channels[channel].raw = nullptr;
    }
}

bool Speaker_Class::isPlaying(uint8_t channel) const {
    std::lock_guard<std::mutex> g(lock);
    return channel < CHANNELS && busy(channels[channel]);
}

bool Speaker_Class::playRaw(const int16_t *data, size_t len, uint32_t rate, bool, uint32_t repeat, int channel,
                            bool stop_current) {
    std::lock_guard<std::mutex> g(lock);
    Channel &c = channels[pick(channel)];
    if (busy(c) && !stop_current) {
        return false;
    }
    c.toneLeft = 0;
    c.raw = data;
    c.rawLen = len;
    c.rawPos = 0;
    c.rawStep = (double)rate / audio::RATE;
    c.repeatsLeft = repeat == 0 ? 1 : repeat;
    return true;
}

void Speaker_Class::setChannelVolume(uint8_t channel, uint8_t volume) {
    std::lock_guard<std::mutex> g(lock);
    if (channel < CHANNELS) {
        channels[channel].volume = volume;
    }
}

namespace audio {

// Mixes frames stereo frames (the same sample on both sides) into out
void mix(int16_t *out, int frames) {
    std::lock_guard<std::mutex> g(lock);
    const float m = master / 255.0f;
    for (int i = 0; i < frames; i++) {
        float v = 0;
        for (Channel &c : channels) {
            float vol = c.volume / 255.0f * m;
            if (c.toneLeft > 0) {
                v += (float)std::sin(c.phase) * TONE_LEVEL * vol;
                c.phase += TWO_PI * c.freq / RATE;
                if (c.phase > TWO_PI) {
                    c.phase -= TWO_PI;
                }
                if (c.toneLeft != UINT32_MAX) {
                    c.toneLeft--;
                }
            } else if (c.raw) {
                v += c.raw[(size_t)c.rawPos] / 32768.0f * 0.8f * vol;
                c.rawPos += c.rawStep;
                if (c.rawPos >= c.rawLen) {
                    c.rawPos -= c.rawLen;
                    if (--c.repeatsLeft == 0) {
                        c.raw = nullptr;
                    }
                }
            }
        }
        v = v > 1 ? 1 : v < -1 ? -1 : v;
        out[2 * i] = out[2 * i + 1] = (int16_t)(v * 32000);
    }
}

}  // namespace audio
