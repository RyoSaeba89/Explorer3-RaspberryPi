// Raspberry Pi: the Cardputer's NVS memory (language, best time, Wi-Fi) kept
// in a "key=value" file, ~/.config/explorer3/settings.txt (readable by its
// owner only: it holds the Wi-Fi password, as the Cardputer's memory does),
// rewritten on every change. Same interface as the simulator's Preferences.h.
//
// A network the Pi is already on (Ethernet, or a Wi-Fi set up in Raspberry
// Pi OS) counts as the remembered one: "Multiplayer" goes straight to it, as
// the Cardputer does with its own remembered Wi-Fi.
#pragma once
#include <Arduino.h>

#include <map>

#include "netctl.h"

class Preferences {
public:
    bool begin(const char *, bool) { return true; }
    bool isKey(const char *k) { return values().count(k) > 0; }
    String getString(const char *k, const String &def) {
        std::string key(k);
        if (key == "ssid") {
            netctl::Status s = netctl::status();
            if (!s.name.empty() && !s.hotspot) return String(s.name);
            if (isKey(k) && values()[k] == netctl::ETHERNET) return String();  // cable gone
        }
        return isKey(k) ? String(values()[k]) : def;
    }
    void putString(const char *k, const String &v) { put(k, v.c_str()); }
    uint32_t getUInt(const char *k, uint32_t def) { return isKey(k) ? (uint32_t)std::stoul(values()[k]) : def; }
    void putUInt(const char *k, uint32_t v) { put(k, std::to_string(v)); }
    uint8_t getUChar(const char *k, uint8_t def) { return isKey(k) ? (uint8_t)std::stoi(values()[k]) : def; }
    void putUChar(const char *k, uint8_t v) { put(k, std::to_string(v)); }
    bool getBool(const char *k, bool def) {
        if (std::string(k) == "auto") {
            netctl::Status s = netctl::status();
            if (!s.name.empty() && !s.hotspot) return true;
        }
        return isKey(k) ? values()[k] == "1" : def;
    }
    void putBool(const char *k, bool v) { put(k, v ? "1" : "0"); }

    // game.cpp: read at start-up, before setup()
    static void load(const std::string &path);

private:
    static std::map<std::string, std::string> &values();
    static void put(const char *k, const std::string &v);
};
