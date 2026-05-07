/**
 * src/framebuffer/fbipc.h
 *
 * Shared TLV IPC protocol for framebuffer display helpers.
 * Used by all driver-side IPC clients (fbq.cpp, fbx.cpp, fbwl.cpp)
 * and referenced by helper-side code (orender-fb-linux/main.cpp).
 *
 * Wire format: [opcode:u8][length:u32-LE][payload:length bytes]
 *
 * See: specs/004-macos-framebuffer-output/contracts/ipc-protocol.md
 */

#ifndef FBIPC_H
#define FBIPC_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <libgen.h>  // dirname()
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

// ---------------------------------------------------------------------------
// Opcodes
// ---------------------------------------------------------------------------

enum class FBOpcode : uint8_t {
    START = 0x01,
    DATA  = 0x02,
    DONE  = 0x03,
    QUIT  = 0x04,
};

// ---------------------------------------------------------------------------
// Packet structs (packed — match wire layout exactly)
// ---------------------------------------------------------------------------

#pragma pack(push, 1)

struct FBHeader {
    FBOpcode opcode;   // 1 byte
    uint32_t length;   // 4 bytes, little-endian
};

struct FBStartPayload {
    uint32_t width;       // image width in pixels
    uint32_t height;      // image height in pixels
    uint32_t numSamples;  // 3 = RGB, 4 = RGBA
    uint32_t titleLen;    // byte length of title string that follows (0..512)
    // followed by titleLen bytes of UTF-8 title (no NUL terminator)
};

struct FBDataPayload {
    uint32_t x;  // tile origin X (0-based)
    uint32_t y;  // tile origin Y (0-based)
    uint32_t w;  // tile width
    uint32_t h;  // tile height
    // followed by w*h*numSamples*sizeof(float) bytes of float32 pixel data (LE)
};

#pragma pack(pop)

// ---------------------------------------------------------------------------
// Socket path utilities
// ---------------------------------------------------------------------------

// Fixed path per user — shared across all orender runs for the same UID.
// Allows successive renders to reuse / replace the existing helper window
// instead of accumulating one window per run.
inline std::string makeFixedSocketPath() {
    char buf[64];
    snprintf(buf, sizeof(buf), "/tmp/orender-fb-%d.sock", (int)getuid());
    return std::string(buf);
}

// Legacy PID-based path kept for reference / Linux helpers that still use it.
inline std::string makeSocketPath(pid_t pid) {
    char buf[64];
    snprintf(buf, sizeof(buf), "/tmp/orender-fb-%d.sock", (int)pid);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Helper binary path resolution
//
// Resolution order:
//   1. $ORENDERHOME/bin/<helperName>
//   2. dirname(argv0)/<helperName>
//   3. <helperName>  (bare name — posix_spawnp will search PATH)
// ---------------------------------------------------------------------------

inline std::string makeHelperPath(const char *argv0, const char *helperName) {
    // 1. ORENDERHOME environment variable
    const char *orenderhome = getenv("ORENDERHOME");
    if (orenderhome && orenderhome[0] != '\0') {
        std::string path(orenderhome);
        path += "/bin/";
        path += helperName;
        return path;
    }

    // 2. Directory of argv0
    if (argv0 && argv0[0] != '\0') {
        // dirname() may modify its argument — work on a copy
        char argv0_copy[4096];
        strncpy(argv0_copy, argv0, sizeof(argv0_copy) - 1);
        argv0_copy[sizeof(argv0_copy) - 1] = '\0';
        const char *dir = dirname(argv0_copy);
        if (dir && strcmp(dir, ".") != 0 && strcmp(dir, "") != 0) {
            std::string path(dir);
            path += "/";
            path += helperName;
            return path;
        }
    }

    // 3. Bare name — caller uses posix_spawnp for PATH search
    return std::string(helperName);
}

// ---------------------------------------------------------------------------
// Low-level write helper — retries on EINTR, returns false on error/EOF
// ---------------------------------------------------------------------------

inline bool writeAll(int fd, const void *buf, size_t len) {
    const uint8_t *p = static_cast<const uint8_t *>(buf);
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return false;
        }
        p   += n;
        len -= (size_t)n;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Packet send helpers
// ---------------------------------------------------------------------------

inline bool sendHeader(int fd, FBOpcode opcode, uint32_t payloadLen) {
    FBHeader hdr;
    hdr.opcode = opcode;
    hdr.length = payloadLen;
    return writeAll(fd, &hdr, sizeof(hdr));
}

inline bool sendStart(int fd, uint32_t width, uint32_t height,
                      uint32_t numSamples, const char *title) {
    uint32_t titleLen = title ? (uint32_t)strlen(title) : 0;
    if (titleLen > 512) titleLen = 512;
    uint32_t payloadLen = sizeof(FBStartPayload) + titleLen;

    if (!sendHeader(fd, FBOpcode::START, payloadLen)) return false;

    FBStartPayload sp;
    sp.width      = width;
    sp.height     = height;
    sp.numSamples = numSamples;
    sp.titleLen   = titleLen;
    if (!writeAll(fd, &sp, sizeof(sp))) return false;
    if (titleLen > 0 && !writeAll(fd, title, titleLen)) return false;
    return true;
}

inline bool sendData(int fd, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                     uint32_t numSamples, const float *pixels) {
    uint32_t pixelBytes = w * h * numSamples * (uint32_t)sizeof(float);
    uint32_t payloadLen = sizeof(FBDataPayload) + pixelBytes;

    if (!sendHeader(fd, FBOpcode::DATA, payloadLen)) return false;

    FBDataPayload dp;
    dp.x = x;
    dp.y = y;
    dp.w = w;
    dp.h = h;
    if (!writeAll(fd, &dp, sizeof(dp))) return false;
    if (!writeAll(fd, pixels, pixelBytes)) return false;
    return true;
}

inline bool sendDone(int fd) {
    return sendHeader(fd, FBOpcode::DONE, 0);
}

inline bool sendQuit(int fd) {
    return sendHeader(fd, FBOpcode::QUIT, 0);
}

#endif // FBIPC_H
