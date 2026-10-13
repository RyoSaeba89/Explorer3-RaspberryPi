// See labels.h.
#include "labels.h"

#include <atomic>

namespace labels {
namespace {
std::atomic<int> current(KEYBOARD);
}

Style style() {
    return (Style)current.load();
}

void setStyle(Style s) {
    current = s;
}

const char *ok(int lang) {
    switch (style()) {
        case XBOX: return "A";
        case PLAYSTATION: return "×";
        default: return lang ? "ENTER" : "ENTRÉE";
    }
}

const char *back(int lang) {
    (void)lang;
    switch (style()) {
        case XBOX: return "B";
        case PLAYSTATION: return "○";
        default: return "ESC";
    }
}

}  // namespace labels
