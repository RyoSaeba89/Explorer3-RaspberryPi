// Explorer3-RaspberryPi: the game's mission control role opens the Pi's own
// control screen (src/control/, main.cpp) instead of the Cardputer's (the
// game's src/controle.h, which this file stands in for when the copy of
// main.cpp is built). Declared here, inside the game's namespace; defined
// in bridge.inc. See the game's controle.h for what each function does.
namespace controle {
void start();
bool key(const KeysState &ks);
bool update(uint32_t now);
void draw(uint32_t now);
void stop();
}  // namespace controle
