// Raspberry Pi: the Cardputer game (src/main.cpp of Escape-Game-Cardputer,
// unchanged apart from the texts and the few lines of tools/prepare.sh, with
// the read-only bridge.inc appended) runs on its own thread, with the PC
// simulator's headers (sim/include). main.cpp shows its picture and sends it
// the keys and buttons.
#include "game.h"

#include <M5Cardputer.h>
#include <Preferences.h>
#include <sys/stat.h>
#include <time.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <random>
#include <thread>

#include "bridge.h"
#include "display.h"
#include "onscreen_keyboard.h"

void setup();
void loop();

M5Cardputer_Class M5Cardputer;
M5_Class M5;

// ---------------------------------------------------------------- Arduino

namespace {

uint64_t nowMs() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

const uint64_t t0 = nowMs();
uint32_t randNext = 1;  // same generator as newlib (the ESP32's rand()), like the simulator
bool useHwRandom = true;
uint64_t newlibState = 1;

int newlibRand() {
    static uint32_t seeded = 0;
    if (seeded != randNext) {  // srand() called by randomSeed()
        newlibState = randNext;
        seeded = randNext;
    }
    newlibState = newlibState * 6364136223846793005ULL + 1;
    return (int)((newlibState >> 32) & 0x7fffffff);
}

}  // namespace

uint32_t millis() {
    return (uint32_t)(nowMs() - t0);
}

void delay(uint32_t ms) {
    timespec t = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000L};
    nanosleep(&t, nullptr);
}

uint32_t esp_random() {
    static std::random_device rd;
    return rd();
}

void randomSeed(unsigned long seed) {
    if (seed != 0) {
        randNext = seed;
        useHwRandom = false;
    }
}

long random(long howbig) {
    if (howbig <= 0) {
        return 0;
    }
    uint32_t v = useHwRandom ? esp_random() : (uint32_t)newlibRand();
    return v % howbig;
}

long random(long howsmall, long howbig) {
    if (howsmall >= howbig) {
        return howsmall;
    }
    return random(howbig - howsmall) + howsmall;
}

// ---------------------------------------------------------------- memory

namespace {
std::string settingsPath;
}

std::map<std::string, std::string> &Preferences::values() {
    static std::map<std::string, std::string> v;
    return v;
}

void Preferences::load(const std::string &path) {
    settingsPath = path;
    FILE *f = fopen(path.c_str(), "r");
    if (!f) {
        return;
    }
    char line[512];
    while (fgets(line, sizeof line, f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
            s.pop_back();
        }
        size_t eq = s.find('=');
        if (eq != std::string::npos) {
            values()[s.substr(0, eq)] = s.substr(eq + 1);
        }
    }
    fclose(f);
}

// Written to a temporary file then renamed, so a crash never leaves half a
// file; readable by its owner only (Wi-Fi password)
void Preferences::put(const char *k, const std::string &v) {
    values()[k] = v;
    if (settingsPath.empty()) {
        return;
    }
    std::string tmp = settingsPath + ".tmp";
    mode_t old = umask(077);
    FILE *f = fopen(tmp.c_str(), "w");
    umask(old);
    if (!f) {
        return;
    }
    for (const auto &e : values()) {
        if (e.second.find('\n') == std::string::npos) {
            fprintf(f, "%s=%s\n", e.first.c_str(), e.second.c_str());
        }
    }
    fclose(f);
    rename(tmp.c_str(), settingsPath.c_str());
}

// ---------------------------------------------------------------- picture

namespace {
std::atomic<int> lastLanguage(0);
std::mutex pictureLock;
uint16_t picture[display::W * display::H];
int pictureCount = 0;
}  // namespace

namespace display {
void publish() {
    std::lock_guard<std::mutex> g(pictureLock);
    copy(picture);
    lastLanguage = bridge::language();
    osk::draw(picture);
    pictureCount++;
}
}  // namespace display

// ---------------------------------------------------------------- display thread

namespace game {

void start(const std::string &settingsFile) {
    static bool started = false;
    if (started) {
        return;
    }
    started = true;
    Preferences::load(settingsFile);
    std::thread([] {
        setup();
        for (;;) {
            loop();
        }
    }).detach();
}

int picture(uint16_t *dst, int seen) {
    std::lock_guard<std::mutex> g(pictureLock);
    if (pictureCount != seen) {
        memcpy(dst, ::picture, sizeof ::picture);
    }
    return pictureCount;
}

int language() {
    return lastLanguage;
}

}  // namespace game
