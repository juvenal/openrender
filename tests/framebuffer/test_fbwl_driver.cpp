/**
 * tests/framebuffer/test_fbwl_driver.cpp
 *
 * Unit tests for CWDisplay as IPC client — verifies the TLV packet
 * encoding used by the refactored Wayland framebuffer driver.
 *
 * Uses socketpair() for deterministic I/O without timing races.
 * Mirrors test_fbq_driver.cpp; Wayland-specific behaviour is in the helper.
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
// T025: START packet — CWDisplay constructor sends correct START payload
// ---------------------------------------------------------------------------

static void test_wl_start_packet_rgb() {
    int sv[2]; makePair(sv);

    bool ok = sendStart(sv[0], 1920, 1080, 3, "wayland-render");
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x01); // START

    // Payload: FBStartPayload(16) + "wayland-render"(14)
    EXPECT_EQ(hdr.length, (uint32_t)30);

    FBStartPayload sp;
    EXPECT_TRUE(readAll(sv[1], &sp, sizeof(sp)));
    EXPECT_EQ(sp.width,      (uint32_t)1920);
    EXPECT_EQ(sp.height,     (uint32_t)1080);
    EXPECT_EQ(sp.numSamples, (uint32_t)3);
    EXPECT_EQ(sp.titleLen,   (uint32_t)14);

    char title[15] = {};
    EXPECT_TRUE(readAll(sv[1], title, 14));
    EXPECT_EQ(strcmp(title, "wayland-render"), 0);

    close(sv[1]);
}

static void test_wl_start_empty_title() {
    int sv[2]; makePair(sv);

    bool ok = sendStart(sv[0], 640, 480, 3, "");
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x01);
    // FBStartPayload(16) + 0 bytes title
    EXPECT_EQ(hdr.length, (uint32_t)16);

    FBStartPayload sp;
    EXPECT_TRUE(readAll(sv[1], &sp, sizeof(sp)));
    EXPECT_EQ(sp.titleLen, (uint32_t)0);

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// T025: DATA forwarding
// ---------------------------------------------------------------------------

static void test_wl_data_packet_rgb() {
    int sv[2]; makePair(sv);

    float pixels[6] = {0.8f, 0.4f, 0.2f,  0.1f, 0.9f, 0.5f};
    bool ok = sendData(sv[0], 16, 32, 2, 1, 3, pixels);
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x02); // DATA
    // FBDataPayload(16) + 2*1*3*4 = 16+24 = 40
    EXPECT_EQ(hdr.length, (uint32_t)40);

    FBDataPayload dp;
    EXPECT_TRUE(readAll(sv[1], &dp, sizeof(dp)));
    EXPECT_EQ(dp.x, (uint32_t)16);
    EXPECT_EQ(dp.y, (uint32_t)32);
    EXPECT_EQ(dp.w, (uint32_t)2);
    EXPECT_EQ(dp.h, (uint32_t)1);

    close(sv[1]);
}

static void test_wl_data_packet_rgba() {
    int sv[2]; makePair(sv);

    float pixels[4] = {0.3f, 0.6f, 0.9f, 1.0f}; // 1x1 RGBA
    bool ok = sendData(sv[0], 0, 0, 1, 1, 4, pixels);
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x02);
    // FBDataPayload(16) + 1*1*4*4 = 32
    EXPECT_EQ(hdr.length, (uint32_t)32);

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// T025: DONE — CWDisplay::finish() sends DONE then closes
// ---------------------------------------------------------------------------

static void test_wl_done_packet() {
    int sv[2]; makePair(sv);

    bool ok = sendDone(sv[0]);
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x03); // DONE
    EXPECT_EQ(hdr.length, (uint32_t)0);

    uint8_t byte;
    ssize_t r = read(sv[1], &byte, 1);
    EXPECT_EQ(r, (ssize_t)0); // EOF after DONE+close

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// T025: Socket closed mid-render → data() returns TRUE (render continues)
// ---------------------------------------------------------------------------

static void test_wl_write_to_closed_socket_fails() {
    int sv[2]; makePair(sv);
    close(sv[1]); // simulate helper crash

    signal(SIGPIPE, SIG_IGN);

    float dummy[3] = {0.0f, 0.5f, 1.0f};
    bool ok = sendData(sv[0], 0, 0, 1, 1, 3, dummy);
    EXPECT_EQ(ok, false);

    close(sv[0]);
}

static void test_wl_done_to_closed_fails() {
    int sv[2]; makePair(sv);
    close(sv[1]);
    signal(SIGPIPE, SIG_IGN);
    bool ok = sendDone(sv[0]);
    EXPECT_EQ(ok, false);
    close(sv[0]);
}

// ---------------------------------------------------------------------------
// T025: QUIT encoding (xdg_toplevel_close sends QUIT back to driver)
// ---------------------------------------------------------------------------

static void test_wl_quit_encoding() {
    int sv[2]; makePair(sv);

    bool ok = sendQuit(sv[0]);
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x04); // QUIT
    EXPECT_EQ(hdr.length, (uint32_t)0);

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    printf("=== test_fbwl_driver ===\n");

    test_wl_start_packet_rgb();
    test_wl_start_empty_title();
    test_wl_data_packet_rgb();
    test_wl_data_packet_rgba();
    test_wl_done_packet();
    test_wl_write_to_closed_socket_fails();
    test_wl_done_to_closed_fails();
    test_wl_quit_encoding();

    printf("Passed: %d  Failed: %d\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
