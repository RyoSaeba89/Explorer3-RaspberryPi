/* See net.h. */
#include "net.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
/* The Cardputer game's PC simulator (Windows): Winsock under the same names */
#include <winsock2.h>
#include <ws2tcpip.h>
#undef EINPROGRESS
#undef EAGAIN
#undef EWOULDBLOCK
#undef ETIMEDOUT
#define EINPROGRESS WSAEWOULDBLOCK
#define EAGAIN      WSAEWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#define ETIMEDOUT   WSAETIMEDOUT
#define close closesocket
#define errno_net() WSAGetLastError()
#define usleep(us) Sleep((us) / 1000)
typedef int socklen_t;
static void set_nonblocking(int s)
{
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
}
static void net_once(void)
{
    static bool done;
    if (!done) {
        WSADATA w;
        WSAStartup(MAKEWORD(2, 2), &w);
        done = true;
    }
}
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define errno_net() errno
static void set_nonblocking(int s)
{
    fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
}
static void net_once(void)
{
}
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0   /* Winsock: the sockets are non-blocking anyway */
#endif

/* --- connection ------------------------------------------------------------- */

static int fd = -1;
static bool connected;

static void addr_of(struct sockaddr_in *a, const uint8_t ip[4], int port)
{
    memset(a, 0, sizeof(*a));
    a->sin_family = AF_INET;
    a->sin_port = htons((uint16_t)port);
    memcpy(&a->sin_addr.s_addr, ip, 4);
}

int link_start(const uint8_t ip[4], int port)
{
    net_once();
    link_close();
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -errno_net();
    set_nonblocking(fd);
    struct sockaddr_in a;
    addr_of(&a, ip, port);
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0 && errno_net() != EINPROGRESS) {
        int e = errno_net();
        link_close();
        return -e;
    }
    return 0;
}

int link_poll(void)
{
    if (fd < 0)
        return -ENOTCONN;
    if (connected)
        return 1;
    fd_set wr;
    FD_ZERO(&wr);
    FD_SET(fd, &wr);
    struct timeval tv = {0, 0};
    if (select(fd + 1, NULL, &wr, NULL, &tv) <= 0)
        return 0;
    int err = 0;
    socklen_t len = sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
    if (err != 0) {
        link_close();
        return -err;
    }
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
    connected = true;
    return 1;
}

int link_read(uint8_t *buf, int cap)
{
    if (fd < 0 || !connected)
        return LINK_CLOSED;
    int n = (int)recv(fd, (char *)buf, cap, MSG_DONTWAIT);
    if (n > 0)
        return n;
    if (n == 0)
        return LINK_CLOSED;
    int e = errno_net();
    if (e == EAGAIN || e == EWOULDBLOCK || e == EINTR)
        return 0;
    return -e;
}

/* Small writes (handshake, pongs): waits up to 2 s for room */
int link_write(const void *buf, int len)
{
    const char *p = (const char *)buf;
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (len > 0) {
        int n = (int)send(fd, p, len, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (n > 0) {
            p += n;
            len -= n;
            continue;
        }
        int e = errno_net();
        if (n < 0 && e != EAGAIN && e != EWOULDBLOCK && e != EINTR)
            return -e;
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        if (t.tv_sec - t0.tv_sec > 2)
            return -ETIMEDOUT;
        usleep(2000);
    }
    return 0;
}

void link_close(void)
{
    if (fd >= 0)
        close(fd);
    fd = -1;
    connected = false;
}

/* --- search ------------------------------------------------------------------ */

/* Sockets open at once and how long they wait: the Cardputer has few
 * sockets (NET_BATCH 8), a PC or an Android device many. */
#ifndef NET_BATCH
#define NET_BATCH    96
#endif
#ifndef NET_CONNECT_MS
#define NET_CONNECT_MS 1500
#endif
#define BATCH        NET_BATCH
#define CONNECT_MS   NET_CONNECT_MS
#define CHECK_MS     2500

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int fstate = FINDER_IDLE;
static found_t found[FINDER_MAX];
static int nfound;
static int progress;
static uint8_t base[4];
static uint8_t only[4];
static bool hasOnly;
static bool again;   /* a request not yet taken by the search thread */

static long ms_since(const struct timespec *t0)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_sec - t0->tv_sec) * 1000 + (t.tv_nsec - t0->tv_nsec) / 1000000;
}

/* The game's page server answers /sym.js with its symbol drawings: that is
 * how a crew is told apart from any other device with the port open. */
static bool is_crew(const uint8_t ip[4], int wsPort)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return false;
#ifdef _WIN32
    DWORD tv = CHECK_MS;
#else
    struct timeval tv = {CHECK_MS / 1000, (CHECK_MS % 1000) * 1000};
#endif
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
    struct sockaddr_in a;
    addr_of(&a, ip, wsPort == PORT_CARDPUTER ? 80 : 8080);
    bool ok = false;
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == 0) {
        /* Host = its address: on its own Explorer3 network the Cardputer
         * redirects any other name to its page (captive portal) */
        char req[96];
        int len = snprintf(req, sizeof(req), "GET /sym.js HTTP/1.1\r\nHost: %u.%u.%u.%u\r\nConnection: close\r\n\r\n",
                           ip[0], ip[1], ip[2], ip[3]);
        if ((int)send(s, req, len, MSG_NOSIGNAL) == len) {
            static char buf[2048];   /* search thread only; small stacks (Cardputer) */
            int got = 0;
            int n;
            while (got < (int)sizeof(buf) - 1 && (n = (int)recv(s, buf + got, (int)sizeof(buf) - 1 - got, 0)) > 0)
                got += n;
            buf[got] = 0;
            ok = strstr(buf, "const SYM=") != NULL;
        }
    }
    close(s);
    return ok;
}

