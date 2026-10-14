// Raspberry Pi: the game's network side (mirror::, diffusion.h), the same
// protocol as the Cardputer's diffusion.cpp so that mission control (a web
// browser, an Android device or another Pi) works unchanged.
// Ports as on the Cardputer (user's choice), granted at install time
// (setcap cap_net_bind_service, see packaging/):
//   http://<address>/      mission control's page (the Cardputer's page,
//                          copied into page.inc by tools/prepare.sh)
//   ws://<address>:81/     the stream: changed screen rows, sounds, pages
// One network thread serves the stream; each page request gets a short-lived
// thread of its own. The Wi-Fi side is NetworkManager's (netctl.h).
#include <diffusion.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#include "display.h"
#include "netctl.h"
#include "sha1.h"

// The Cardputer's page: const char PAGE[] (made by tools/prepare.sh)
#include "page.inc"

namespace mirror {
namespace {

constexpr int HTTP_PORT = 80;
constexpr int WS_PORT = 81;
constexpr int MAX_CLIENTS = 5;            // as on the Cardputer
constexpr int MAX_PAGE_THREADS = 8;       // page requests served at once
constexpr uint32_t FRAME_MS = 40;         // 25 pictures/s at most
constexpr uint32_t CLOCK_MS = 250;        // game clock for the sound
constexpr uint32_t PING_MS = 5000;        // heartbeat, as on the Cardputer
constexpr int MISSED_PONGS = 2;
constexpr uint32_t HANDSHAKE_MS = 10000;  // to ask for the stream once connected
constexpr size_t FRAME_BUF = 12 * 1024;   // a picture may go in several messages
constexpr int MAX_H = 135;

struct Client {
    int fd = -1;
    bool open = false;  // handshake done
    std::string in;     // bytes received, not yet used
    int missed = 0;     // pings without a pong
    uint32_t since = 0; // connected at (millis)
};

std::mutex screenMutex;   // the game's canvas, drawn by the game thread
std::mutex stateLock;     // everything below that the game thread sets
const uint16_t *screenBuf = nullptr;
int screenW = 0;
int screenH = 0;
std::deque<std::string> soundMsgs;  // "T,...", "S,...", "R,..." without the current time
std::string panelText = "0";
bool panelDirty = false;
std::string bannerText;
int bannerY = 0;
int bannerH = 0;
bool bannerDirty = false;
std::string mazeText;  // maze theme ("M"), empty: the planets
bool mazeDirty = false;
uint8_t pageLang = 0;
bool langDirty = false;
std::string symbolsJs = "const SYM=[];";
std::atomic<int> clients(0);
std::atomic<int> pageThreads(0);

// Server on / off, asked by the game thread, done by the
// network thread (it alone touches the listening sockets)
std::mutex requestLock;
std::condition_variable requestDone;
enum class Request { None, Open, Close };
Request request = Request::None;
int requestId = 0;
int doneId = 0;
bool openResult = false;
bool netThreadStarted = false;

// Network thread only
int httpFd = -1;
int wsFd = -1;
Client conns[MAX_CLIENTS];
uint32_t rowHash[MAX_H];  // 0 = row to send again
int nextRow = 0;
int bandY = 0;
int bandH = 0;
uint8_t frameBuf[FRAME_BUF];

int listenOn(int port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return -1;
    }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (sockaddr *)&a, sizeof a) < 0 || listen(fd, 16) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool sendAll(int fd, const void *data, size_t len) {
    const char *p = static_cast<const char *>(data);
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n <= 0) {
            return false;
        }
        p += n;
        len -= n;
    }
    return true;
}

std::string header(const std::string &req, const char *name) {
    std::string lower = req;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    std::string key = std::string("\r\n") + name + ":";
    size_t p = lower.find(key);
    if (p == std::string::npos) {
        return "";
    }
    p += key.size();
    size_t e = req.find("\r\n", p);
    std::string v = req.substr(p, e - p);
    v.erase(0, v.find_first_not_of(' '));
    v.erase(v.find_last_not_of(' ') + 1);
    return v;
}

// ---- HTTP: the page and /sym.js, one request per connection, own thread

// Captive portal (Explorer3 network): any name other than ours leads to the
// page, so the phone's "sign in to network" window opens it (as on the
// Cardputer)
bool redirect(const std::string &req, std::string &location) {
    if (!netctl::hotspot()) {
        return false;
    }
    std::string host = header(req, "host");
    size_t colon = host.find(':');
    if (colon != std::string::npos) {
        host.erase(colon);
    }
    std::transform(host.begin(), host.end(), host.begin(), ::tolower);
    if (host == netctl::AP_ADDRESS || host == HOST_NAME) {
        return false;
    }
    location = std::string("http://") + netctl::AP_ADDRESS + "/";
    return true;
}

