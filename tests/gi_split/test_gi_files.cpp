/**
 * tests/gi_split/test_gi_files.cpp
 *
 * Spec 032, T-F GiFiles_Unchanged (FR-019): the global-illumination files are unchanged by the cache split.
 *
 *   * The photon map. orender (-t:1, one render per process) runs examples/rib/tests/photon-parity.rib, whose
 *     frame 1 is a photon pass that writes photon-parity.gpm. The file is compared FIELD BY FIELD, not as raw
 *     bytes (OQ-8): the header, numItems, maxItems, then items 1 to n by field (P, N, flags, C, theta, phi), the
 *     bound, both matrices and maxPower. Item 0 (unused), the CPhoton padding (2 bytes after flags, 2 after
 *     phi: never assigned, so possibly nonzero) and an empty map's dummy direction are skipped. Each item
 *     field is reduced to the SHA-256 of its bytes over all items, so the reference stays small and a failure
 *     names the field.
 *   * The irradiance cache. orender (-t:1) runs examples/rib/tests/sphere-occlusion-reyes.rib with
 *     Attribute "irradiance" "handle"/"filemode" "w" added (a scratch copy, the tracked scene is not edited),
 *     which writes the cache when the frame ends. The file is fully defined, so its SHA-256 is compared.
 *
 * The test parses the .gpm by fixed offsets with a local mirror of the 44-byte on-disk layout (the precedent
 * is tests/visual/test_photon_solar0.cpp) and includes no renderer header, so it needs no edit when
 * photonMap.h moves.
 *
 * Usage:
 *   test_gi_files <orender> <rib dir> <work dir> --record <file>      write the reference text
 *   test_gi_files <orender> <rib dir> <work dir> <reference file>     compare against it
 * The environment is the visual tests' (SHADERS, ORENDERHOME, DISPLAYS, ...), set by CTest.
 */

#include "gi_testkit.h"

#include <cstdint>
#include <string>
#include <unistd.h>
#include <vector>

using gitest::Bytes;

// On-disk layout of one CPhoton: vector P, N; short flags (+2 padding); vector C; unsigned char theta, phi
// (+2 padding). 44 bytes (PhotonSolar0_* pins the size).
static constexpr size_t PHOTON_SIZE = 44;

template <typename T>
static T load(const Bytes &b, size_t offset) {
    T v;
    memcpy(&v, b.data() + offset, sizeof(T));
    return v;
}

static std::string hexFloat(float f) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%a", (double)f);
    return buf;
}

static std::string floats(const Bytes &b, size_t offset, int n) {
    std::string s;
    for (int i = 0; i < n; i++) {
        s += (i ? " " : "") + hexFloat(load<float>(b, offset + 4 * (size_t)i));
    }
    return s;
}

static std::string decimals(const Bytes &b, size_t offset, int n) {
    std::string s;
    for (int i = 0; i < n; i++) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%s%.9g", i ? " " : "", (double)load<float>(b, offset + 4 * (size_t)i));
        s += buf;
    }
    return s;
}

// The field-by-field description of a photon map file.
static bool describePhotonMap(const std::string &path, std::string &out, std::string &error) {
    Bytes b;
    if (!gitest::readFile(path, b)) {
        error = "no map written: " + path;
        return false;
    }
    // header: magic (4), four version ints (16), an int length, the type string and its NUL
    size_t o = 4 + 16;
    if (b.size() < o + 4) {
        error = "truncated header";
        return false;
    }
    const int32_t typeLength = load<int32_t>(b, o);
    if (typeLength < 0 || b.size() < o + 4 + (size_t)typeLength + 1 + 8) {
        error = "truncated header";
        return false;
    }
    char head[256];
    snprintf(head, sizeof(head), "header magic=0x%08x version=%d.%d.%d wordsize=%d type=\"%s\"", load<uint32_t>(b, 0),
             load<int32_t>(b, 4), load<int32_t>(b, 8), load<int32_t>(b, 12), load<int32_t>(b, 16), (const char *)&b[o + 4]);
    out += head;
    out += "\n";
    o += 4 + (size_t)typeLength + 1;

    const int32_t numItems = load<int32_t>(b, o);
    const int32_t maxItems = load<int32_t>(b, o + 4);
    o += 8;
    const size_t expected = o + PHOTON_SIZE * ((size_t)numItems + 1) + 6 * 4 + 32 * 4 + 4;
    if (numItems < 0 || b.size() != expected) {
        error = "unexpected file size " + std::to_string(b.size()) + " for " + std::to_string(numItems) +
                " photons (expected " + std::to_string(expected) + "): has the CPhoton layout changed?";
        return false;
    }
    out += "numItems " + std::to_string(numItems) + "\n";
    out += "maxItems " + std::to_string(maxItems) + "\n";

    // items 1 to n, field by field; item 0 is skipped
    static const struct {
            const char *name;
            size_t offset, size;
    } fields[] = {{"P", 0, 12}, {"N", 12, 12}, {"flags", 24, 2}, {"C", 28, 12}, {"theta", 40, 1}, {"phi", 41, 1}};
    for (const auto &f : fields) {
        Bytes joined;
        for (int32_t k = 1; k <= numItems; k++) {
            const size_t p = o + PHOTON_SIZE * (size_t)k + f.offset;
            joined.insert(joined.end(), b.begin() + (long)p, b.begin() + (long)(p + f.size));
        }
        out += std::string("field ") + f.name + " sha256 " + gitest::sha256Hex(joined) + "\n";
    }
    // the first eight items in the clear, so that a difference is readable
    for (int32_t k = 1; k <= numItems && k <= 8; k++) {
        const size_t p = o + PHOTON_SIZE * (size_t)k;
        char buf[400];
        snprintf(buf, sizeof(buf), "item %d P (%s) N (%s) flags %d C (%s) theta %u phi %u", k, decimals(b, p, 3).c_str(),
                 decimals(b, p + 12, 3).c_str(), (int)load<int16_t>(b, p + 24), decimals(b, p + 28, 3).c_str(),
                 (unsigned)b[p + 40], (unsigned)b[p + 41]);
        out += buf;
        out += "\n";
    }
    size_t t = o + PHOTON_SIZE * ((size_t)numItems + 1);
    out += "bmin " + floats(b, t, 3) + "\n";
    out += "bmax " + floats(b, t + 12, 3) + "\n";
    t += 24;
    out += "matrix0 " + floats(b, t, 16) + "\n";
    out += "matrix1 " + floats(b, t + 64, 16) + "\n";
    t += 128;
    out += "maxPower " + hexFloat(load<float>(b, t)) + "\n";
    return true;
}

