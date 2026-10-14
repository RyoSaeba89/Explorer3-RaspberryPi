// Explorer 3 on the Raspberry Pi (3, 400): the display thread. One SDL
// window, on the desktop or on the console without a desktop (SDL's KMSDRM,
// always full screen). It shows either the game (game.cpp, the Cardputer's
// game on its own thread) or mission control (control/), turns the keyboard
// and the gamepads into the game's keys, and plays the sound.
//
// On the desktop (user's choice): a 1280 x 720 window; F11 switches to full
// screen and back, and the choice is kept for the next time (window.conf).
//
//   explorer3               as last time (a window the first time)
//   explorer3 --window      in a window
//   explorer3 --fullscreen  full screen
//
// Quit (user's choice): Ctrl+Q, Alt+F4 on the desktop, or Start + Select
// together on a gamepad.
#include <SDL.h>
#include <sys/stat.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#include "audio.h"
#include "display.h"
#include "game.h"
#include "input.h"
#include "labels.h"
#include "netctl.h"
#include "control_link.h"

extern "C" {
#include "control.h"
#include "draw.h"
#include "sdl_draw.h"
}

#ifndef EXPLORER3_VERSION
#define EXPLORER3_VERSION "dev"
#endif

namespace {

std::atomic<bool> controlMode(false);  // the window shows mission control
std::string configDir;
int audioLatencyMs = 50;
SDL_Window *window = nullptr;
bool fullScreen = false;
bool console = false;  // no desktop (KMSDRM): always full screen

constexpr int WINDOW_W = 1280;
constexpr int WINDOW_H = 720;

// ---------------------------------------------------------------- files

// ~/.config/explorer3 (or $XDG_CONFIG_HOME/explorer3), made if needed
std::string makeConfigDir() {
    std::string base;
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg) {
        base = xdg;
    } else if (home && *home) {
        base = std::string(home) + "/.config";
    } else {
        base = "/tmp";
    }
    mkdir(base.c_str(), 0700);
    std::string dir = base + "/explorer3";
    mkdir(dir.c_str(), 0700);
    return dir;
}

// window.conf: "fullscreen=1" when F11 last chose full screen
bool loadFullScreen() {
    FILE *f = fopen((configDir + "/window.conf").c_str(), "r");
    if (!f) {
        return false;
    }
    char line[64];
    bool on = false;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "fullscreen=", 11) == 0) {
            on = line[11] == '1';
        }
    }
    fclose(f);
    return on;
}

void saveFullScreen(bool on) {
    std::string path = configDir + "/window.conf";
    FILE *f = fopen((path + ".tmp").c_str(), "w");
    if (!f) {
        return;
    }
    fprintf(f, "fullscreen=%d\n", on ? 1 : 0);
    fclose(f);
    rename((path + ".tmp").c_str(), path.c_str());
}

// ---------------------------------------------------------------- window