void serveHttp(int fd) {
    timeval tv = {2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    std::string req;
    char buf[1024];
    while (req.find("\r\n\r\n") == std::string::npos && req.size() < 8192) {
        ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n <= 0) {
            break;
        }
        req.append(buf, n);
    }
    std::string path;
    size_t sp = req.find(' ');
    if (sp != std::string::npos) {
        size_t sp2 = req.find(' ', sp + 1);
        path = req.substr(sp + 1, sp2 == std::string::npos ? std::string::npos : sp2 - sp - 1);
    }
    size_t q = path.find('?');
    if (q != std::string::npos) {
        path.erase(q);
    }
    std::string type = "text/plain";
    std::string body = "404";
    std::string extra;
    const char *status = "404 Not Found";
    std::string location;
    if (path == "/sym.js") {
        type = "application/javascript";
        std::lock_guard<std::mutex> g(stateLock);
        body = symbolsJs;
        status = "200 OK";
    } else if (redirect(req, location)) {
        status = "302 Found";
        body.clear();
        extra = "Location: " + location + "\r\n";
    } else if (path == "/") {
        type = "text/html; charset=utf-8";
        body = PAGE;
        status = "200 OK";
    }
    std::string head = std::string("HTTP/1.1 ") + status + "\r\nContent-Type: " + type +
                       "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + extra +
                       "Cache-Control: no-store\r\nConnection: close\r\n\r\n";
    sendAll(fd, head.data(), head.size());
    sendAll(fd, body.data(), body.size());
    close(fd);
    pageThreads--;
}

// ---- WebSocket

bool sendFrame(Client &c, int opcode, const void *data, size_t len) {
    uint8_t head[4];
    size_t h = 2;
    head[0] = 0x80 | opcode;
    if (len < 126) {
        head[1] = (uint8_t)len;
    } else {
        head[1] = 126;  // payloads stay below 64 KB
        head[2] = (uint8_t)(len >> 8);
        head[3] = (uint8_t)len;
        h = 4;
    }
    return sendAll(c.fd, head, h) && sendAll(c.fd, data, len);
}

void countClients() {
    int n = 0;
    for (const Client &k : conns) {
        n += k.open ? 1 : 0;
    }
    clients = n;
}

void dropClient(Client &c) {
    if (c.fd >= 0) {
        close(c.fd);
    }
    c = Client();
    countClients();
}

void broadcast(int opcode, const void *data, size_t len) {
    for (Client &c : conns) {
        if (c.open && !sendFrame(c, opcode, data, len)) {
            dropClient(c);
        }
    }
}

void broadcastText(const std::string &s) {
    broadcast(1, s.data(), s.size());
}

// A new browser: the whole screen, its page, its language, the maze theme and the banner
void welcome() {
    memset(rowHash, 0, sizeof rowHash);
    std::lock_guard<std::mutex> g(stateLock);
    panelDirty = langDirty = bannerDirty = mazeDirty = true;
}

