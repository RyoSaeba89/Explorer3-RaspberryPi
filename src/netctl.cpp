// See netctl.h.
#include "netctl.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

extern char **environ;

namespace netctl {
namespace {

// ---------------------------------------------------------------- commands

struct Result {
    int code = -1;    // exit code; -1: could not start, -2: too slow (killed)
    std::string out;  // standard output and error together
};

// The environment of every command: ours, with messages in English (they are
// parsed) and no pager
std::vector<std::string> commandEnv() {
    std::vector<std::string> env;
    for (char **e = environ; *e; e++) {
        if (strncmp(*e, "LC_ALL=", 7) != 0 && strncmp(*e, "LANG=", 5) != 0) {
            env.push_back(*e);
        }
    }
    env.push_back("LC_ALL=C");
    env.push_back("LANG=C");
    return env;
}

// Runs a program without a shell, waits at most timeoutMs
Result run(const std::vector<std::string> &args, int timeoutMs) {
    Result r;
    std::vector<std::string> envStrings = commandEnv();
    std::vector<char *> argv, envp;
    for (const std::string &a : args) argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(nullptr);
    for (const std::string &e : envStrings) envp.push_back(const_cast<char *>(e.c_str()));
    envp.push_back(nullptr);

    int fds[2];
    if (pipe2(fds, O_CLOEXEC) < 0) {
        return r;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return r;
    }
    if (pid == 0) {  // child: only async-signal-safe calls until exec
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        dup2(fds[1], 1);
        dup2(fds[1], 2);
        int null = open("/dev/null", O_RDONLY);
        if (null >= 0) dup2(null, 0);
        execvpe(argv[0], argv.data(), envp.data());
        _exit(127);
    }
    close(fds[1]);
    auto start = std::chrono::steady_clock::now();
    char buf[4096];
    for (;;) {
        int left = timeoutMs - (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - start).count();
        if (left <= 0) {
            kill(pid, SIGKILL);
            r.code = -2;
            break;
        }
        pollfd p = {fds[0], POLLIN, 0};
        int n = poll(&p, 1, left);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) continue;
        ssize_t got = read(fds[0], buf, sizeof buf);
        if (got <= 0) break;  // end of output: the program is ending
        if (r.out.size() < 256 * 1024) r.out.append(buf, got);
    }
    close(fds[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (r.code != -2) {
        r.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (r.code == 127) r.code = -1;  // program not found
    }
    return r;
}

// One line of "nmcli -t" output: fields split on ':' ("\:" is a colon in a value)
std::vector<std::string> fields(const std::string &line) {
    std::vector<std::string> f(1);
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '\\' && i + 1 < line.size()) {
            f.back() += line[++i];
        } else if (c == ':') {
            f.emplace_back();
        } else {
            f.back() += c;
        }
    }
    return f;
}

std::vector<std::string> lines(const std::string &s) {
    std::vector<std::string> v;
    size_t p = 0;
    while (p < s.size()) {
        size_t e = s.find('\n', p);
        if (e == std::string::npos) e = s.size();
        if (e > p) v.push_back(s.substr(p, e - p));
        p = e + 1;
    }
    return v;
}

bool contains(const std::string &s, const char *what) {
    return s.find(what) != std::string::npos;
}

// IPv4 address of an interface, empty if none
std::string addressOf(const std::string &ifname) {
    std::string ip;
    ifaddrs *list = nullptr;
    if (getifaddrs(&list) < 0) {
        return ip;
    }
    for (ifaddrs *a = list; a; a = a->ifa_next) {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET || !(a->ifa_flags & IFF_UP)) continue;
        if (ifname.empty() ? (a->ifa_flags & IFF_LOOPBACK) != 0 : ifname != a->ifa_name) continue;
        char s[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((sockaddr_in *)a->ifa_addr)->sin_addr, s, sizeof s);
        ip = s;
        break;
    }
    freeifaddrs(list);
    return ip;
}

// ---------------------------------------------------------------- state

