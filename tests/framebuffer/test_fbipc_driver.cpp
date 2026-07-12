/**
 * tests/framebuffer/test_fbipc_driver.cpp
 *
 * Unit tests for the CIPCDisplay IPC client — verifies the TLV packet
 * encoding functions from fbipc.h used by the unified display driver.
 *
 * Consolidates the former test_fbq_driver, test_fbx_driver, and
 * test_fbwl_driver files; all three tested the same fbipc.h helpers.
 *
 * Uses a pair of Unix-domain sockets (socketpair) for deterministic I/O
 * without timing races.
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
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

// Read exactly n bytes from fd
static bool readAll(int fd, void *buf, size_t n) {
    uint8_t *p = static_cast<uint8_t *>(buf);
    while (n > 0) {
        ssize_t r = read(fd, p, n);
        if (r <= 0) return false;
        p += r; n -= (size_t)r;
    }
    return true;
}

// Create a connected socketpair: sv[0] = writer (driver side), sv[1] = reader (helper side)
static void makePair(int sv[2]) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        perror("socketpair"); exit(1);
    }
}

// ---------------------------------------------------------------------------
// START packet construction
// ---------------------------------------------------------------------------

static void test_start_packet_field_values() {
    int sv[2]; makePair(sv);

    bool ok = sendStart(sv[0], 1920, 1080, 3, 1750000000ULL, "test-scene");
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x01); // START

    // Payload: FBStartPayload(24) + "test-scene"(10) = 34
    EXPECT_EQ(hdr.length, (uint32_t)34);

    FBStartPayload sp;
    EXPECT_TRUE(readAll(sv[1], &sp, sizeof(sp)));
    EXPECT_EQ(sp.width,      (uint32_t)1920);
    EXPECT_EQ(sp.height,     (uint32_t)1080);
    EXPECT_EQ(sp.numSamples, (uint32_t)3);
    EXPECT_EQ(sp.startEpoch, (uint64_t)1750000000ULL);
    EXPECT_EQ(sp.titleLen,   (uint32_t)10);

    char title[11] = {};
    EXPECT_TRUE(readAll(sv[1], title, 10));
    EXPECT_EQ(strcmp(title, "test-scene"), 0);

    close(sv[1]);
}

static void test_start_packet_zero_title() {
    int sv[2]; makePair(sv);

    bool ok = sendStart(sv[0], 320, 240, 4, 0, "");
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x01);
    // Payload: FBStartPayload(24) + 0 title bytes = 24
    EXPECT_EQ(hdr.length, (uint32_t)24);

    FBStartPayload sp;
    EXPECT_TRUE(readAll(sv[1], &sp, sizeof(sp)));
    EXPECT_EQ(sp.numSamples, (uint32_t)4);
    EXPECT_EQ(sp.titleLen,   (uint32_t)0);

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// DATA packet encoding
// ---------------------------------------------------------------------------

static void test_data_packet_encoding() {
    int sv[2]; makePair(sv);

    float pixels[12] = {1.0f, 0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
                         0.0f, 0.0f, 1.0f,  0.5f, 0.5f, 0.5f};
    bool ok = sendData(sv[0], 10, 20, 2, 2, 3, pixels);
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x02); // DATA
    // Payload: FBDataPayload(16) + 2*2*3*sizeof(float) = 16+48 = 64
    EXPECT_EQ(hdr.length, (uint32_t)64);

    FBDataPayload dp;
    EXPECT_TRUE(readAll(sv[1], &dp, sizeof(dp)));
    EXPECT_EQ(dp.x, (uint32_t)10);
    EXPECT_EQ(dp.y, (uint32_t)20);
    EXPECT_EQ(dp.w, (uint32_t)2);
    EXPECT_EQ(dp.h, (uint32_t)2);

    // Verify first pixel value (float LE)
    float firstPixel;
    EXPECT_TRUE(readAll(sv[1], &firstPixel, sizeof(float)));
    EXPECT_TRUE(firstPixel > 0.99f && firstPixel < 1.01f);

    close(sv[1]);
}

static void test_data_packet_rgba() {
    int sv[2]; makePair(sv);

    float pixels[4] = {0.1f, 0.2f, 0.3f, 1.0f}; // 1x1 RGBA
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

static void test_data_packet_large_tile() {
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
    // FBDataPayload(16) + 16*16*3*4 = 16 + 3072 = 3088
    EXPECT_EQ(hdr.length, (uint32_t)(16 + w * h * ch * 4));

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// DONE packet
// ---------------------------------------------------------------------------

static void test_done_packet() {
    int sv[2]; makePair(sv);

    bool ok = sendDone(sv[0], 1234);
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x03); // DONE
    EXPECT_EQ(hdr.length, (uint32_t)sizeof(FBDonePayload));

    FBDonePayload dp;
    EXPECT_TRUE(readAll(sv[1], &dp, sizeof(dp)));
    EXPECT_EQ(dp.durationMillis, (uint32_t)1234);

    close(sv[1]);
}

static void test_done_packet_eof() {
    int sv[2]; makePair(sv);

    bool ok = sendDone(sv[0], 1234);
    EXPECT_TRUE(ok);
    close(sv[0]);

    FBHeader hdr;
    EXPECT_TRUE(readAll(sv[1], &hdr, sizeof(hdr)));
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x03); // DONE
    EXPECT_EQ(hdr.length, (uint32_t)sizeof(FBDonePayload));

    FBDonePayload dp;
    EXPECT_TRUE(readAll(sv[1], &dp, sizeof(dp)));

    // After DONE + close, next read should return 0 (EOF)
    uint8_t byte;
    ssize_t r = read(sv[1], &byte, 1);
    EXPECT_EQ(r, (ssize_t)0);

    close(sv[1]);
}

// ---------------------------------------------------------------------------
// Error paths
// ---------------------------------------------------------------------------

static void test_write_to_closed_socket_fails() {
    int sv[2]; makePair(sv);
    close(sv[1]); // close reader end — simulates helper exit

    signal(SIGPIPE, SIG_IGN);

    float dummy[3] = {0.1f, 0.2f, 0.3f};
    bool ok = sendData(sv[0], 0, 0, 1, 1, 3, dummy);
    EXPECT_EQ(ok, false);

    close(sv[0]);
}

static void test_sendDone_to_closed_fd_fails() {
    int sv[2]; makePair(sv);
    close(sv[1]);
    signal(SIGPIPE, SIG_IGN);
    bool ok = sendDone(sv[0], 0);
    EXPECT_EQ(ok, false);
    close(sv[0]);
}

// ---------------------------------------------------------------------------
// QUIT packet
// ---------------------------------------------------------------------------

static void test_sendQuit_encoding() {
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
    printf("=== test_fbipc_driver ===\n");

    test_start_packet_field_values();
    test_start_packet_zero_title();
    test_data_packet_encoding();
    test_data_packet_rgba();
    test_data_packet_large_tile();
    test_done_packet();
    test_done_packet_eof();
    test_write_to_closed_socket_fails();
    test_sendDone_to_closed_fd_fails();
    test_sendQuit_encoding();

    printf("Passed: %d  Failed: %d\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