bool handshake(Client &c) {
    size_t end = c.in.find("\r\n\r\n");
    if (end == std::string::npos) {
        return c.in.size() < 8192;  // wait for the rest
    }
    std::string req = c.in.substr(0, end + 2);
    c.in.erase(0, end + 4);
    std::string key = header(req, "sec-websocket-key");
    if (key.empty()) {
        return false;
    }
    std::string accept = sha1::base64(sha1::digest(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
    std::string resp = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    if (!sendAll(c.fd, resp.data(), resp.size())) {
        return false;
    }
    c.open = true;
    countClients();
    welcome();
    return true;
}

// Frames from the browser: answers pings, notes pongs, closes on request
bool readFrames(Client &c) {
    for (;;) {
        if (c.in.size() < 2) {
            return true;
        }
        const uint8_t *b = reinterpret_cast<const uint8_t *>(c.in.data());
        int opcode = b[0] & 0x0F;
        bool masked = b[1] & 0x80;
        uint64_t len = b[1] & 0x7F;
        size_t pos = 2;
        if (len == 126) {
            if (c.in.size() < 4) return true;
            len = (b[2] << 8) | b[3];
            pos = 4;
        } else if (len == 127) {
            return false;  // never sent by mission control
        }
        size_t need = pos + (masked ? 4 : 0) + len;
        if (c.in.size() < need) {
            return need < 65536;
        }
        std::string payload = c.in.substr(pos + (masked ? 4 : 0), len);
        if (masked) {
            for (size_t i = 0; i < payload.size(); i++) {
                payload[i] ^= b[pos + (i & 3)];
            }
        }
        c.in.erase(0, need);
        if (opcode == 8) {
            return false;
        } else if (opcode == 9) {
            if (!sendFrame(c, 10, payload.data(), payload.size())) return false;
        } else if (opcode == 10) {
            c.missed = 0;
        }
    }
}

// ---- Screen rows, as the Cardputer sends them

uint32_t hashRow(const uint16_t *p) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < screenW; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h | 1;
}

// Row y raw or run-length encoded, whichever is shorter; 0 if it does not fit
size_t encodeRow(int y, uint8_t *out, size_t room) {
    const uint16_t *p = screenBuf + y * screenW;
    size_t rle = 2;
    for (int x = 0; x < screenW;) {
        int n = 1;
        while (x + n < screenW && n < 255 && p[x + n] == p[x]) n++;
        rle += 3;
        x += n;
    }
    size_t raw = 2 + screenW * 2;
    size_t need = std::min(rle, raw);
    if (need > room) {
        return 0;
    }
    out[0] = y;
    if (rle < raw) {
        out[1] = 1;
        uint8_t *o = out + 2;
        for (int x = 0; x < screenW;) {
            int n = 1;
            while (x + n < screenW && n < 255 && p[x + n] == p[x]) n++;
            const uint8_t *bytes = reinterpret_cast<const uint8_t *>(p + x);
            *o++ = n;
            *o++ = bytes[0];  // the sprite's byte order: high byte first
            *o++ = bytes[1];
            x += n;
        }
    } else {
        out[1] = 0;
        memcpy(out + 2, p, screenW * 2);
    }
    return need;
}

void sendScreen() {
    size_t len = 0;
    int stopAt = -1;
    {
        std::lock_guard<std::mutex> g(screenMutex);
        if (!screenBuf) {
            return;
        }
        for (int i = 0; i < screenH; i++) {
            int y = (nextRow + i) % screenH;
            if (y >= bandY && y < bandY + bandH) {
                continue;  // banner drawn by the page
            }
            uint32_t h = hashRow(screenBuf + y * screenW);
            if (h == rowHash[y]) {
                continue;
            }
            size_t n = encodeRow(y, frameBuf + len, FRAME_BUF - len);
            if (n == 0) {
                stopAt = y;
                break;
            }
            len += n;
            rowHash[y] = h;
        }
    }
    nextRow = stopAt < 0 ? 0 : stopAt;
    if (len > 0) {
        broadcast(2, frameBuf, len);
    }
}

// Pages, language, banner and sounds set by the game
void sendState(uint32_t now) {
    std::deque<std::string> sounds;
    std::string panel, banner, maze;
    bool sendPanel, sendLang, sendBanner, sendMaze;
    int by, bh;
    uint8_t lang;
    {
        std::lock_guard<std::mutex> g(stateLock);
        sounds.swap(soundMsgs);
        sendPanel = panelDirty;
        sendLang = langDirty;
        sendBanner = bannerDirty;
        sendMaze = mazeDirty;
        panelDirty = langDirty = bannerDirty = mazeDirty = false;
        panel = panelText;
        maze = mazeText;
        banner = bannerText;
        by = bannerY;
        bh = bannerH;
        lang = pageLang;
    }
    std::string t = std::to_string(now);
    for (const std::string &m : sounds) {  // "T,at,..." -> "T,now,at,..."
        broadcastText(m.substr(0, 2) + t + "," + m.substr(2));
    }
    if (sendLang) {
        broadcastText("L," + t + "," + (lang ? "en" : "fr"));
    }
    if (sendMaze) {  // before the page, which then shows the right theme at once
        broadcastText("M," + t + (maze.empty() ? "" : "," + maze));
    }
    if (sendPanel) {
        broadcastText("K," + t + "," + panel);
    }
    if (sendBanner) {
        bool on = !banner.empty();
        if (!on) {
            for (int i = bandY; i < bandY + bandH && i < MAX_H; i++) {
                rowHash[i] = 0;  // the banner's rows are sent again
            }
        }
        bandY = on ? by : 0;
        bandH = on ? bh : 0;
        broadcastText("B," + t + "," + std::to_string(by) + "," + std::to_string(bh) + "," + banner);
    }
}

void closeServer() {
    for (Client &c : conns) {
        if (c.fd >= 0) {
            dropClient(c);
        }
    }
    if (httpFd >= 0) close(httpFd);
    if (wsFd >= 0) close(wsFd);
    httpFd = wsFd = -1;
    netctl::announce("");
}

// Requests from startServer() and stopServer()
void handleRequests() {
    std::lock_guard<std::mutex> g(requestLock);
    if (request == Request::None) {
        return;
    }
    if (request == Request::Open) {
        if (httpFd < 0) httpFd = listenOn(HTTP_PORT);
        if (wsFd < 0) wsFd = listenOn(WS_PORT);
        openResult = httpFd >= 0 && wsFd >= 0;
        if (!openResult) {
            closeServer();  // both ports or nothing
        }
    } else {
        closeServer();
    }
    request = Request::None;
    doneId = requestId;
    requestDone.notify_all();
}

void netThread() {
    uint32_t nextFrame = 0, nextClock = 0, nextPing = 0, nextName = 0;
    for (;;) {
        handleRequests();
        if (httpFd < 0) {  // server off: wait for a request
            {
                std::lock_guard<std::mutex> g(stateLock);
                soundMsgs.clear();
            }
            std::unique_lock<std::mutex> g(requestLock);
            requestDone.wait_for(g, std::chrono::milliseconds(20), [] { return request != Request::None; });
            continue;
        }
        fd_set rd;
        FD_ZERO(&rd);
        int maxFd = std::max(httpFd, wsFd);
        FD_SET(httpFd, &rd);
        FD_SET(wsFd, &rd);
        for (const Client &c : conns) {
            if (c.fd >= 0) { FD_SET(c.fd, &rd); maxFd = std::max(maxFd, c.fd); }
        }
        timeval tv = {0, 5000};
        int r = select(maxFd + 1, &rd, nullptr, nullptr, &tv);
        if (r > 0) {
            if (FD_ISSET(httpFd, &rd)) {
                int fd = accept4(httpFd, nullptr, nullptr, SOCK_CLOEXEC);
                if (fd >= 0) {
                    if (pageThreads >= MAX_PAGE_THREADS) {
                        close(fd);
                    } else {
                        pageThreads++;
                        std::thread(serveHttp, fd).detach();
                    }
                }
            }
            if (FD_ISSET(wsFd, &rd)) {
                int fd = accept4(wsFd, nullptr, nullptr, SOCK_CLOEXEC);
                if (fd >= 0) {
                    Client *slot = nullptr;
                    for (Client &c : conns) {
                        if (c.fd < 0) { slot = &c; break; }
                    }
                    if (!slot) {
                        close(fd);  // 5 browsers at most
                    } else {
                        int one = 1;
                        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
                        timeval st = {3, 0};
                        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &st, sizeof st);
                        slot->fd = fd;
                        slot->since = millis();
                    }
                }
            }
            for (Client &c : conns) {
                if (c.fd < 0 || !FD_ISSET(c.fd, &rd)) {
                    continue;
                }
                char buf[2048];
                ssize_t n = recv(c.fd, buf, sizeof buf, 0);
                if (n <= 0) {
                    dropClient(c);
                    continue;
                }
                c.in.append(buf, n);
                bool ok = c.open ? readFrames(c) : handshake(c) && readFrames(c);
                if (!ok) {
                    dropClient(c);
                }
            }
        }
        uint32_t now = millis();
        for (Client &c : conns) {
            if (c.fd >= 0 && !c.open && now - c.since > HANDSHAKE_MS) {
                dropClient(c);  // connected but silent: its place goes to the next browser
            }
        }
        if ((int32_t)(now - nextName) >= 0) {  // explorer3.local follows the address
            nextName = now + 2000;
            netctl::announce(netctl::status().ip);
        }
        if (clients == 0) {
            std::lock_guard<std::mutex> g(stateLock);
            soundMsgs.clear();  // nobody to play them
            continue;
        }
        sendState(now);
        if ((int32_t)(now - nextClock) >= 0) {
            nextClock = now + CLOCK_MS;
            broadcastText("P," + std::to_string(now));
        }
        if ((int32_t)(now - nextPing) >= 0) {
            nextPing = now + PING_MS;
            for (Client &c : conns) {
                if (!c.open) continue;
                if (++c.missed > MISSED_PONGS || !sendFrame(c, 9, "", 0)) {
                    dropClient(c);  // browser gone without closing (Wi-Fi off, console off)
                }
            }
        }
        if ((int32_t)(now - nextFrame) >= 0) {
            nextFrame = now + FRAME_MS;
            sendScreen();
        }
    }
}