std::mutex lock;  // everything below
Status current;
std::string wifiDevice;  // "wlan0", empty: no Wi-Fi
int stations = 0;        // devices joined to the Explorer3 network
bool hasNmcli = true;
std::atomic<bool> settingUp(false);

// Scan
Scan scanState = Scan::Done;
std::vector<Net> scanFound;
int scanGeneration = 0;

// Connection
Link linkState = Link::Connecting;
Link lastFailure = Link::NoAnswer;
int connectGeneration = 0;

// explorer3.local
pid_t avahiPid = -1;
std::string announced;

Status readStatus() {
    Status s;
    std::string wifi;
    Result r = run({"nmcli", "-t", "-f", "DEVICE,TYPE,STATE,CONNECTION", "device"}, 5000);
    if (r.code == -1) {  // no NetworkManager: the first address found, as it is
        std::lock_guard<std::mutex> g(lock);
        hasNmcli = false;
        s.ip = addressOf("");
        s.name = s.ip.empty() ? "" : ETHERNET;
        return s;
    }
    std::string ethDev, wifiConn;
    for (const std::string &l : lines(r.out)) {
        std::vector<std::string> f = fields(l);
        if (f.size() < 4) continue;
        bool up = f[2].compare(0, 9, "connected") == 0;
        if (f[1] == "wifi" && wifi.empty()) {
            wifi = f[0];
            if (up) wifiConn = f[3];
        } else if (f[1] == "ethernet" && up && ethDev.empty()) {
            ethDev = f[0];
        }
    }
    if (!wifi.empty() && wifiConn == AP_NAME) {
        s.hotspot = true;
        s.name = AP_NAME;
        s.ip = addressOf(wifi);
    } else if (!ethDev.empty() && !addressOf(ethDev).empty()) {
        s.name = ETHERNET;
        s.ip = addressOf(ethDev);
    } else if (!wifiConn.empty()) {
        s.name = wifiConn;
        s.ip = addressOf(wifi);
    }
    if (s.ip.empty()) {
        s.name.clear();
        s.hotspot = false;
    }
    std::lock_guard<std::mutex> g(lock);
    wifiDevice = wifi;
    return s;
}

int countStations(const std::string &dev) {
    Result r = run({"iw", "dev", dev, "station", "dump"}, 3000);
    int n = 0;
    for (const std::string &l : lines(r.out)) {
        n += l.compare(0, 8, "Station ") == 0 ? 1 : 0;
    }
    return n;
}

void statusLoop() {
    for (;;) {
        Status s = readStatus();
        int n = 0;
        std::string dev;
        {
            std::lock_guard<std::mutex> g(lock);
            dev = wifiDevice;
        }
        if (s.hotspot && !dev.empty()) {
            n = countStations(dev);
        }
        {
            std::lock_guard<std::mutex> g(lock);
            current = s;
            stations = n;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(s.hotspot ? 1000 : 2000));
    }
}

std::string device() {
    std::lock_guard<std::mutex> g(lock);
    return wifiDevice;
}

void stopHotspot() {
    run({"nmcli", "connection", "down", "id", AP_NAME}, 10000);
}

// NetworkManager's signal (0..100) as an RSSI for the game's bars
int rssiOf(int signal) {
    return signal / 2 - 100;
}

// nmcli device wifi list: SSID, SIGNAL, SECURITY
std::vector<Net> listNetworks(bool rescan) {
    std::vector<Net> nets;
    std::vector<std::string> args = {"nmcli", "-t", "-f", "SSID,SIGNAL,SECURITY", "device", "wifi", "list"};
    std::string dev = device();
    if (!dev.empty()) {
        args.push_back("ifname");
        args.push_back(dev);
    }
    args.push_back("--rescan");
    args.push_back(rescan ? "yes" : "no");
    Result r = run(args, 30000);
    if (r.code != 0) {
        throw r.code;
    }
    for (const std::string &l : lines(r.out)) {
        std::vector<std::string> f = fields(l);
        if (f.size() < 3 || f[0].empty() || f[0] == AP_NAME) continue;
        int rssi = rssiOf(atoi(f[1].c_str()));
        bool open = f[2].empty() || f[2] == "--";
        auto it = std::find_if(nets.begin(), nets.end(), [&](const Net &n) { return n.ssid == f[0]; });
        if (it != nets.end()) {
            it->rssi = std::max(it->rssi, rssi);
        } else {
            nets.push_back({f[0], rssi, open});
        }
    }
    std::sort(nets.begin(), nets.end(), [](const Net &a, const Net &b) { return a.rssi > b.rssi; });
    return nets;
}

