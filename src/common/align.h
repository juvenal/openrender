/**
 * Project: Pixie
 *
 * File: align.h
 *
 * Description:
 *   This file defines various macros for memory alignment.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef ALIGN_H
#define ALIGN_H

// C++20: Use standard headers for fixed-width types
#include <cstdint>
#include <cstddef>

#include "global.h"

////////////////////////////////////////////////////////////////////////
// Platform Detection (C++20 Modernization)
//
// Standardized platform detection using consistent macros
////////////////////////////////////////////////////////////////////////

#if defined(_WIN32) || defined(_WIN64)
    #define PIXIE_PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #define PIXIE_PLATFORM_MACOS 1
    #include <TargetConditionals.h>
#elif defined(__linux__)
    #define PIXIE_PLATFORM_LINUX 1
#elif defined(__unix__) || defined(__unix)
    #define PIXIE_PLATFORM_UNIX 1
#else
    #warning "Unknown platform - assuming POSIX compatibility"
#endif

////////////////////////////////////////////////////////////////////////
// Architecture Detection (x86_64 vs ARM64)
//
// Explicit architecture detection for platform-specific optimizations
////////////////////////////////////////////////////////////////////////

#if defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
    #define PIXIE_ARCH_X86_64 1
    #define PIXIE_CACHE_LINE_SIZE 64
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
    #define PIXIE_ARCH_ARM64 1
    #define PIXIE_CACHE_LINE_SIZE 64  // Apple Silicon & modern ARM64
#elif defined(__i386__) || defined(__i386) || defined(_M_IX86)
    #define PIXIE_ARCH_X86 1
    #define PIXIE_CACHE_LINE_SIZE 64
#elif defined(__arm__) || defined(_M_ARM)
    #define PIXIE_ARCH_ARM32 1
    #define PIXIE_CACHE_LINE_SIZE 64
#else
    #warning "Unknown architecture - assuming 64-byte cache lines"
    #define PIXIE_CACHE_LINE_SIZE 64
#endif

////////////////////////////////////////////////////////////////////////
// Fixed-Width Integer Types (C++20)
//
// Use standard C++ types - no manual typedefs needed
// These are guaranteed by the C++ standard to be available
////////////////////////////////////////////////////////////////////////

using std::uint32_t;
using std::uint64_t;
using std::int32_t;
using std::int64_t;
using std::uintptr_t;
using std::intptr_t;
using std::size_t;

// Verify type sizes at compile time
static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
static_assert(sizeof(uintptr_t) == sizeof(void*), "uintptr_t must match pointer size");

////////////////////////////////////////////////////////////////////////
// Memory Alignment Functions (C++20 constexpr)
//
// Modern C++20 constexpr functions replace old macros
// Benefits:
//   - Type safety (no implicit conversions)
//   - Compile-time evaluation when possible
//   - Better error messages
//   - Can be used in constant expressions
////////////////////////////////////////////////////////////////////////

// Check if a pointer is 8-byte (64-bit) aligned
// Note: Not constexpr because reinterpret_cast of pointers cannot be evaluated at compile time
inline bool isAligned64(const void* ptr) noexcept {
    return (reinterpret_cast<std::uintptr_t>(ptr) & 0x7) == 0;
}

// Check if a value is 8-byte (64-bit) aligned
constexpr bool isAligned64(std::size_t value) noexcept {
    return (value & 0x7) == 0;
}

// Align value up to next 8-byte (64-bit) boundary
constexpr std::size_t align64(std::size_t value) noexcept {
    return (value + 7) & ~std::size_t{7};
}

// Align pointer up to next 8-byte (64-bit) boundary
template<typename T>
constexpr T* align64Ptr(T* ptr) noexcept {
    auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    addr = (addr + 7) & ~std::uintptr_t{7};
    return reinterpret_cast<T*>(addr);
}

// General alignment functions for arbitrary boundaries
constexpr std::size_t alignTo(std::size_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

constexpr bool isAligned(std::size_t value, std::size_t alignment) noexcept {
    return (value & (alignment - 1)) == 0;
}

// Note: Not constexpr because reinterpret_cast of pointers cannot be evaluated at compile time
inline bool isAligned(const void* ptr, std::size_t alignment) noexcept {
    return (reinterpret_cast<std::uintptr_t>(ptr) & (alignment - 1)) == 0;
}

////////////////////////////////////////////////////////////////////////
// Cache Line Alignment
//
// Useful for avoiding false sharing in multi-threaded code
////////////////////////////////////////////////////////////////////////

// Align to cache line boundary
constexpr std::size_t alignToCacheLine(std::size_t value) noexcept {
    return alignTo(value, PIXIE_CACHE_LINE_SIZE);
}

// Check if aligned to cache line
constexpr bool isCacheLineAligned(std::size_t value) noexcept {
    return isAligned(value, PIXIE_CACHE_LINE_SIZE);
}

// Note: Not constexpr (calls non-constexpr isAligned)
inline bool isCacheLineAligned(const void* ptr) noexcept {
    return isAligned(ptr, PIXIE_CACHE_LINE_SIZE);
}

////////////////////////////////////////////////////////////////////////
// Legacy Macro Compatibility (DEPRECATED)
//
// For gradual migration from old macro-based code
// TODO: Remove these after updating all call sites
////////////////////////////////////////////////////////////////////////

// DEPRECATED: Use isAligned64(ptr) function instead
#define IS_ALIGNED_64_DEPRECATED(__data) isAligned64(__data)

// DEPRECATED: Use align64(value) function instead
#define ALIGN_64_DEPRECATED(__data) align64(__data)

#endif // ALIGN_H

