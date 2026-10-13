// The sound: one SDL output (main.cpp) at RATE, stereo, fed by the game's
// speaker (audio.cpp) or by mission control's synthesis (control/).
#pragma once
#include <cstdint>

namespace audio {

constexpr int RATE = 48000;  // mission control's SOUND_RATE (control/sound.h): no resampling

// The game's speaker: frames stereo frames into out
void mix(int16_t *out, int frames);

}  // namespace audio
