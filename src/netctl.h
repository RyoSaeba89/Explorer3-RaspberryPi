// The Raspberry Pi's network, driven through NetworkManager (nmcli), as the
// Cardputer drives its own Wi-Fi (user's choice: "like the Cardputer"): the
// game lists the Wi-Fi networks, joins one with its password, or creates the
// Explorer3 network (access point, 192.168.4.1, captive portal). A network
// the Pi is already on (Ethernet, or a Wi-Fi set up in Raspberry Pi OS) is
// used as it is.
//
// Every nmcli call runs without a shell (names and passwords are passed as
// plain arguments) and, except the access point, on its own thread: the game
// never waits for it. Any thread.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace netctl {

constexpr const char *AP_NAME = "Explorer3";      // network name, password and connection name
constexpr const char *AP_PASS = "Explorer3";
constexpr const char *AP_ADDRESS = "192.168.4.1";  // as the Cardputer's
constexpr const char *ETHERNET = "Ethernet";       // name shown for a wired connection

// Starts the status thread (every 2 s: connection name and address)
void init();

struct Status {
    std::string name;   // Wi-Fi name, "Ethernet", or empty when offline
    std::string ip;     // "192.168.1.42", empty when offline
    bool hotspot = false;  // the Explorer3 network is up (name = Explorer3, ip = 192.168.4.1)
};
Status status();        // latest known
Status refresh();       // asks now (a few ms; not on the display thread)

// ---- Wi-Fi list: the networks NetworkManager already knows of first, then
// those of a new search (two lists, like the Cardputer's two passes)
struct Net {
    std::string ssid;
    int rssi;   // dBm, estimated from NetworkManager's 0..100 signal
    bool open;
};
enum class Scan { Running, Partial, Done, Failed };
void startScan();  // also takes down the Explorer3 network
Scan pollScan(std::vector<Net> &out);

// ---- joining a Wi-Fi
enum class Link { Connecting, Connected, BadPassword, NotFound, NoAnswer };
void connect(const std::string &ssid, const std::string &pass);
Link link();     // BadPassword as soon as NetworkManager refuses the password
Link failure();  // why it failed (NotFound or NoAnswer), once it has

// ---- the Explorer3 network (blocking, a few seconds: busy() meanwhile)
bool startHotspot();
bool hotspot();
int hotspotClients();  // devices joined
bool busy();           // the network is being set up: the screen says so

// ---- explorer3.local (Avahi), announced for the current address
void announce(const std::string &ip);

}  // namespace netctl
