// tests/unit/imageio/decode_harness.h -- the shared harness of the decode tests (spec 048, Phase 4).
//
// The tests reach OpenImageIO only through the Texturing interface: createImageInput(path, backend). They install a
// capturing diagnostic sink (libRIBCommon) so an error's text can be checked, and they use non-fatal EXPECTs so one
// run reports every difference, then exit non-zero.
#pragma once

#include "common/diagnostics.h"
#include "imageInput.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace harness {

    inline int &failures() {
        static int n = 0;
        return n;
    }

#define EXPECT(cond, ...) \
    do { \
        if (!(cond)) { \
            harness::failures()++; \
            fprintf(stderr, "FAIL (%s:%d): ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
        } \
    } while (0)

    // Collects what error()/warning() report. A thread that sets `capture` gets its own messages; any other
    // message is kept in `orphans` (so the concurrent test sees reports from threads it did not wrap).
    class CaptureSink : public CDiagnosticSink {
        public:
            void report(EErrorCode, EDiagnosticSeverity, const char *message) override {
                if (capture() != nullptr) {
                    capture()->push_back(message);
                } else {
                    std::lock_guard<std::mutex> lock(mutex);
                    orphans.push_back(message);
                }
            }
            static std::vector<std::string> *&capture() {
                thread_local std::vector<std::string> *c = nullptr;
                return c;
            }
            std::mutex mutex;
            std::vector<std::string> orphans;
    };

    inline CaptureSink &sink() {
        static CaptureSink s;
        return s;
    }

    inline void installSink() {
        installDiagnosticSink(&sink());
    }

    struct Decoded {
            bool created = false, opened = false, read = false;
            CImageInfo info;
            std::vector<uint8_t> bytes;
            std::vector<std::string> messages;
            bool ok() const { return created && opened && read; }
            bool mentions(const std::string &word) const {
                for (const std::string &m : messages)
                    if (m.find(word) != std::string::npos)
                        return true;
                return false;
            }
            std::string allMessages() const {
                std::string s;
                for (const std::string &m : messages)
                    s += m;
                return s;
            }
    };

    inline size_t byteCount(const CImageInfo &i) {
        return (size_t)i.width * (size_t)i.height * (size_t)i.numChannels * (size_t)(i.bitsPerSample / 8);
    }

    // open, read once, close; every diagnostic of the calling thread is captured into the result
    inline Decoded decode(const std::string &path, EImageBackend backend) {
        Decoded d;
        std::vector<std::string> msgs;
        CaptureSink::capture() = &msgs;
        CImageInput *in = createImageInput(path.c_str(), backend);
        d.created = in != nullptr;
        if (in != nullptr) {
            d.opened = in->open(path.c_str(), d.info);
            if (d.opened) {
                d.bytes.assign(byteCount(d.info), 0xA5);
                d.read = in->readImage(d.bytes.data());
                if (!d.read)
                    d.bytes.clear();
            }
            in->close();
            delete in;
        }
        CaptureSink::capture() = nullptr;
        d.messages = msgs;
        return d;
    }

    inline bool isNan(uint32_t bits) { return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0; }

    // Equal when the info is equal and every sample is equal bit for bit (a float NaN equals a NaN, by class).
    inline bool sameSamples(const Decoded &a, const Decoded &b, std::string &why) {
        char buf[200];
        if (a.info.width != b.info.width || a.info.height != b.info.height || a.info.numChannels != b.info.numChannels ||
            a.info.bitsPerSample != b.info.bitsPerSample || a.info.isFloatFormat != b.info.isFloatFormat) {
            snprintf(buf, sizeof(buf), "info differs: %dx%d ch%d %dbit float=%d vs %dx%d ch%d %dbit float=%d", a.info.width,
                     a.info.height, a.info.numChannels, a.info.bitsPerSample, (int)a.info.isFloatFormat, b.info.width,
                     b.info.height, b.info.numChannels, b.info.bitsPerSample, (int)b.info.isFloatFormat);
            why = buf;
            return false;
        }
        if (a.bytes.size() != b.bytes.size()) {
            why = "buffer sizes differ";
            return false;
        }
        if (a.info.isFloatFormat) {
            for (size_t i = 0; i + 4 <= a.bytes.size(); i += 4) {
                uint32_t x, y;
                memcpy(&x, &a.bytes[i], 4);
                memcpy(&y, &b.bytes[i], 4);
                if (x != y && !(isNan(x) && isNan(y))) {
                    snprintf(buf, sizeof(buf), "float sample %zu differs: 0x%08x vs 0x%08x", i / 4, x, y);
                    why = buf;
                    return false;
                }
            }
        } else if (memcmp(a.bytes.data(), b.bytes.data(), a.bytes.size()) != 0) {
            size_t i = 0;
            while (a.bytes[i] == b.bytes[i])
                i++;
            snprintf(buf, sizeof(buf), "byte %zu differs: 0x%02x vs 0x%02x", i, a.bytes[i], b.bytes[i]);
            why = buf;
            return false;
        }
        return true;
    }

    inline std::string envOr(const char *name, const char *fallback) {
        const char *v = getenv(name);
        return v != nullptr ? v : fallback;
    }

    inline std::string imageioFixtures() { return envOr("IMAGEIO_FIXTURES_DIR", "tests/unit/imageio/fixtures"); }
    inline std::string legacyFixtures() { return envOr("IMAGE_INPUT_FIXTURES_DIR", "tests/unit/image_input/fixtures"); }

    inline bool endsWith(const std::string &s, const char *suffix) {
        size_t n = strlen(suffix);
        return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
    }

    inline std::string baseName(const std::string &path) {
        size_t p = path.find_last_of('/');
        return p == std::string::npos ? path : path.substr(p + 1);
    }

    // sorted regular files of dir that end in one of the suffixes
    inline std::vector<std::string> listFiles(const std::string &dir, const std::vector<const char *> &suffixes) {
        std::vector<std::string> out;
        DIR *d = opendir(dir.c_str());
        if (d == nullptr)
            return out;
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            for (const char *s : suffixes)
                if (endsWith(n, s)) {
                    out.push_back(dir + "/" + n);
                    break;
                }
        }
        closedir(d);
        std::sort(out.begin(), out.end());
        return out;
    }

    inline std::vector<uint8_t> readFile(const std::string &path) {
        std::vector<uint8_t> b;
        FILE *f = fopen(path.c_str(), "rb");
        if (f == nullptr)
            return b;
        uint8_t buf[8192];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            b.insert(b.end(), buf, buf + n);
        fclose(f);
        return b;
    }

    inline bool writeFile(const std::string &path, const std::vector<uint8_t> &b) {
        FILE *f = fopen(path.c_str(), "wb");
        if (f == nullptr)
            return false;
        bool ok = fwrite(b.data(), 1, b.size(), f) == b.size();
        fclose(f);
        return ok;
    }

    // a fresh temporary directory, kept for the life of the process and left to the system's temp cleaning
    inline std::string scratchDir() {
        static std::string dir;
        if (dir.empty()) {
            std::string t = envOr("TMPDIR", "/tmp") + "/or048_decode_XXXXXX";
            std::vector<char> buf(t.begin(), t.end());
            buf.push_back('\0');
            char *made = mkdtemp(buf.data());
            if (made == nullptr) {
                fprintf(stderr, "cannot create a scratch directory\n");
                exit(2);
            }
            dir = made;
        }
        return dir;
    }

    inline int finish(const char *name) {
        if (failures() != 0) {
            fprintf(stderr, "%s: FAILED, %d check(s)\n", name, failures());
            return 1;
        }
        printf("%s: ALL PASS\n", name);
        return 0;
    }

} // namespace harness
