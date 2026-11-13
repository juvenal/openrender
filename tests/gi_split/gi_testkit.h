/**
 * tests/gi_split/gi_testkit.h
 *
 * Spec 032: the small helpers the gi_split tests share. Header-only, standard C++ only (no renderer header), so
 * a test that includes it adds no include edge.
 *
 *   readFile / writeFile        whole-file byte vectors
 *   sha256Hex                   SHA-256 of a byte range (FIPS 180-4), as 64 lowercase hex digits
 *   Sections                    the container format of the loopback references: a magic line, then for each
 *                               section "<name> <byte count>\n" followed by that many bytes and "\n"
 *   describeFirstDifference     where two byte vectors first differ, for a failure message
 */

#ifndef GI_TESTKIT_H
#define GI_TESTKIT_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace gitest {

typedef std::vector<unsigned char> Bytes;

inline bool readFile(const std::string &path, Bytes &out) {
    out.clear();
    FILE *f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    fclose(f);
    return true;
}

inline bool writeFile(const std::string &path, const Bytes &bytes) {
    FILE *f = fopen(path.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool ok = bytes.empty() || fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    return fclose(f) == 0 && ok;
}

inline bool writeText(const std::string &path, const std::string &text) {
    return writeFile(path, Bytes(text.begin(), text.end()));
}

// ---------------------------------------------------------------------------------------------------------
// SHA-256
// ---------------------------------------------------------------------------------------------------------
inline std::string sha256Hex(const unsigned char *data, size_t length) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    // pad: 0x80, zeros to 56 mod 64, then the bit length as a 64-bit big-endian integer
    Bytes msg(data, data + length);
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) {
        msg.push_back(0);
    }
    const uint64_t bits = (uint64_t)length * 8;
    for (int i = 7; i >= 0; i--) {
        msg.push_back((unsigned char)(bits >> (8 * i)));
    }

    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
    for (size_t block = 0; block < msg.size(); block += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            const unsigned char *p = &msg[block + 4 * (size_t)i];
            w[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
        }
        for (int i = 16; i < 64; i++) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = S0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    char hex[65];
    for (int i = 0; i < 8; i++) {
        snprintf(hex + 8 * i, 9, "%08x", h[i]);
    }
    return std::string(hex, 64);
}

inline std::string sha256Hex(const Bytes &b) {
    return sha256Hex(b.data(), b.size());
}

// ---------------------------------------------------------------------------------------------------------
// The reference container: "GI32REF1\n", then per section "<name> <size>\n<bytes>\n".
// ---------------------------------------------------------------------------------------------------------
struct Sections {
        std::vector<std::pair<std::string, Bytes>> list;

        void add(const std::string &name, const Bytes &bytes) { list.emplace_back(name, bytes); }

        Bytes serialize() const {
            Bytes out;
            const std::string magic = "GI32REF1\n";
            out.insert(out.end(), magic.begin(), magic.end());
            for (const auto &s : list) {
                const std::string head = s.first + " " + std::to_string(s.second.size()) + "\n";
                out.insert(out.end(), head.begin(), head.end());
                out.insert(out.end(), s.second.begin(), s.second.end());
                out.push_back('\n');
            }
            return out;
        }

        bool parse(const Bytes &in) {
            list.clear();
            const std::string magic = "GI32REF1\n";
            if (in.size() < magic.size() || memcmp(in.data(), magic.data(), magic.size()) != 0) {
                return false;
            }
            size_t pos = magic.size();
            while (pos < in.size()) {
                size_t eol = pos;
                while (eol < in.size() && in[eol] != '\n') {
                    eol++;
                }
                if (eol >= in.size()) {
                    return false;
                }
                const std::string head((const char *)&in[pos], eol - pos);
                const size_t sp = head.rfind(' ');
                if (sp == std::string::npos) {
                    return false;
                }
                const size_t size = (size_t)strtoull(head.c_str() + sp + 1, nullptr, 10);
                pos = eol + 1;
                if (pos + size + 1 > in.size()) {
                    return false;
                }
                list.emplace_back(head.substr(0, sp), Bytes(in.begin() + (long)pos, in.begin() + (long)(pos + size)));
                pos += size + 1;
            }
            return true;
        }
};

inline std::string describeFirstDifference(const Bytes &a, const Bytes &b) {
    char buf[200];
    const size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            snprintf(buf, sizeof(buf), "first difference at byte %zu (0x%02x against 0x%02x); sizes %zu and %zu", i, a[i], b[i], a.size(), b.size());
            return buf;
        }
    }
    snprintf(buf, sizeof(buf), "equal for %zu bytes, then sizes differ: %zu and %zu", n, a.size(), b.size());
    return buf;
}

} // namespace gitest

#endif // GI_TESTKIT_H
