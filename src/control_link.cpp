// See control_link.h.
#include "control_link.h"

#include <atomic>

namespace {
std::atomic<bool> wanted(false);
std::atomic<bool> done(false);
}  // namespace

namespace control_link {

void request() {
    done = false;
    wanted = true;
}

bool requested() {
    return wanted.exchange(false);
}

void finish() {
    done = true;
}

bool finished() {
    return done.exchange(false);
}

}  // namespace control_link