// Asks the network thread; waits for its answer when wait is true
bool ask(Request r, bool wait) {
    std::unique_lock<std::mutex> g(requestLock);
    if (!netThreadStarted) {
        netThreadStarted = true;
        std::thread(netThread).detach();
    }
    request = r;
    int id = ++requestId;
    requestDone.notify_all();
    if (!wait) {
        return true;
    }
    requestDone.wait_for(g, std::chrono::seconds(3), [&] { return doneId >= id; });
    return doneId >= id && openResult;
}

void queueSound(const std::string &m) {
    std::lock_guard<std::mutex> g(stateLock);
    if (clients > 0 && soundMsgs.size() < 256) {
        soundMsgs.push_back(m);
    }
}

}  // namespace

// ---------------------------------------------------------------- Wi-Fi

void startScan() {
    netctl::startScan();
}

Scan pollScan(std::vector<WifiNet> &out) {
    std::vector<netctl::Net> nets;
    netctl::Scan s = netctl::pollScan(nets);
    out.clear();
    for (const netctl::Net &n : nets) {
        out.push_back({String(n.ssid), n.rssi, n.open});
    }
    switch (s) {
        case netctl::Scan::Partial: return Scan::Partial;
        case netctl::Scan::Done: return Scan::Done;
        case netctl::Scan::Failed: return Scan::Failed;
        default: return Scan::Running;
    }
}

