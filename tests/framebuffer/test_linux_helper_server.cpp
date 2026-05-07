/**
 * tests/framebuffer/test_linux_helper_server.cpp
 *
 * Unit tests for the orender-fb-linux TLV server logic (T025b).
 *
 * Tests TLV packet reading/dispatching in isolation. Uses socketpair() to
 * inject pre-formed packets into a read loop that mirrors the helper's
 * main.cpp packet-dispatch logic, without spawning actual X11/Wayland windows.
 *
 * Covers: START → session init, DATA → pixel buffer update, DONE → complete
 * state, QUIT → clean exit flag, EOF-without-QUIT → interrupted state,
 * malformed packet → safe close.
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>

#include "framebuffer/fbipc.h"

// ---------------------------------------------------------------------------
// Test harness
// ---------------------------------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_EQ(a, b) do { \
    if ((a) == (b)) { g_passed++; } else { \
        fprintf(stderr, "FAIL [%s:%d]: expected %lld == %lld\n", \
                __FILE__, __LINE__, (long long)(a), (long long)(b)); \
        g_failed++; \
    } \
} while(0)

#define EXPECT_TRUE(cond) do { \
    if (cond) { g_passed++; } else { \
        fprintf(stderr, "FAIL [%s:%d]: expected true: %s\n", \
                __FILE__, __LINE__, #cond); \
        g_failed++; \
    } \
} while(0)

#define EXPECT_STR_CONTAINS(str, needle) do { \
    if (strstr((str), (needle)) != nullptr) { g_passed++; } else { \
        fprintf(stderr, "FAIL [%s:%d]: \"%s\" does not contain \"%s\"\n", \
                __FILE__, __LINE__, (str), (needle)); \
        g_failed++; \
    } \
} while(0)

static bool readAll(int fd, void *buf, size_t n) {
    uint8_t *p = static_cast<uint8_t *>(buf);
    while (n > 0) {
        ssize_t r = read(fd, p, n);
        if (r <= 0) return false;
        p += r; n -= (size_t)r;
    }
    return true;
}

static void makePair(int sv[2]) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        perror("socketpair"); exit(1);
    }
}

// ---------------------------------------------------------------------------
// Minimal TLV server logic (mirrors orender-fb-linux/main.cpp dispatch)
// Isolates the packet-reading/state-machine without X11/Wayland deps.
// ---------------------------------------------------------------------------

enum class SessionState { Idle, Active, Complete, Interrupted, Quit };

struct SessionCtx {
    uint32_t width = 0, height = 0, numSamples = 0;
    char title[256] = {};
    int tileCount = 0;
    uint32_t lastTileX = 0, lastTileY = 0, lastTileW = 0, lastTileH = 0;
    float *pixelBuf = nullptr;  // allocated on START
    SessionState state = SessionState::Idle;
};

// Returns false when the loop should exit.
static bool dispatchOnePacket(int fd, SessionCtx &ctx) {
    FBHeader hdr;
    if (!readAll(fd, &hdr, sizeof(hdr)))
        return false; // EOF

    uint32_t len = hdr.length;
    std::vector<uint8_t> payload(len);
    if (len > 0 && !readAll(fd, payload.data(), len))
        return false;

    switch (hdr.opcode) {
    case FBOpcode::START: {
        if (len < sizeof(FBStartPayload)) return false;
        FBStartPayload *sp = reinterpret_cast<FBStartPayload *>(payload.data());
        ctx.width      = sp->width;
        ctx.height     = sp->height;
        ctx.numSamples = sp->numSamples;
        uint32_t tlen  = sp->titleLen;
        if (tlen > 0 && len >= sizeof(FBStartPayload) + tlen) {
            size_t copy = std::min((size_t)tlen, sizeof(ctx.title) - 1);
            memcpy(ctx.title, payload.data() + sizeof(FBStartPayload), copy);
            ctx.title[copy] = '\0';
        }
        ctx.pixelBuf = new float[ctx.width * ctx.height * ctx.numSamples]();
        ctx.state = SessionState::Active;
        break;
    }
    case FBOpcode::DATA: {
        if (ctx.state != SessionState::Active) break;
        if (len < sizeof(FBDataPayload)) return false;
        FBDataPayload *dp = reinterpret_cast<FBDataPayload *>(payload.data());
        ctx.lastTileX = dp->x; ctx.lastTileY = dp->y;
        ctx.lastTileW = dp->w; ctx.lastTileH = dp->h;
        ctx.tileCount++;
        // copy pixels into buf if space permits
        size_t floatCount = (size_t)dp->w * dp->h * ctx.numSamples;
        if (len >= sizeof(FBDataPayload) + floatCount * sizeof(float) &&
            ctx.pixelBuf != nullptr) {
            float *src = reinterpret_cast<float *>(payload.data() + sizeof(FBDataPayload));
            float *dst = ctx.pixelBuf +
                (dp->y * ctx.width + dp->x) * ctx.numSamples;
            memcpy(dst, src, floatCount * sizeof(float));
        }
        break;
    }
    case FBOpcode::DONE:
        ctx.state = SessionState::Complete;
        snprintf(ctx.title + strlen(ctx.title),
                 sizeof(ctx.title) - strlen(ctx.title),
                 " — Complete");
        return false; // server stops reading after DONE
    case FBOpcode::QUIT:
        ctx.state = SessionState::Quit;
        return false;
    default:
        return false; // unknown opcode → safe close
    }
    return true;
}

// Run packet loop until it returns false.
static void runLoop(int fd, SessionCtx &ctx) {
    while (dispatchOnePacket(fd, ctx)) {}
    // EOF without DONE/QUIT → interrupted
    if (ctx.state == SessionState::Active) {
        ctx.state = SessionState::Interrupted;
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "Interrupted — %s", ctx.title);
        strncpy(ctx.title, tmp, sizeof(ctx.title) - 1);
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// T025b: START packet → session initialised with correct dimensions/title
static void test_start_initialises_session() {
    int sv[2]; makePair(sv);
    SessionCtx ctx;

    sendStart(sv[0], 320, 240, 3, "test-scene");
    close(sv[0]); // triggers EOF after START

    runLoop(sv[1], ctx);

    EXPECT_EQ(ctx.width,      (uint32_t)320);
    EXPECT_EQ(ctx.height,     (uint32_t)240);
    EXPECT_EQ(ctx.numSamples, (uint32_t)3);
    // EOF immediately after START → interrupted path prefixes title
    EXPECT_STR_CONTAINS(ctx.title, "test-scene");

    delete[] ctx.pixelBuf;
    close(sv[1]);
}

// T025b: DATA packet → pixel buffer updated at correct coordinates
static void test_data_updates_pixel_buffer() {
    int sv[2]; makePair(sv);
    SessionCtx ctx;

    sendStart(sv[0], 4, 4, 3, "");
    float pixels[3] = {0.9f, 0.1f, 0.5f}; // 1x1 RGB at (2,3)
    sendData(sv[0], 2, 3, 1, 1, 3, pixels);
    close(sv[0]);

    runLoop(sv[1], ctx);

    EXPECT_EQ(ctx.tileCount, 1);
    EXPECT_EQ(ctx.lastTileX, (uint32_t)2);
    EXPECT_EQ(ctx.lastTileY, (uint32_t)3);

    // Pixel at (2,3) in 4-wide image: offset = (3*4+2)*3 = 42
    if (ctx.pixelBuf) {
        float r = ctx.pixelBuf[42];
        EXPECT_TRUE(r > 0.89f && r < 0.91f);
    }

    delete[] ctx.pixelBuf;
    close(sv[1]);
}

// T025b: DONE packet → state = Complete, title updated
static void test_done_sets_complete_state() {
    int sv[2]; makePair(sv);
    SessionCtx ctx;

    sendStart(sv[0], 8, 8, 3, "my-render");
    sendDone(sv[0]);
    close(sv[0]);

    runLoop(sv[1], ctx);

    EXPECT_EQ(static_cast<int>(ctx.state), static_cast<int>(SessionState::Complete));
    EXPECT_STR_CONTAINS(ctx.title, "Complete");

    delete[] ctx.pixelBuf;
    close(sv[1]);
}

// T025b: QUIT packet → state = Quit + clean exit
static void test_quit_sets_quit_state() {
    int sv[2]; makePair(sv);
    SessionCtx ctx;

    sendStart(sv[0], 8, 8, 3, "quit-test");
    sendQuit(sv[0]);
    close(sv[0]);

    runLoop(sv[1], ctx);

    EXPECT_EQ(static_cast<int>(ctx.state), static_cast<int>(SessionState::Quit));

    delete[] ctx.pixelBuf;
    close(sv[1]);
}

// T025b: EOF without QUIT → interrupted state, title prefixed
static void test_eof_without_quit_sets_interrupted() {
    int sv[2]; makePair(sv);
    SessionCtx ctx;

    float pixels[3] = {0.5f, 0.5f, 0.5f};
    sendStart(sv[0], 4, 4, 3, "partial");
    sendData(sv[0], 0, 0, 1, 1, 3, pixels); // one tile delivered
    close(sv[0]); // abrupt disconnect

    runLoop(sv[1], ctx);

    EXPECT_EQ(static_cast<int>(ctx.state), static_cast<int>(SessionState::Interrupted));
    EXPECT_STR_CONTAINS(ctx.title, "Interrupted");

    delete[] ctx.pixelBuf;
    close(sv[1]);
}

// T025b: malformed packet (unknown opcode) → safe close, no crash
static void test_unknown_opcode_safe_close() {
    int sv[2]; makePair(sv);
    SessionCtx ctx;

    sendStart(sv[0], 4, 4, 3, "bad-pkt");

    // Inject an unknown opcode (0xFF)
    uint8_t bad[5] = {0xFF, 0x00, 0x00, 0x00, 0x00}; // opcode=0xFF, length=0
    write(sv[0], bad, 5);
    close(sv[0]);

    // Should not crash, should not hang
    runLoop(sv[1], ctx);

    // State ended before hitting Active→Interrupted transition since we
    // got to Active on START and then bailed on unknown opcode
    EXPECT_TRUE(ctx.state == SessionState::Active || ctx.state == SessionState::Interrupted);

    delete[] ctx.pixelBuf;
    close(sv[1]);
}

// T025b: EOF immediately (no START) → stays Idle, no crash
static void test_eof_before_start_stays_idle() {
    int sv[2]; makePair(sv);
    SessionCtx ctx;

    close(sv[0]); // immediate EOF
    runLoop(sv[1], ctx);

    EXPECT_EQ(static_cast<int>(ctx.state), static_cast<int>(SessionState::Idle));

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    printf("=== test_linux_helper_server ===\n");

    test_start_initialises_session();
    test_data_updates_pixel_buffer();
    test_done_sets_complete_state();
    test_quit_sets_quit_state();
    test_eof_without_quit_sets_interrupted();
    test_unknown_opcode_safe_close();
    test_eof_before_start_stays_idle();

    printf("Passed: %d  Failed: %d\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
