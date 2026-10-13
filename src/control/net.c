/* See net.h. */
#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

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
    link_close();
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -errno;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    struct sockaddr_in a;
    addr_of(&a, ip, port);
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0 && errno != EINPROGRESS) {
        int e = errno;
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
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    if (err != 0) {
        link_close();
        return -err;
    }
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    connected = true;
    return 1;
}

int link_read(uint8_t *buf, int cap)
{
    if (fd < 0 || !connected)
        return LINK_CLOSED;
    ssize_t n = recv(fd, buf, (size_t)cap, MSG_DONTWAIT);
    if (n > 0)
        return (int)n;
    if (n == 0)
        return LINK_CLOSED;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return 0;
    return -errno;
}

/* Small writes (handshake, pongs): waits up to 2 s for room */
int link_write(const void *buf, int len)
{
    const char *p = (const char *)buf;
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (len > 0) {
        ssize_t n = send(fd, p, (size_t)len, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (n > 0) {
            p += n;
            len -= (int)n;
            continue;
        }
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            return -errno;
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

#define BATCH        96     /* sockets open at once */
#define CONNECT_MS   1500
#define CHECK_MS     2500

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int fstate = FINDER_IDLE;
static found_t found[FINDER_MAX];
static int nfound;
static int progress;
static uint8_t base[4];
static uint8_t only[4];
static bool hasOnly;

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
    struct timeval tv = {CHECK_MS / 1000, (CHECK_MS % 1000) * 1000};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    struct sockaddr_in a;
    addr_of(&a, ip, wsPort == PORT_CARDPUTER ? 80 : 8080);
    bool ok = false;
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == 0) {
        /* Host = its address: on its own Explorer3 network the Cardputer
         * redirects any other name to its page (captive portal) */
        char req[96];
        int len = snprintf(req, sizeof(req), "GET /sym.js HTTP/1.1\r\nHost: %u.%u.%u.%u\r\nConnection: close\r\n\r\n",
                           ip[0], ip[1], ip[2], ip[3]);
        if (send(s, req, (size_t)len, MSG_NOSIGNAL) == (ssize_t)len) {
            char buf[2048];
            int got = 0;
            ssize_t n;
            while (got < (int)sizeof(buf) - 1 && (n = recv(s, buf + got, sizeof(buf) - 1 - got, 0)) > 0)
                got += (int)n;
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
    if (nfound < FINDER_MAX) {
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
    int total = hasOnly ? 2 : 254 * 2;
    for (int start = 0; start < total; start += BATCH) {
        int n = total - start < BATCH ? total - start : BATCH;
        int fds[BATCH];
        uint8_t ips[BATCH][4];
        int prt[BATCH];
        for (int i = 0; i < n; i++) {
            int idx = start + i;
            if (hasOnly) {
                memcpy(ips[i], only, 4);
            } else {
                memcpy(ips[i], base, 3);
                ips[i][3] = (uint8_t)(idx / 2 + 1);
            }
            prt[i] = ports[idx % 2];
            fds[i] = -1;
            if (!hasOnly && ips[i][3] == base[3])
                continue;   /* not ourselves */
            int s = socket(AF_INET, SOCK_STREAM, 0);
            if (s < 0)
                continue;
            fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
            struct sockaddr_in a;
            addr_of(&a, ips[i], prt[i]);
            if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0 && errno != EINPROGRESS) {
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
                getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &err, &len);
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
        progress = (start + n) * 100 / total;
        pthread_mutex_unlock(&lock);
    }
    pthread_mutex_lock(&lock);
    fstate = FINDER_DONE;
    progress = 100;
    pthread_mutex_unlock(&lock);
    return NULL;
}

void finder_start(const uint8_t self[4], const uint8_t *ip)
{
    pthread_mutex_lock(&lock);
    if (fstate == FINDER_RUNNING) {
        pthread_mutex_unlock(&lock);
        return;
    }
    fstate = FINDER_RUNNING;
    nfound = 0;
    progress = 0;
    memcpy(base, self, 4);
    hasOnly = ip != NULL;
    if (ip)
        memcpy(only, ip, 4);
    pthread_mutex_unlock(&lock);
    pthread_t t;
    pthread_create(&t, NULL, search, NULL);
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