void connect(const String &ssid, const String &pass) {
    netctl::connect(ssid.c_str(), pass.c_str());
}

Link link() {
    switch (netctl::link()) {
        case netctl::Link::Connected: return Link::Connected;
        case netctl::Link::BadPassword: return Link::BadPassword;
        default: return Link::Connecting;
    }
}

Link failure() {
    return netctl::failure() == netctl::Link::NotFound ? Link::NotFound : Link::NoAnswer;
}

int attempt() {
    return 1;  // NetworkManager tries again by itself, without telling
}

// ---------------------------------------------------------------- Explorer3 network

bool startAccessPoint() {
    return netctl::startHotspot();
}

bool accessPoint() {
    return netctl::hotspot();
}

int apClients() {
    return netctl::hotspotClients();
}

String wifiQrText() {
    return String("WIFI:T:WPA;S:") + AP_SSID + ";P:" + AP_PASS + ";;";
}

String ipAddress() {
    return String(netctl::status().ip);
}

// ---------------------------------------------------------------- server

// The page server (crew role; as mission control the game never starts it)
bool startServer(const uint16_t *screen, int w, int h) {
    {
        std::lock_guard<std::mutex> g(screenMutex);
        screenBuf = screen;
        screenW = w;
        screenH = std::min(h, MAX_H);
    }
    return ask(Request::Open, true);
}

// No server while this Pi is mission control (bridge.inc, controle::start)
void stopServer() {
    ask(Request::Close, false);
}

int clientCount() {
    return clients;
}

// The game draws its canvas between these two calls
void lockScreen() {
    screenMutex.lock();
}

void unlockScreen() {
    screenMutex.unlock();
    display::publish();  // right after the frame is drawn
}

void sendTone(uint32_t at, uint16_t freq, uint16_t dur, uint8_t ch) {
    queueSound("T," + std::to_string(at) + "," + std::to_string(freq) + "," + std::to_string(dur) + "," +
               std::to_string(ch));
}

void sendStop(uint32_t at, uint8_t ch) {
    queueSound("S," + std::to_string(at) + "," + std::to_string(ch));
}

void sendRumble(uint32_t at) {
    queueSound("R," + std::to_string(at));
}

void setPanel(const String &text) {
    std::lock_guard<std::mutex> g(stateLock);
    panelText = text.c_str();
    panelDirty = true;
}

void setBanner(const String &text, int y, int h) {
    std::lock_guard<std::mutex> g(stateLock);
    bannerText = text.c_str();
    bannerY = y;
    bannerH = h;
    bannerDirty = true;
}

void setMazeTheme(const String &text) {
    std::lock_guard<std::mutex> g(stateLock);
    mazeText = text.c_str();
    mazeDirty = true;
}

void setLanguage(uint8_t lang) {
    std::lock_guard<std::mutex> g(stateLock);
    pageLang = lang;
    langDirty = true;
}

void setSymbols(const char *js) {
    std::lock_guard<std::mutex> g(stateLock);
    symbolsJs = js;
}

}  // namespace mirror