void scanTask(int gen) {
    auto publish = [&](Scan s, const std::vector<Net> *nets) {
        std::lock_guard<std::mutex> g(lock);
        if (gen != scanGeneration) return false;
        scanState = s;
        if (nets) scanFound = *nets;
        return true;
    };
    if (readStatus().hotspot) {
        stopHotspot();
    }
    run({"nmcli", "radio", "wifi", "on"}, 5000);
    bool first = false;
    try {
        std::vector<Net> nets = listNetworks(false);  // what NetworkManager already knows: at once
        if (!nets.empty()) {
            first = true;
            if (!publish(Scan::Partial, &nets)) return;
        }
    } catch (int) {
    }
    try {
        std::vector<Net> nets = listNetworks(true);  // a new search: a few seconds
        publish(Scan::Done, &nets);
    } catch (int) {
        publish(first ? Scan::Done : Scan::Failed, nullptr);
    }
}

void setLink(int gen, Link l) {
    std::lock_guard<std::mutex> g(lock);
    if (gen != connectGeneration) return;
    linkState = l;
    if (l != Link::Connecting && l != Link::Connected) {
        lastFailure = l == Link::BadPassword ? Link::NoAnswer : l;
    }
}

Link reasonOf(const Result &r) {
    if (contains(r.out, "Secrets were required") || contains(r.out, "psk: property is invalid")) {
        return Link::BadPassword;
    }
    if (contains(r.out, "No network with SSID")) {
        return Link::NotFound;
    }
    return Link::NoAnswer;
}

