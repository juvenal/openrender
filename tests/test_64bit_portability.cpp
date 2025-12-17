/**
 * Project: Pixie
 *
 * File: test_64bit_portability.cpp
 *
 * Description:
 *   64-bit portability test suite
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "../src/common/portable_io.h"
#include "../src/common/align.h"
#include "../src/common/global.h"

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    void test_##name(); \
    void run_test_##name() { \
        printf("Running test: %s ... ", #name); \
        fflush(stdout); \
        test_##name(); \
        tests_passed++; \
        printf("PASSED\n"); \
    } \
    void test_##name()

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("\nAssertion failed: %s\nFile: %s, Line: %d\n", \
                   #condition, __FILE__, __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (_a != _b) { \
            printf("\nAssertion failed: %s == %s\n", #a, #b); \
            printf("  Expected: %zu\n  Got: %zu\n", (size_t)_b, (size_t)_a); \
            printf("File: %s, Line: %d\n", __FILE__, __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

////////////////////////////////////////////////////////////////////////
// Test 1: Type Size Invariants
////////////////////////////////////////////////////////////////////////

TEST(type_sizes) {
    // Fixed-width types must have correct sizes
    static_assert(sizeof(int32_p) == 4, "int32_p must be 4 bytes");
    static_assert(sizeof(uint32_p) == 4, "uint32_p must be 4 bytes");
    static_assert(sizeof(int64_p) == 8, "int64_p must be 8 bytes");
    static_assert(sizeof(uint64_p) == 8, "uint64_p must be 8 bytes");
    static_assert(sizeof(float32_p) == 4, "float32_p must be 4 bytes");
    static_assert(sizeof(float64_p) == 8, "float64_p must be 8 bytes");

    // Runtime verification
    ASSERT_EQ(sizeof(int32_p), static_cast<size_t>(4));
    ASSERT_EQ(sizeof(uint32_p), static_cast<size_t>(4));
    ASSERT_EQ(sizeof(int64_p), static_cast<size_t>(8));
    ASSERT_EQ(sizeof(uint64_p), static_cast<size_t>(8));
    ASSERT_EQ(sizeof(float32_p), static_cast<size_t>(4));
    ASSERT_EQ(sizeof(float64_p), static_cast<size_t>(8));

    // Pointer types
    static_assert(sizeof(void*) == sizeof(uintptr_t), "uintptr_t must match pointer size");
    ASSERT_EQ(sizeof(void*), sizeof(uintptr_t));

    // T32 and T64 unions
    ASSERT_EQ(sizeof(T32), std::size_t(4));
    ASSERT_EQ(sizeof(T64), std::size_t(8));
    ASSERT(sizeof(T64) >= sizeof(void*));  // T64 must be able to hold pointers
}

////////////////////////////////////////////////////////////////////////
// Test 2: Binary I/O Round-Trip
////////////////////////////////////////////////////////////////////////

TEST(binary_io_roundtrip) {
    FILE* tmp = tmpfile();
    ASSERT(tmp != nullptr);

    // Test int32_p
    int32_p write_int = 0x12345678;
    ASSERT(writeInt32(tmp, write_int));

    // Test float32_p
    float32_p write_float = 3.14159f;
    ASSERT(writeFloat32(tmp, write_float));

    // Test vector
    float write_vec[3] = {1.0f, 2.0f, 3.0f};
    ASSERT(writeVector(tmp, write_vec, 3));

    // Test int64_p
    int64_p write_int64 = 0x123456789ABCDEF0LL;
    ASSERT(writeInt64(tmp, write_int64));

    // Rewind and read back
    rewind(tmp);

    int32_p read_int;
    ASSERT(readInt32(tmp, read_int));
    ASSERT_EQ(read_int, write_int);

    float32_p read_float;
    ASSERT(readFloat32(tmp, read_float));
    ASSERT(read_float == write_float);

    float read_vec[3];
    ASSERT(readVector(tmp, read_vec, 3));
    ASSERT(read_vec[0] == 1.0f);
    ASSERT(read_vec[1] == 2.0f);
    ASSERT(read_vec[2] == 3.0f);

    int64_p read_int64;
    ASSERT(readInt64(tmp, read_int64));
    ASSERT_EQ(read_int64, write_int64);

    fclose(tmp);
}

////////////////////////////////////////////////////////////////////////
// Test 3: Endianness Functions
////////////////////////////////////////////////////////////////////////

TEST(endianness) {
    // Test byte swapping
    uint32_p val32 = 0x12345678;
    uint32_p swapped32 = byteswap32(val32);
    ASSERT_EQ(swapped32, 0x78563412u);

    // Double swap should return original
    ASSERT_EQ(byteswap32(swapped32), val32);

    uint64_p val64 = 0x123456789ABCDEF0ull;
    uint64_p swapped64 = byteswap64(val64);
    ASSERT_EQ(swapped64, 0xF0DEBC9A78563412ull);
    ASSERT_EQ(byteswap64(swapped64), val64);

    // toLittleEndian should be consistent
    int32_p test_val = 0x11223344;
    int32_p converted = toLittleEndian(test_val);
    int32_p back = fromLittleEndian(converted);
    ASSERT_EQ(back, test_val);
}

////////////////////////////////////////////////////////////////////////
// Test 4: Alignment Functions
////////////////////////////////////////////////////////////////////////

TEST(alignment) {
    // Test pointer alignment checking
    ASSERT(isAligned64(reinterpret_cast<void*>(0x0)));
    ASSERT(isAligned64(reinterpret_cast<void*>(0x8)));
    ASSERT(isAligned64(reinterpret_cast<void*>(0x10)));
    ASSERT(!isAligned64(reinterpret_cast<void*>(0x4)));
    ASSERT(!isAligned64(reinterpret_cast<void*>(0x1)));

    // Test value alignment checking
    ASSERT(isAligned64(std::size_t(0)));
    ASSERT(isAligned64(std::size_t(8)));
    ASSERT(isAligned64(std::size_t(16)));
    ASSERT(!isAligned64(std::size_t(4)));
    ASSERT(!isAligned64(std::size_t(1)));

    // Test alignment function
    ASSERT_EQ(align64(0), std::size_t(0));
    ASSERT_EQ(align64(1), std::size_t(8));
    ASSERT_EQ(align64(7), std::size_t(8));
    ASSERT_EQ(align64(8), std::size_t(8));
    ASSERT_EQ(align64(9), std::size_t(16));
    ASSERT_EQ(align64(15), std::size_t(16));
    ASSERT_EQ(align64(16), std::size_t(16));

    // Test general alignment
    ASSERT(isAligned(std::size_t(0), std::size_t(16)));
    ASSERT(isAligned(std::size_t(16), std::size_t(16)));
    ASSERT(!isAligned(std::size_t(8), std::size_t(16)));
    ASSERT_EQ(alignTo(0, 16), std::size_t(0));
    ASSERT_EQ(alignTo(1, 16), std::size_t(16));
    ASSERT_EQ(alignTo(15, 16), std::size_t(16));
    ASSERT_EQ(alignTo(16, 16), std::size_t(16));

    // Test cache line alignment
    ASSERT_EQ(PIXIE_CACHE_LINE_SIZE, 64);
    ASSERT_EQ(alignToCacheLine(0), std::size_t(0));
    ASSERT_EQ(alignToCacheLine(1), std::size_t(64));
    ASSERT_EQ(alignToCacheLine(63), std::size_t(64));
    ASSERT_EQ(alignToCacheLine(64), std::size_t(64));
    ASSERT_EQ(alignToCacheLine(65), std::size_t(128));
}

////////////////////////////////////////////////////////////////////////
// Test 5: Platform and Architecture Detection
////////////////////////////////////////////////////////////////////////

TEST(platform_detection) {
    // At least one platform should be defined
    int platform_count = 0;
    #ifdef PIXIE_PLATFORM_WINDOWS
        platform_count++;
        printf("\n  Detected: Windows");
    #endif
    #ifdef PIXIE_PLATFORM_MACOS
        platform_count++;
        printf("\n  Detected: macOS");
    #endif
    #ifdef PIXIE_PLATFORM_LINUX
        platform_count++;
        printf("\n  Detected: Linux");
    #endif
    #ifdef PIXIE_PLATFORM_UNIX
        platform_count++;
        printf("\n  Detected: Unix");
    #endif
    ASSERT(platform_count > 0);

    // At least one architecture should be defined
    int arch_count = 0;
    #ifdef PIXIE_ARCH_X86_64
        arch_count++;
        printf("\n  Detected: x86_64");
    #endif
    #ifdef PIXIE_ARCH_ARM64
        arch_count++;
        printf("\n  Detected: ARM64");
    #endif
    #ifdef PIXIE_ARCH_X86
        arch_count++;
        printf("\n  Detected: x86");
    #endif
    #ifdef PIXIE_ARCH_ARM32
        arch_count++;
        printf("\n  Detected: ARM32");
    #endif
    ASSERT(arch_count > 0);

    printf("\n  Cache line size: %d bytes", PIXIE_CACHE_LINE_SIZE);
}

////////////////////////////////////////////////////////////////////////
// Test 6: Array I/O
////////////////////////////////////////////////////////////////////////

TEST(array_io) {
    FILE* tmp = tmpfile();
    ASSERT(tmp != nullptr);

    // Write int32 array
    int32_p write_ints[] = {1, 2, 3, 4, 5};
    ASSERT(writeInt32Array(tmp, write_ints, 5));

    // Write float32 array
    float32_p write_floats[] = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
    ASSERT(writeFloat32Array(tmp, write_floats, 5));

    // Read back
    rewind(tmp);

    int32_p read_ints[5];
    ASSERT(readInt32Array(tmp, read_ints, 5));
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(read_ints[i], write_ints[i]);
    }

    float32_p read_floats[5];
    ASSERT(readFloat32Array(tmp, read_floats, 5));
    for (int i = 0; i < 5; i++) {
        ASSERT(read_floats[i] == write_floats[i]);
    }

    fclose(tmp);
}

////////////////////////////////////////////////////////////////////////
// Test 7: Constexpr Alignment (Compile-Time)
////////////////////////////////////////////////////////////////////////

TEST(constexpr_alignment) {
    // These should be evaluated at compile time
    static_assert(isAligned64(std::size_t(0)), "0 should be aligned");
    static_assert(isAligned64(std::size_t(8)), "8 should be aligned");
    static_assert(!isAligned64(std::size_t(4)), "4 should not be aligned");

    static_assert(align64(0) == 0, "align64(0) == 0");
    static_assert(align64(1) == 8, "align64(1) == 8");
    static_assert(align64(8) == 8, "align64(8) == 8");

    static_assert(alignTo(0, 16) == 0, "alignTo(0, 16) == 0");
    static_assert(alignTo(1, 16) == 16, "alignTo(1, 16) == 16");

    // If we get here, all compile-time tests passed
    ASSERT(true);
}

////////////////////////////////////////////////////////////////////////
// Main Test Runner
////////////////////////////////////////////////////////////////////////

int main() {
    printf("========================================\n");
    printf("Pixie 64-bit Portability Test Suite\n");
    printf("========================================\n\n");

    // Run all tests
    run_test_type_sizes();
    run_test_binary_io_roundtrip();
    run_test_endianness();
    run_test_alignment();
    run_test_platform_detection();
    run_test_array_io();
    run_test_constexpr_alignment();

    // Summary
    printf("\n========================================\n");
    printf("Test Results:\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("========================================\n");

    if (tests_failed > 0) {
        printf("FAILURE: Some tests failed!\n");
        return 1;
    }

    printf("SUCCESS: All tests passed!\n");
    return 0;
}