// Full screen or a window; the mouse pointer only shows in a window
void setFullScreen(bool on) {
    fullScreen = on || console;
    SDL_SetWindowFullscreen(window, fullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if (!fullScreen) {
        SDL_SetWindowSize(window, WINDOW_W, WINDOW_H);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    SDL_ShowCursor(fullScreen ? SDL_DISABLE : SDL_ENABLE);
}

// ---------------------------------------------------------------- sound

// SDL's audio thread: the game's speaker, or mission control's sound. The
// latter is rendered all the time, so that its clock stays right.
void SDLCALL fillAudio(void *, Uint8 *stream, int len) {
    int16_t *out = reinterpret_cast<int16_t *>(stream);
    int frames = len / 4;
    static int16_t control[8192 * 2];
    if (frames > 8192) {
        frames = 8192;
    }
    audio::mix(out, frames);
    control_mix(control, frames, audioLatencyMs);
    if (controlMode) {
        memcpy(out, control, (size_t)frames * 4);
    }
}

void openAudio() {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = audio::RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = fillAudio;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev == 0) {
        fprintf(stderr, "explorer3: no sound (%s)\n", SDL_GetError());
        return;
    }
    // A sample written now is heard once SDL's buffer and the device's have played
    audioLatencyMs = 2 * have.samples * 1000 / have.freq;
    SDL_PauseAudioDevice(dev, 0);
}

// ---------------------------------------------------------------- gamepads

struct Pad {
    SDL_GameController *ctl = nullptr;
    labels::Style style = labels::XBOX;
    bool start = false, select = false, comboUsed = false;
    int stickX = 0, stickY = 0;  // -1, 0, 1: the left stick as a D-pad
};
std::map<SDL_JoystickID, Pad> pads;

labels::Style styleOf(SDL_GameController *c) {
    switch (SDL_GameControllerGetType(c)) {
        case SDL_CONTROLLER_TYPE_PS3:
        case SDL_CONTROLLER_TYPE_PS4:
        case SDL_CONTROLLER_TYPE_PS5: return labels::PLAYSTATION;
        default: return labels::XBOX;
    }
}

void openPad(int index) {
    SDL_GameController *c = SDL_GameControllerOpen(index);
    if (!c) {
        return;
    }
    Pad p;
    p.ctl = c;
    p.style = styleOf(c);
    pads[SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c))] = p;
}

void closePad(SDL_JoystickID id) {
    auto it = pads.find(id);
    if (it != pads.end()) {
        SDL_GameControllerClose(it->second.ctl);
        pads.erase(it);
    }
}

void quit() {
    SDL_Quit();
    exit(0);
}

// A gamepad role (input::UP...) pressed or released
void padRole(int role, bool down) {
    if (controlMode) {
        static const int toControl[] = {0, CB_UP, CB_DOWN, CB_LEFT, CB_RIGHT, CB_OK, CB_BACK, CB_KB, 0, CB_L, CB_R};
        if (role > 0 && role <= input::PAUSE && toControl[role]) {
            control_button(toControl[role], down);
        }
    } else if (down) {
        input::push(role);
    }
}

void padButton(Pad &p, Uint8 button, bool down) {
    labels::setStyle(p.style);
    control_style(p.style);
    // Start and Select act on release, unless they were pressed together (quit)
    if (button == SDL_CONTROLLER_BUTTON_START || button == SDL_CONTROLLER_BUTTON_BACK) {
        bool start = button == SDL_CONTROLLER_BUTTON_START;
        if (down) {
            (start ? p.start : p.select) = true;
            if (p.start && p.select) {
                quit();
            }
            return;
        }
        if (!p.comboUsed) {
            padRole(start ? input::PAUSE : input::HELP, true);
            padRole(start ? input::PAUSE : input::HELP, false);
        }
        (start ? p.start : p.select) = false;
        if (!p.start && !p.select) {
            p.comboUsed = false;
        }
        return;
    }
    int role = 0;
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: role = input::UP; break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: role = input::DOWN; break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: role = input::LEFT; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: role = input::RIGHT; break;
        case SDL_CONTROLLER_BUTTON_A: role = input::CONFIRM; break;          // bottom: A, cross
        case SDL_CONTROLLER_BUTTON_B: role = input::BACK; break;             // right: B, circle
        case SDL_CONTROLLER_BUTTON_Y: role = input::KEYBOARD; break;         // top: Y, triangle
        case SDL_CONTROLLER_BUTTON_X: role = input::SPACE; break;            // left: X, square
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: role = input::HELP; break;  // LB, L1
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: role = input::PAUSE; break;  // RB, R1
        default: break;
    }
    if (role) {
        padRole(role, down);
    }
}

