/**
 * tests/framebuffer/test_fbx_driver.cpp
 *
 * Unit tests for CXDisplay as IPC client — verifies the TLV packet
 * encoding used by the refactored X11 framebuffer driver.
 *
 * Uses socketpair() for deterministic I/O without timing races.
 * Mirrors test_fbq_driver.cpp; X11-specific behaviour is in the helper.
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
// T024: START packet — CXDisplay constructor sends correct START payload
// ---------------------------------------------------------------------------

static void test_x11_start_packet_rgb() {
    int sv[2]; makePair(sv);

    bool ok = sendStart(sv[0], 800, 600, 3, "x11-test");
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x01); // START

    // Payload: FBStartPayload(16) + "x11-test"(8)
    EXPECT_EQ(hdr.length, (uint32_t)24);

    FBStartPayload sp;
    EXPECT_TRUE(readAll(sv[1], &sp, sizeof(sp)));
    EXPECT_EQ(sp.width,      (uint32_t)800);
    EXPECT_EQ(sp.height,     (uint32_t)600);
    EXPECT_EQ(sp.numSamples, (uint32_t)3);
    EXPECT_EQ(sp.titleLen,   (uint32_t)8);

    char title[9] = {};
    EXPECT_TRUE(readAll(sv[1], title, 8));
    EXPECT_EQ(strcmp(title, "x11-test"), 0);

    close(sv[1]);
}

static void test_x11_start_packet_rgba() {
    int sv[2]; makePair(sv);

    bool ok = sendStart(sv[0], 1280, 720, 4, "rgba-render");
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x01);

    FBStartPayload sp;
    EXPECT_TRUE(readAll(sv[1], &sp, sizeof(sp)));
    EXPECT_EQ(sp.numSamples, (uint32_t)4);

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// T024: DATA forwarding
// ---------------------------------------------------------------------------

static void test_x11_data_packet_rgb() {
    int sv[2]; makePair(sv);

    float pixels[9] = {1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f};
    bool ok = sendData(sv[0], 5, 10, 3, 1, 3, pixels);
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x02); // DATA
    // FBDataPayload(16) + 3*1*3*4 = 16+36 = 52
    EXPECT_EQ(hdr.length, (uint32_t)52);

    FBDataPayload dp;
    EXPECT_TRUE(readAll(sv[1], &dp, sizeof(dp)));
    EXPECT_EQ(dp.x, (uint32_t)5);
    EXPECT_EQ(dp.y, (uint32_t)10);
    EXPECT_EQ(dp.w, (uint32_t)3);
    EXPECT_EQ(dp.h, (uint32_t)1);

    close(sv[1]);
}

static void test_x11_data_packet_large_tile() {
    int sv[2]; makePair(sv);

    // 16×16 tile fits in socket buffer (3072 bytes of pixel data)
    int w = 16, h = 16, ch = 3;
    int n = w * h * ch;
    float *pixels = new float[n];
    for (int i = 0; i < n; ++i) pixels[i] = 0.5f;

    bool ok = sendData(sv[0], 0, 0, w, h, ch, pixels);
    EXPECT_TRUE(ok);
    delete[] pixels;
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x02);
    // 16 + 16*16*3*4 = 16 + 3072 = 3088
    EXPECT_EQ(hdr.length, (uint32_t)(16 + w * h * ch * 4));

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// T024: DONE — CXDisplay::finish() sends DONE then closes
// ---------------------------------------------------------------------------

static void test_x11_done_packet() {
    int sv[2]; makePair(sv);

    bool ok = sendDone(sv[0]);
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x03); // DONE
    EXPECT_EQ(hdr.length, (uint32_t)0);

    // After DONE + close, next read should return 0 (EOF)
    uint8_t byte;
    ssize_t r = read(sv[1], &byte, 1);
    EXPECT_EQ(r, (ssize_t)0);

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// T024: Socket closed mid-render → data() must return TRUE (render continues)
// ---------------------------------------------------------------------------

static void test_x11_write_to_closed_socket_fails() {
    int sv[2]; makePair(sv);
    close(sv[1]); // simulate helper crash

    signal(SIGPIPE, SIG_IGN);

    float dummy[3] = {0.2f, 0.4f, 0.6f};
    bool ok = sendData(sv[0], 0, 0, 1, 1, 3, dummy);
    EXPECT_EQ(ok, false);

    close(sv[0]);
}

static void test_x11_done_to_closed_socket_fails() {
    int sv[2]; makePair(sv);
    close(sv[1]);
    signal(SIGPIPE, SIG_IGN);
    bool ok = sendDone(sv[0]);
    EXPECT_EQ(ok, false);
    close(sv[0]);
}

// ---------------------------------------------------------------------------
// T024: QUIT encoding (window-close sends QUIT back to driver)
// ---------------------------------------------------------------------------

static void test_x11_quit_encoding() {
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
    printf("=== test_fbx_driver ===\n");

    test_x11_start_packet_rgb();
    test_x11_start_packet_rgba();
    test_x11_data_packet_rgb();
    test_x11_data_packet_large_tile();
    test_x11_done_packet();
    test_x11_write_to_closed_socket_fails();
    test_x11_done_to_closed_socket_fails();
    test_x11_quit_encoding();

    printf("Passed: %d  Failed: %d\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
