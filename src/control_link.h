// Between the game's mission control role (its role menu, then
// src/controle.h) and the window (main.cpp): once the network is up the
// game asks for the Pi's own control screen (src/control/), and comes back
// to its role menu when that screen closes. Any thread.
#pragma once

namespace control_link {

void request();     // game thread: show mission control
bool requested();   // main thread: true once per request
void finish();      // main thread: mission control closed
bool finished();    // game thread: true once per finish

}  // namespace control_link