// The left stick moves like the D-pad: one press each time it leaves the centre
void padStick(Pad &p, Uint8 axis, Sint16 value) {
    const int on = 20000, off = 12000;  // hysteresis
    int *cur = nullptr;
    int neg = 0, pos = 0;
    if (axis == SDL_CONTROLLER_AXIS_LEFTX) {
        cur = &p.stickX;
        neg = input::LEFT;
        pos = input::RIGHT;
    } else if (axis == SDL_CONTROLLER_AXIS_LEFTY) {
        cur = &p.stickY;
        neg = input::UP;
        pos = input::DOWN;
    } else {
        return;
    }
    int next = value <= -on ? -1 : value >= on ? 1 : (value > -off && value < off) ? 0 : *cur;
    if (next == *cur) {
        return;
    }
    labels::setStyle(p.style);
    control_style(p.style);
    if (*cur != 0) {
        padRole(*cur < 0 ? neg : pos, false);
    }
    *cur = next;
    if (next != 0) {
        padRole(next < 0 ? neg : pos, true);
    }
}

// ---------------------------------------------------------------- keyboard

void key(const SDL_KeyboardEvent &e, bool down) {
    SDL_Keycode k = e.keysym.sym;
    bool ctrl = (e.keysym.mod & KMOD_CTRL) != 0;
    if (down && ctrl && k == SDLK_q) {
        quit();
    }
    if (k == SDLK_F11) {
        if (down && !e.repeat && !console) {
            setFullScreen(!fullScreen);
            saveFullScreen(fullScreen);
        }
        return;
    }
    labels::setStyle(labels::KEYBOARD);
    control_style(labels::KEYBOARD);
    if (controlMode) {
        int b = 0;
        switch (k) {
            case SDLK_UP: b = CB_UP; break;
            case SDLK_DOWN: b = CB_DOWN; break;
            case SDLK_LEFT: b = CB_LEFT; break;
            case SDLK_RIGHT: b = CB_RIGHT; break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER: b = CB_OK; break;
            case SDLK_ESCAPE: b = CB_BACK; break;
            case SDLK_TAB: b = CB_KB; break;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
            case SDLK_PAGEDOWN: b = CB_L; break;
            case SDLK_PLUS:
            case SDLK_EQUALS:
            case SDLK_KP_PLUS:
            case SDLK_PAGEUP: b = CB_R; break;
            default: break;
        }
        if (b && !e.repeat) {
            control_button(b, down);
        }
        return;
    }
    if (!down || e.repeat) {  // the Cardputer's keyboard does not repeat
        return;
    }
    int code = 0;
    switch (k) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER: code = input::KEY_ENTER; break;
        case SDLK_ESCAPE: code = input::KEY_ESC; break;
        case SDLK_BACKSPACE:
        case SDLK_DELETE: code = input::KEY_DEL; break;
        case SDLK_TAB: code = input::KEY_TAB; break;
        case SDLK_SPACE: code = input::KEY_SPACE; break;
        case SDLK_UP: code = input::KEY_UP; break;
        case SDLK_DOWN: code = input::KEY_DOWN; break;
        case SDLK_LEFT: code = input::KEY_LEFT; break;
        case SDLK_RIGHT: code = input::KEY_RIGHT; break;
        case SDLK_LCTRL:
        case SDLK_RCTRL: code = input::KEY_CTRL; break;
        default: break;
    }
    if (code) {
        input::push(code);
    }
}

// Letters, digits and punctuation, in the keyboard's own layout
void text(const char *s) {
    if (controlMode) {
        return;
    }
    for (const char *p = s; *p; p++) {
        if (*p > ' ' && *p < 127) {
            input::push(*p);
        }
    }
}

// ---------------------------------------------------------------- screens

// While the Explorer3 network is being set up, the game waits: say so
void drawBusy(int lang) {
    const char *msg = lang ? "Creating the Explorer3 network…" : "Création du réseau Explorer3…";
    draw_fill(0, CANVAS_H / 2 - 40, CANVAS_W, 80, rgba_t{6, 6, 26, 230});
    draw_text_centered(CANVAS_W / 2, CANVAS_H / 2 - GLYPH_H * 1.5f, CANVAS_W, 3.0f, 0, rgb(0xffe146), 1, msg);
}