void connectTask(int gen, std::string ssid, std::string pass) {
    Status s = readStatus();
    if (!s.hotspot && s.name == ssid && !s.ip.empty()) {
        setLink(gen, Link::Connected);  // already on it (Raspberry Pi OS's own setting, or Ethernet)
        return;
    }
    if (s.hotspot) {
        stopHotspot();
    }
    if (ssid == ETHERNET) {  // a cable that is no longer there
        setLink(gen, Link::NotFound);
        return;
    }
    // A profile made by this attempt is removed if it fails (wrong password...)
    bool known = false;
    for (const std::string &l : lines(run({"nmcli", "-t", "-f", "NAME", "connection", "show"}, 5000).out)) {
        known = known || fields(l)[0] == ssid;
    }
    std::vector<std::string> args = {"nmcli", "--wait", "18", "device", "wifi", "connect", ssid};
    if (!pass.empty()) {
        args.push_back("password");
        args.push_back(pass);
    }
    std::string dev = device();
    if (!dev.empty()) {
        args.push_back("ifname");
        args.push_back(dev);
    }
    Result r = run(args, 25000);
    Link why = reasonOf(r);
    if (r.code != 0 && why == Link::NotFound) {
        args.push_back("hidden");  // "Other network": a hidden one
        args.push_back("yes");
        r = run(args, 25000);
        why = reasonOf(r);
    }
    if (r.code != 0) {
        if (!known) {
            run({"nmcli", "connection", "delete", "id", ssid}, 10000);
        }
        setLink(gen, why);
        return;
    }
    for (int i = 0; i < 20; i++) {  // connected: wait for its address
        Status now = readStatus();
        if (!now.ip.empty()) {
            std::lock_guard<std::mutex> g(lock);
            current = now;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    setLink(gen, Link::Connected);
}

}  // namespace

// ---------------------------------------------------------------- status

void init() {
    static bool started = false;
    if (started) return;
    started = true;
    Status s = readStatus();
    {
        std::lock_guard<std::mutex> g(lock);
        current = s;
    }
    std::thread(statusLoop).detach();
}

Status status() {
    std::lock_guard<std::mutex> g(lock);
    return current;
}

Status refresh() {
    Status s = readStatus();
    std::lock_guard<std::mutex> g(lock);
    current = s;
    return s;
}

// ---------------------------------------------------------------- scan

void startScan() {
    int gen;
    {
        std::lock_guard<std::mutex> g(lock);
        gen = ++scanGeneration;
        scanState = Scan::Running;
        scanFound.clear();
        if (!hasNmcli) {
            scanState = Scan::Failed;
            return;
        }
    }
    std::thread(scanTask, gen).detach();
}

Scan pollScan(std::vector<Net> &out) {
    std::lock_guard<std::mutex> g(lock);
    out = scanFound;
    return scanState;
}

// ---------------------------------------------------------------- connection

void connect(const std::string &ssid, const std::string &pass) {
    int gen;
    {
        std::lock_guard<std::mutex> g(lock);
        gen = ++connectGeneration;
        linkState = Link::Connecting;
        lastFailure = Link::NoAnswer;
    }
    std::thread(connectTask, gen, ssid, pass).detach();
}

Link link() {
    std::lock_guard<std::mutex> g(lock);
    if (linkState == Link::BadPassword) return Link::BadPassword;
    return linkState == Link::Connected ? Link::Connected : Link::Connecting;
}

Link failure() {
    std::lock_guard<std::mutex> g(lock);
    return lastFailure;
}

// ---------------------------------------------------------------- Explorer3 network

bool startHotspot() {
    settingUp = true;
    std::string dev = device();
    bool ok = false;
    if (!dev.empty()) {
        run({"nmcli", "radio", "wifi", "on"}, 5000);
        run({"nmcli", "connection", "delete", "id", AP_NAME}, 10000);
        // In memory only (save no): nothing stays in Raspberry Pi OS's settings.
        // Shared IPv4: NetworkManager's dnsmasq gives addresses and answers every
        // name with ours (packaging/dnsmasq-explorer3.conf), the captive portal.
        Result add = run({"nmcli", "connection", "add", "save", "no", "type", "wifi", "ifname", dev, "con-name",
                          AP_NAME, "autoconnect", "no", "ssid", AP_NAME, "802-11-wireless.mode", "ap",
                          "802-11-wireless.band", "bg", "ipv4.method", "shared", "ipv4.addresses",
                          std::string(AP_ADDRESS) + "/24", "ipv6.method", "disabled", "wifi-sec.key-mgmt",
                          "wpa-psk", "wifi-sec.proto", "rsn", "wifi-sec.pairwise", "ccmp", "wifi-sec.group",
                          "ccmp", "wifi-sec.psk", AP_PASS},
                         15000);
        if (add.code == 0) {
            ok = run({"nmcli", "--wait", "20", "connection", "up", "id", AP_NAME}, 25000).code == 0;
        }
    }
    if (ok) {
        refresh();
    }
    settingUp = false;
    return ok;
}

bool hotspot() {
    std::lock_guard<std::mutex> g(lock);
    return current.hotspot;
}

int hotspotClients() {
    std::lock_guard<std::mutex> g(lock);
    return current.hotspot ? stations : 0;
}

bool busy() {
    return settingUp;
}

// ---------------------------------------------------------------- explorer3.local

void announce(const std::string &ip) {
    std::lock_guard<std::mutex> g(lock);
    if (ip == announced) return;
    announced = ip;
    if (avahiPid > 0) {
        kill(avahiPid, SIGTERM);
        waitpid(avahiPid, nullptr, 0);
        avahiPid = -1;
    }
    if (ip.empty()) return;
    // avahi-publish keeps the name for as long as it runs; it ends with us
    std::string name = "explorer3.local";
    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        int null = open("/dev/null", O_RDWR);
        if (null >= 0) {
            dup2(null, 0);
            dup2(null, 1);
            dup2(null, 2);
        }
        execlp("avahi-publish", "avahi-publish", "-a", "-R", name.c_str(), ip.c_str(), (char *)nullptr);
        _exit(127);
    }
    avahiPid = pid;
}

}  // namespace netctl
