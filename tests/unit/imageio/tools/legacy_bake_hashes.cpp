// tests/unit/imageio/tools/legacy_bake_hashes.cpp -- the legacy bake hashes of spec 048 (M2, T087 and T093).
//
//   legacy_bake_hashes --record <file>    bake every case of bake_cases.h through the LEGACY writer (no "imageio" parameter) and write
//                                         the sha256 of each output, `sha256sum -c` compatible (<hash>  <case>.tex)
//   legacy_bake_hashes --check <file>     bake them again and compare with the file: every hash must be unchanged
//
// A tool, not a test. It is run on the unmodified legacy writer before the sink split (T093) and again after it (T094, T135), on each
// platform against its own file (the legacy bytes of a source that is resized differ between macOS and Linux). The entries of the
// four `med_*` cases equal the otexmake outputs recorded in the Phase 1 baseline.
#include "bake_cases.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

// ---- SHA-256 (FIPS 180-4) ----
namespace {

    struct Sha256 {
            uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
            uint64_t total = 0;
            uint8_t buf[64];
            size_t used = 0;

            static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

            void block(const uint8_t *p) {
                static const uint32_t k[64] = {
                    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
                    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
                    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
                    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
                    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
                    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
                uint32_t w[64];
                for (int i = 0; i < 16; i++)
                    w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
                for (int i = 16; i < 64; i++) {
                    uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
                    uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
                    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
                }
                uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
                for (int i = 0; i < 64; i++) {
                    uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
                    uint32_t ch = (e & f) ^ (~e & g);
                    uint32_t t1 = hh + S1 + ch + k[i] + w[i];
                    uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
                    uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
                    uint32_t t2 = S0 + mj;
                    hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
                }
                h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
            }

            void update(const uint8_t *p, size_t n) {
                total += n;
                while (n > 0) {
                    size_t take = std::min(n, sizeof(buf) - used);
                    memcpy(buf + used, p, take);
                    used += take; p += take; n -= take;
                    if (used == 64) { block(buf); used = 0; }
                }
            }

            std::string finish() {
                uint64_t bits = total * 8;
                uint8_t pad = 0x80;
                update(&pad, 1);
                uint8_t zero = 0;
                while (used != 56)
                    update(&zero, 1);
                uint8_t len[8];
                for (int i = 0; i < 8; i++)
                    len[i] = (uint8_t)(bits >> (56 - 8 * i));
                update(len, 8);
                char out[65];
                for (int i = 0; i < 8; i++)
                    snprintf(out + 8 * i, 9, "%08x", h[i]);
                return out;
            }
    };

    bool hashFile(const std::string &path, std::string &hex) {
        FILE *f = fopen(path.c_str(), "rb");
        if (f == nullptr)
            return false;
        Sha256 s;
        uint8_t b[8192];
        size_t n;
        while ((n = fread(b, 1, sizeof(b), f)) > 0)
            s.update(b, n);
        fclose(f);
        hex = s.finish();
        return true;
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[1], "--record") != 0 && strcmp(argv[1], "--check") != 0)) {
        fprintf(stderr, "usage: legacy_bake_hashes --record|--check <file>\n");
        return 2;
    }
    const bool record = strcmp(argv[1], "--record") == 0;
    const std::string file = argv[2];

    char tmpl[] = "/tmp/or048_bakehash_XXXXXX";
    const char *dir = mkdtemp(tmpl);
    if (dir == nullptr) {
        fprintf(stderr, "cannot create a scratch directory\n");
        return 2;
    }

    RiBegin(RI_NULL);
    std::map<std::string, std::string> hashes;
    int missing = 0;
    for (const bakecases::BakeCase &c : bakecases::cases()) {
        const std::string out = std::string(dir) + "/" + c.name + ".tex";
        bakecases::bakeCase(c, out, nullptr); // no "imageio" parameter: the legacy decoders and the legacy writer
        std::string hex;
        if (hashFile(out, hex))
            hashes[std::string(c.name) + ".tex"] = hex;
        else {
            fprintf(stderr, "no output for %s\n", c.name);
            missing++;
        }
    }
    RiEnd();

    if (record) {
        FILE *f = fopen(file.c_str(), "w");
        if (f == nullptr) {
            fprintf(stderr, "cannot write %s\n", file.c_str());
            return 2;
        }
        for (const auto &kv : hashes)
            fprintf(f, "%s  %s\n", kv.second.c_str(), kv.first.c_str());
        fclose(f);
        printf("legacy_bake_hashes: %zu hashes recorded in %s, %d cases without output\n", hashes.size(), file.c_str(), missing);
        return missing == 0 ? 0 : 1;
    }

    // --check: every recorded hash must be reproduced, and no case may be missing from the file
    std::map<std::string, std::string> recorded;
    FILE *f = fopen(file.c_str(), "r");
    if (f == nullptr) {
        fprintf(stderr, "cannot read %s\n", file.c_str());
        return 2;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char hex[80], nm[400];
        if (sscanf(line, "%79s %399s", hex, nm) == 2)
            recorded[nm] = hex;
    }
    fclose(f);
    int bad = 0;
    for (const auto &kv : recorded) {
        auto it = hashes.find(kv.first);
        if (it == hashes.end()) {
            printf("MISSING   %s\n", kv.first.c_str());
            bad++;
        } else if (it->second != kv.second) {
            printf("CHANGED   %s\n", kv.first.c_str());
            bad++;
        }
    }
    for (const auto &kv : hashes)
        if (recorded.find(kv.first) == recorded.end()) {
            printf("UNRECORDED %s\n", kv.first.c_str());
            bad++;
        }
    printf("legacy_bake_hashes: %zu recorded, %d difference(s)\n", recorded.size(), bad);
    return (bad == 0 && missing == 0) ? 0 : 1;
}