void openControl() {
    netctl::Status s = netctl::status();
    control_network(s.name.c_str(), s.ip.c_str());
    control_style(labels::style());
    control_open((configDir + "/control.conf").c_str(), game::language());
    controlMode = true;
}

// Back from mission control: the game goes back to its role menu
void closeControl() {
    controlMode = false;
    control_link::finish();
}

}  // namespace

int main(int argc, char **argv) {
    int asked = -1;  // --window 0, --fullscreen 1, otherwise as last time
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--window") == 0) {
            asked = 0;
        } else if (strcmp(argv[i], "--fullscreen") == 0) {
            asked = 1;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("explorer3 %s\n", EXPLORER3_VERSION);
            return 0;
        }
    }
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");  // buttons by position
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "explorer3: %s\n", SDL_GetError());
        return 1;
    }
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "explorer3: no gamepad (%s)\n", SDL_GetError());
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "explorer3: no sound (%s)\n", SDL_GetError());
    }
    configDir = makeConfigDir();
    const char *driver = SDL_GetCurrentVideoDriver();
    console = driver && strcmp(driver, "KMSDRM") == 0;
    bool full = console || (asked >= 0 ? asked == 1 : loadFullScreen());
    Uint32 flags = SDL_WINDOW_RESIZABLE | (full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_Window *win = SDL_CreateWindow("Explorer 3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W,
                                       WINDOW_H, flags | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        fprintf(stderr, "explorer3: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer *ren =
        SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, 0);
    }
    if (!ren) {
        fprintf(stderr, "explorer3: %s\n", SDL_GetError());
        return 1;
    }
    window = win;
    fullScreen = full;
    SDL_ShowCursor(full ? SDL_DISABLE : SDL_ENABLE);
    SDL_StartTextInput();
    sd_init(ren);

    netctl::init();
    game::start(configDir + "/settings.txt");
    openAudio();

    static uint16_t picture[display::W * display::H];
    int seen = -1;
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: quit(); break;
                case SDL_KEYDOWN: key(e.key, true); break;
                case SDL_KEYUP: key(e.key, false); break;
                case SDL_TEXTINPUT: text(e.text.text); break;
                case SDL_CONTROLLERDEVICEADDED: openPad(e.cdevice.which); break;
                case SDL_CONTROLLERDEVICEREMOVED: closePad(e.cdevice.which); break;
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP: {
                    auto it = pads.find(e.cbutton.which);
                    if (it != pads.end()) {
                        padButton(it->second, e.cbutton.button, e.type == SDL_CONTROLLERBUTTONDOWN);
                    }
                    break;
                }
                case SDL_CONTROLLERAXISMOTION: {
                    auto it = pads.find(e.caxis.which);
                    if (it != pads.end()) {
                        padStick(it->second, e.caxis.axis, e.caxis.value);
                    }
                    break;
                }
                case SDL_RENDER_TARGETS_RESET:
                case SDL_RENDER_DEVICE_RESET:
                    sd_reset();
                    seen = -1;
                    break;
                default: break;
            }
        }
        if (control_link::requested()) {
            openControl();  // mission control: the network is up
        }
        int w = 0, h = 0;
        SDL_GetRendererOutputSize(ren, &w, &h);
        if (controlMode) {
            netctl::Status s = netctl::status();
            control_network(s.name.c_str(), s.ip.c_str());
            if (!control_frame(w, h)) {
                closeControl();
            }
            continue;
        }
        int n = game::picture(picture, seen);
        sd_begin(w, h);
        sd_screen(picture, n != seen);
        seen = n;
        if (netctl::busy()) {
            drawBusy(game::language());
        }
        sd_end();
    }
}
