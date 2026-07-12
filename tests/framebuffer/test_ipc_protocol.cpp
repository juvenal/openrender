/**
 * tests/framebuffer/test_ipc_protocol.cpp
 *
 * Unit tests for fbipc.h — TLV protocol constants, packet structs,
 * makeSocketPath(), makeHelperPath().
 *
 * Run: ctest -R test_ipc_protocol --output-on-failure
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

// Include the protocol header under test
#include "framebuffer/fbipc.h"

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_EQ(a, b) do { \
    if ((a) == (b)) { \
        g_passed++; \
    } else { \
        fprintf(stderr, "FAIL [%s:%d]: expected %lld == %lld\n", \
                __FILE__, __LINE__, (long long)(a), (long long)(b)); \
        g_failed++; \
    } \
} while(0)

#define EXPECT_NE(a, b) do { \
    if ((a) != (b)) { \
        g_passed++; \
    } else { \
        fprintf(stderr, "FAIL [%s:%d]: expected %lld != %lld\n", \
                __FILE__, __LINE__, (long long)(a), (long long)(b)); \
        g_failed++; \
    } \
} while(0)

#define EXPECT_TRUE(cond) do { \
    if (cond) { \
        g_passed++; \
    } else { \
        fprintf(stderr, "FAIL [%s:%d]: expected true: %s\n", \
                __FILE__, __LINE__, #cond); \
        g_failed++; \
    } \
} while(0)

#define EXPECT_STREQ(a, b) do { \
    if (strcmp((a), (b)) == 0) { \
        g_passed++; \
    } else { \
        fprintf(stderr, "FAIL [%s:%d]: expected \"%s\" == \"%s\"\n", \
                __FILE__, __LINE__, (a), (b)); \
        g_failed++; \
    } \
} while(0)

// ---------------------------------------------------------------------------
// T005: makeSocketPath tests
// ---------------------------------------------------------------------------

static void test_makeSocketPath_format() {
    // Socket path must be "/tmp/orender-fb-<pid>.sock"
    std::string path = makeSocketPath(12345);
    EXPECT_STREQ(path.c_str(), "/tmp/orender-fb-12345.sock");
}

static void test_makeSocketPath_pid_zero() {
    std::string path = makeSocketPath(0);
    EXPECT_STREQ(path.c_str(), "/tmp/orender-fb-0.sock");
}

static void test_makeSocketPath_large_pid() {
    std::string path = makeSocketPath(99999);
    EXPECT_STREQ(path.c_str(), "/tmp/orender-fb-99999.sock");
}

// ---------------------------------------------------------------------------
// T005: Opcode values
// ---------------------------------------------------------------------------

static void test_opcode_values() {
    EXPECT_EQ(static_cast<uint8_t>(FBOpcode::START), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(FBOpcode::DATA),  0x02);
    EXPECT_EQ(static_cast<uint8_t>(FBOpcode::DONE),  0x03);
    EXPECT_EQ(static_cast<uint8_t>(FBOpcode::QUIT),  0x04);
}

// ---------------------------------------------------------------------------
// T008: Struct size validation (wire-compatibility)
// ---------------------------------------------------------------------------

static void test_struct_sizes() {
    // FBHeader: 1 (opcode) + 4 (length LE) = 5 bytes, packed
    EXPECT_EQ(sizeof(FBHeader), (size_t)5);

    // FBStartPayload fixed fields: 4+4+4+8+4 = 24 bytes (no variable title)
    EXPECT_EQ(sizeof(FBStartPayload), (size_t)24);

    // FBDataPayload fixed fields: 4+4+4+4 = 16 bytes (no pixel array)
    EXPECT_EQ(sizeof(FBDataPayload), (size_t)16);
}

// ---------------------------------------------------------------------------
// T005: FBHeader field layout
// ---------------------------------------------------------------------------

static void test_header_opcode_field() {
    FBHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.opcode = FBOpcode::DATA;
    hdr.length = 42;
    EXPECT_EQ(static_cast<uint8_t>(hdr.opcode), 0x02);
    EXPECT_EQ(hdr.length, (uint32_t)42);
}

static void test_header_little_endian_length() {
    FBHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.length = 0x01020304u;
    // Verify the raw bytes are laid out in little-endian order
    const uint8_t *raw = reinterpret_cast<const uint8_t *>(&hdr);
    // opcode is byte 0; length starts at byte 1
    EXPECT_EQ(raw[1], (uint8_t)0x04); // LSB
    EXPECT_EQ(raw[2], (uint8_t)0x03);
    EXPECT_EQ(raw[3], (uint8_t)0x02);
    EXPECT_EQ(raw[4], (uint8_t)0x01); // MSB
}

// ---------------------------------------------------------------------------
// T005: FBStartPayload field layout
// ---------------------------------------------------------------------------

static void test_start_payload_fields() {
    FBStartPayload sp;
    memset(&sp, 0, sizeof(sp));
    sp.width      = 1920;
    sp.height     = 1080;
    sp.numSamples = 3;
    sp.titleLen   = 0;
    EXPECT_EQ(sp.width,      (uint32_t)1920);
    EXPECT_EQ(sp.height,     (uint32_t)1080);
    EXPECT_EQ(sp.numSamples, (uint32_t)3);
    EXPECT_EQ(sp.titleLen,   (uint32_t)0);
}

static void test_start_payload_max_title_len() {
    // titleLen field must accept up to 512 per spec
    FBStartPayload sp;
    sp.titleLen = 512;
    EXPECT_EQ(sp.titleLen, (uint32_t)512);
}

static void test_start_payload_numsamples_4() {
    FBStartPayload sp;
    sp.numSamples = 4;
    EXPECT_EQ(sp.numSamples, (uint32_t)4);
}

// ---------------------------------------------------------------------------
// T005: FBDataPayload field layout
// ---------------------------------------------------------------------------

static void test_data_payload_fields() {
    FBDataPayload dp;
    memset(&dp, 0, sizeof(dp));
    dp.x = 64;
    dp.y = 128;
    dp.w = 32;
    dp.h = 32;
    EXPECT_EQ(dp.x, (uint32_t)64);
    EXPECT_EQ(dp.y, (uint32_t)128);
    EXPECT_EQ(dp.w, (uint32_t)32);
    EXPECT_EQ(dp.h, (uint32_t)32);
}

// ---------------------------------------------------------------------------
// T005: Payload size calculation (DATA packets)
// ---------------------------------------------------------------------------

static void test_data_pixel_byte_count() {
    // w=4, h=4, numSamples=3 → 4*4*3*sizeof(float) = 192 bytes of pixel data
    const uint32_t w = 4, h = 4, numSamples = 3;
    const size_t expected = w * h * numSamples * sizeof(float);
    EXPECT_EQ(expected, (size_t)192);
}

static void test_data_pixel_byte_count_rgba() {
    // w=2, h=2, numSamples=4 → 2*2*4*4 = 64 bytes
    const uint32_t w = 2, h = 2, numSamples = 4;
    const size_t expected = w * h * numSamples * sizeof(float);
    EXPECT_EQ(expected, (size_t)64);
}

// ---------------------------------------------------------------------------
// T005: Zero-length payload packets (DONE, QUIT)
// ---------------------------------------------------------------------------

static void test_done_zero_length() {
    FBHeader hdr;
    hdr.opcode = FBOpcode::DONE;
    hdr.length = 0;
    EXPECT_EQ(hdr.length, (uint32_t)0);
}

static void test_quit_zero_length() {
    FBHeader hdr;
    hdr.opcode = FBOpcode::QUIT;
    hdr.length = 0;
    EXPECT_EQ(hdr.length, (uint32_t)0);
}

// ---------------------------------------------------------------------------
// T005: Boundary conditions — max tile size
// ---------------------------------------------------------------------------

static void test_max_tile_size() {
    // Max width/height per spec: 16384. Pixel count for RGBA:
    // 16384 * 16384 * 4 * 4 = 4,294,967,296 bytes — fits in uint32_t? No.
    // But the length field is 32-bit LE — single tile cannot exceed ~4GB payload.
    // The spec constrains per-tile size implicitly (tiles are sub-regions).
    // This test just verifies the max dimension constants are representable.
    const uint32_t max_dim = 16384;
    FBStartPayload sp;
    sp.width  = max_dim;
    sp.height = max_dim;
    EXPECT_EQ(sp.width,  (uint32_t)16384);
    EXPECT_EQ(sp.height, (uint32_t)16384);
}

// ---------------------------------------------------------------------------
// T007a: makeHelperPath tests
// ---------------------------------------------------------------------------

static void test_makeHelperPath_with_orenderhome() {
    // When ORENDERHOME is set, returns $ORENDERHOME/bin/<helperName>
    setenv("ORENDERHOME", "/opt/openrender", 1);
    std::string path = makeHelperPath("/usr/local/bin/orender", "orender-fb-macos");
    EXPECT_STREQ(path.c_str(), "/opt/openrender/bin/orender-fb-macos");
    unsetenv("ORENDERHOME");
}

static void test_makeHelperPath_without_orenderhome_uses_argv0_dir() {
    // When ORENDERHOME is unset, resolves relative to argv0 directory
    unsetenv("ORENDERHOME");
    std::string path = makeHelperPath("/usr/local/bin/orender", "orender-fb-linux");
    EXPECT_STREQ(path.c_str(), "/usr/local/bin/orender-fb-linux");
}

static void test_makeHelperPath_argv0_in_cwd() {
    // argv0 with no directory component — fall back to helperName (PATH lookup)
    unsetenv("ORENDERHOME");
    std::string path = makeHelperPath("orender", "orender-fb-macos");
    // Should fall back to bare name for PATH lookup
    EXPECT_STREQ(path.c_str(), "orender-fb-macos");
}

static void test_makeHelperPath_orenderhome_overrides_argv0() {
    // ORENDERHOME always takes precedence over argv0 directory
    setenv("ORENDERHOME", "/custom/prefix", 1);
    std::string path = makeHelperPath("/some/other/dir/orender", "orender-fb-linux");
    EXPECT_STREQ(path.c_str(), "/custom/prefix/bin/orender-fb-linux");
    unsetenv("ORENDERHOME");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    printf("=== test_ipc_protocol ===\n");

    // makeSocketPath
    test_makeSocketPath_format();
    test_makeSocketPath_pid_zero();
    test_makeSocketPath_large_pid();

    // Opcode values
    test_opcode_values();

    // Struct sizes (T008)
    test_struct_sizes();

    // Header fields
    test_header_opcode_field();
    test_header_little_endian_length();

    // START payload
    test_start_payload_fields();
    test_start_payload_max_title_len();
    test_start_payload_numsamples_4();

    // DATA payload
    test_data_payload_fields();
    test_data_pixel_byte_count();
    test_data_pixel_byte_count_rgba();

    // DONE/QUIT zero-length
    test_done_zero_length();
    test_quit_zero_length();

    // Boundary conditions
    test_max_tile_size();

    // makeHelperPath (T007a)
    test_makeHelperPath_with_orenderhome();
    test_makeHelperPath_without_orenderhome_uses_argv0_dir();
    test_makeHelperPath_argv0_in_cwd();
    test_makeHelperPath_orenderhome_overrides_argv0();

    printf("Passed: %d  Failed: %d\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