static bool runOrender(const std::string &orender, const std::string &rib) {
    const std::string cmd = "\"" + orender + "\" -t:1 \"" + rib + "\"";
    printf("Running: %s\n", cmd.c_str());
    fflush(stdout);
    return system(cmd.c_str()) == 0;
}

static std::string describeAll(const std::string &orender, const std::string &ribDir, std::string &error) {
    std::string text = "# spec 032 T-F GiFiles_Unchanged: the photon map field by field (item 0 and the padding skipped) and the cache by SHA-256\n";

    // 1. the photon map
    remove("photon-parity.gpm");
    if (!runOrender(orender, ribDir + "/photon-parity.rib")) {
        error = "orender failed on photon-parity.rib";
        return "";
    }
    text += "[photon-parity.gpm]\n";
    if (!describePhotonMap("photon-parity.gpm", text, error)) {
        return "";
    }

    // 2. the irradiance cache, written by sphere-occlusion-reyes with the cache in "w" mode
    Bytes rib;
    if (!gitest::readFile(ribDir + "/sphere-occlusion-reyes.rib", rib)) {
        error = "cannot read sphere-occlusion-reyes.rib";
        return "";
    }
    std::string ribText(rib.begin(), rib.end());
    const std::string world = "WorldBegin\n";
    const size_t at = ribText.find(world);
    if (at == std::string::npos) {
        error = "sphere-occlusion-reyes.rib has no WorldBegin";
        return "";
    }
    ribText.insert(at + world.size(),
                   "  Attribute \"irradiance\" \"string handle\" [\"gi-files.icache\"]\n"
                   "  Attribute \"irradiance\" \"string filemode\" [\"w\"]\n");
    remove("gi-files.icache");
    if (!gitest::writeText("sphere-occlusion-reyes-cache.rib", ribText) ||
        !runOrender(orender, "sphere-occlusion-reyes-cache.rib")) {
        error = "orender failed on sphere-occlusion-reyes-cache.rib";
        return "";
    }
    Bytes cache;
    if (!gitest::readFile("gi-files.icache", cache)) {
        error = "no cache written: gi-files.icache";
        return "";
    }
    text += "[gi-files.icache]\n";
    text += "size " + std::to_string(cache.size()) + "\n";
    text += "sha256 " + gitest::sha256Hex(cache) + "\n";
    return text;
}

int main(int argc, char **argv) {
    if (argc != 6 && argc != 5) {
        fprintf(stderr, "usage: %s <orender> <rib dir> <work dir> (--record <file> | <reference file>)\n", argv[0]);
        return 2;
    }
    const std::string orender = argv[1], ribDir = argv[2], work = argv[3];
    const bool record = std::string(argv[4]) == "--record";
    if ((record && argc != 6) || (!record && argc != 5)) {
        fprintf(stderr, "usage: %s <orender> <rib dir> <work dir> (--record <file> | <reference file>)\n", argv[0]);
        return 2;
    }
    if (chdir(work.c_str()) != 0) {
        fprintf(stderr, "cannot enter %s\n", work.c_str());
        return 2;
    }

    std::string error;
    const std::string text = describeAll(orender, ribDir, error);
    if (text.empty()) {
        fprintf(stderr, "FAIL: %s\n", error.c_str());
        return 1;
    }

    if (record) {
        if (!gitest::writeText(argv[5], text)) {
            fprintf(stderr, "cannot write %s\n", argv[5]);
            return 1;
        }
        printf("recorded %s\n", argv[5]);
        return 0;
    }

    Bytes ref;
    if (!gitest::readFile(argv[4], ref)) {
        fprintf(stderr, "FAIL: no reference %s\n", argv[4]);
        return 1;
    }
    const std::string refText(ref.begin(), ref.end());
    if (refText == text) {
        printf("GiFiles_Unchanged: the photon map and the irradiance cache equal the reference\n");
        return 0;
    }
    // name the first differing line
    size_t a = 0, b = 0;
    int line = 1;
    while (a < refText.size() && b < text.size()) {
        const size_t ea = refText.find('\n', a), eb = text.find('\n', b);
        const std::string la = refText.substr(a, ea == std::string::npos ? std::string::npos : ea - a);
        const std::string lb = text.substr(b, eb == std::string::npos ? std::string::npos : eb - b);
        if (la != lb) {
            fprintf(stderr, "FAIL: line %d differs\n  reference: %s\n  now:       %s\n", line, la.c_str(), lb.c_str());
            return 1;
        }
        if (ea == std::string::npos || eb == std::string::npos) {
            break;
        }
        a = ea + 1;
        b = eb + 1;
        line++;
    }
    fprintf(stderr, "FAIL: the descriptions differ in length (%zu against %zu bytes)\n", refText.size(), text.size());
    return 1;
}