static void add_found(const uint8_t ip[4], int port)
{
    pthread_mutex_lock(&lock);
    if (!again && nfound < FINDER_MAX) {   /* a newer request: this result is stale */
        int i = nfound++;
        while (i > 0 && memcmp(found[i - 1].ip, ip, 4) > 0) {   /* keep the list sorted */
            found[i] = found[i - 1];
            i--;
        }
        memcpy(found[i].ip, ip, 4);
        found[i].port = port;
    }
    pthread_mutex_unlock(&lock);
}

static void *search(void *unused)
{
    (void)unused;
    static const int ports[2] = {PORT_CARDPUTER, PORT_ANDROID};
    uint8_t self[4], target[4];
    bool single = false;
    int total = 0;
    for (int start = 0;; start += BATCH) {
        /* a request made while searching (an address typed, the search
         * started again) is taken here, between two batches */
        pthread_mutex_lock(&lock);
        if (again) {
            again = false;
            memcpy(self, base, 4);
            memcpy(target, only, 4);
            single = hasOnly;
            total = single ? 2 : 254 * 2;
            start = 0;
        }
        if (start >= total) {
            fstate = FINDER_DONE;
            progress = 100;
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        pthread_mutex_unlock(&lock);
        int n = total - start < BATCH ? total - start : BATCH;
        int fds[BATCH];
        uint8_t ips[BATCH][4];
        int prt[BATCH];
        for (int i = 0; i < n; i++) {
            int idx = start + i;
            if (single) {
                memcpy(ips[i], target, 4);
            } else {
                memcpy(ips[i], self, 3);
                ips[i][3] = (uint8_t)(idx / 2 + 1);
            }
            prt[i] = ports[idx % 2];
            fds[i] = -1;
            if (!single && ips[i][3] == self[3])
                continue;   /* not ourselves */
            int s = socket(AF_INET, SOCK_STREAM, 0);
            if (s < 0)
                continue;
            set_nonblocking(s);
            struct sockaddr_in a;
            addr_of(&a, ips[i], prt[i]);
            if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0 && errno_net() != EINPROGRESS) {
                close(s);
                continue;
            }
            fds[i] = s;
        }
        struct timespec t0;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        int open[BATCH];
        memset(open, 0, sizeof(open));
        for (;;) {
            long left = CONNECT_MS - ms_since(&t0);
            fd_set wr;
            FD_ZERO(&wr);
            int maxfd = -1;
            for (int i = 0; i < n; i++) {
                if (fds[i] >= 0) {
                    FD_SET(fds[i], &wr);
                    if (fds[i] > maxfd)
                        maxfd = fds[i];
                }
            }
            if (maxfd < 0 || left <= 0)
                break;
            struct timeval tv = {left / 1000, (left % 1000) * 1000};
            if (select(maxfd + 1, NULL, &wr, NULL, &tv) <= 0)
                break;
            for (int i = 0; i < n; i++) {
                if (fds[i] < 0 || !FD_ISSET(fds[i], &wr))
                    continue;
                int err = 0;
                socklen_t len = sizeof(err);
                getsockopt(fds[i], SOL_SOCKET, SO_ERROR, (char *)&err, &len);
                open[i] = err == 0;
                close(fds[i]);
                fds[i] = -1;
            }
        }
        for (int i = 0; i < n; i++) {
            if (fds[i] >= 0)
                close(fds[i]);
        }
        for (int i = 0; i < n; i++) {
            if (open[i] && is_crew(ips[i], prt[i]))
                add_found(ips[i], prt[i]);
        }
        pthread_mutex_lock(&lock);
        if (!again)
            progress = (start + n) * 100 / total;
        pthread_mutex_unlock(&lock);
    }
}

/* During a search, the new request replaces it at its next batch (a second
 * thread would need more sockets than the Cardputer has). */
void finder_start(const uint8_t self[4], const uint8_t *ip)
{
    net_once();
    pthread_mutex_lock(&lock);
    bool running = fstate == FINDER_RUNNING;
    fstate = FINDER_RUNNING;
    nfound = 0;
    progress = 0;
    memcpy(base, self, 4);
    hasOnly = ip != NULL;
    if (ip)
        memcpy(only, ip, 4);
    again = true;
    pthread_mutex_unlock(&lock);
    if (running)
        return;
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 16384);   /* small default stacks (Cardputer) */
    pthread_create(&t, &attr, search, NULL);
    pthread_attr_destroy(&attr);
    pthread_detach(t);
}

int finder_poll(found_t *out, int *count)
{
    pthread_mutex_lock(&lock);
    memcpy(out, found, sizeof(found_t) * (size_t)nfound);
    *count = nfound;
    int s = fstate;
    pthread_mutex_unlock(&lock);
    return s;
}

int finder_progress(void)
{
    pthread_mutex_lock(&lock);
    int p = progress;
    pthread_mutex_unlock(&lock);
    return p;
}
