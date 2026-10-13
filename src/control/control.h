/* Mission control on the Pi: this Pi as the crew's screen, in place of a web
 * browser (as Explorer3-Android can be). Called by
 * main.cpp on the display thread, except control_mix (audio thread). */
#ifndef CONTROL_H
#define CONTROL_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Buttons, by role (main.cpp turns keys and gamepad buttons into them) */
enum { CB_UP = 1, CB_DOWN, CB_LEFT, CB_RIGHT, CB_OK, CB_BACK, CB_KB, CB_L, CB_R };

/* The control screen opens: settings file (last address, language), the
 * game's language until the crew sends its own (0 French, 1 English). */
void control_open(const char *settings, int lang);

/* Keys drawn as the keyboard's (0), an Xbox pad's (1) or a PlayStation pad's (2) */
void control_style(int style);

/* The Pi's connection: name (Wi-Fi or "Ethernet") and IPv4 address, empty
 * when offline. Cheap when nothing changed: called every frame. */
void control_network(const char *name, const char *ip);

void control_button(int code, bool down);

/* One frame on a w x h window (drawn through sdl_draw.h); false when the
 * player went back to the game's menu (the connection is then closed). */
bool control_frame(int w, int h);

/* The sound (audio thread): frames stereo frames at SOUND_RATE; latencyMs:
 * how long before a sample written now is heard. */
void control_mix(int16_t *out, int frames, int latencyMs);

#ifdef __cplusplus
}
#endif

#endif
