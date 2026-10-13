// SHA-1 and base64, only for the WebSocket handshake (Sec-WebSocket-Accept).
#pragma once
#include <cstdint>
#include <string>

namespace sha1 {

inline uint32_t rol(uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

// 20-byte digest of s
inline std::string digest(const std::string &s) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::string m = s;
    uint64_t bits = (uint64_t)s.size() * 8;
    m += (char)0x80;
    while (m.size() % 64 != 56) {
        m += (char)0;
    }
    for (int i = 7; i >= 0; i--) {
        m += (char)(bits >> (i * 8));
    }
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            const unsigned char *p = reinterpret_cast<const unsigned char *>(m.data() + off + i * 4);
            w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
        }
        for (int i = 16; i < 80; i++) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    std::string out;
    for (uint32_t v : h) {
        for (int i = 3; i >= 0; i--) {
            out += (char)(v >> (i * 8));
        }
    }
    return out;
}

inline std::string base64(const std::string &in) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    const unsigned char *p = reinterpret_cast<const unsigned char *>(in.data());
    for (; i + 2 < in.size(); i += 3) {
        uint32_t v = p[i] << 16 | p[i + 1] << 8 | p[i + 2];
        out += T[v >> 18]; out += T[(v >> 12) & 63]; out += T[(v >> 6) & 63]; out += T[v & 63];
    }
    if (i + 1 == in.size()) {
        uint32_t v = p[i] << 16;
        out += T[v >> 18]; out += T[(v >> 12) & 63]; out += "==";
    } else if (i + 2 == in.size()) {
        uint32_t v = p[i] << 16 | p[i + 1] << 8;
        out += T[v >> 18]; out += T[(v >> 12) & 63]; out += T[(v >> 6) & 63]; out += '=';
    }
    return out;
}

}  // namespace sha1
