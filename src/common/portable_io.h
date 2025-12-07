/**
 * Project: Pixie
 *
 * File: portable_io.h
 *
 * Description:
 *   This file defines the interface for portable_io.
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

///////////////////////////////////////////////////////////////////////
//
//  File				:	portable_io.h
//  Description			:	Portable binary I/O with endianness handling
//							Provides fixed-width types and byte-order aware
//							read/write functions for cross-platform compatibility
//
//	Created				:	2025 (C++20 modernization)
//	Purpose				:	Ensure binary file formats work across:
//							  - 32-bit and 64-bit platforms
//							  - Little-endian and big-endian architectures
//							  - Different compilers (sizeof(int) may vary)
//
//	File Format Standard:	All Pixie binary files use little-endian format
//
////////////////////////////////////////////////////////////////////////
#ifndef PORTABLE_IO_H
#define PORTABLE_IO_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

////////////////////////////////////////////////////////////////////////
// Endianness Detection
//
// Use compiler-defined macros when available, fall back to
// architecture-based detection for older compilers
////////////////////////////////////////////////////////////////////////

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define PIXIE_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define PIXIE_BIG_ENDIAN 1
#else
// Fallback: Most common architectures are little-endian
#if defined(_WIN32) || defined(_WIN64) ||                            \
    defined(__x86_64__) || defined(__i386__) || defined(__i686__) || \
    defined(__arm__) || defined(__aarch64__) ||                      \
    defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64)
#define PIXIE_LITTLE_ENDIAN 1
#else
#error "Cannot determine endianness for this platform"
#endif
#endif

////////////////////////////////////////////////////////////////////////
// Portable Fixed-Width Types for Serialization
//
// These types have guaranteed sizes across all platforms:
//   int32_p  : Always 32-bit signed integer (4 bytes)
//   uint32_p : Always 32-bit unsigned integer (4 bytes)
//   int64_p  : Always 64-bit signed integer (8 bytes)
//   uint64_p : Always 64-bit unsigned integer (8 bytes)
//   float32_p: Always IEEE 754 single precision (4 bytes)
//   float64_p: Always IEEE 754 double precision (8 bytes)
//
// Use these instead of int, long, float, double for binary I/O
////////////////////////////////////////////////////////////////////////

using int32_p = std::int32_t;
using uint32_p = std::uint32_t;
using int64_p = std::int64_t;
using uint64_p = std::uint64_t;
using float32_p = float;  // IEEE 754 required by C++ standard
using float64_p = double; // IEEE 754 required by C++ standard

// Compile-time size verification
static_assert(sizeof(int32_p) == 4, "int32_p must be 4 bytes");
static_assert(sizeof(uint32_p) == 4, "uint32_p must be 4 bytes");
static_assert(sizeof(int64_p) == 8, "int64_p must be 8 bytes");
static_assert(sizeof(uint64_p) == 8, "uint64_p must be 8 bytes");
static_assert(sizeof(float32_p) == 4, "float32_p must be 4 bytes");
static_assert(sizeof(float64_p) == 8, "float64_p must be 8 bytes");

////////////////////////////////////////////////////////////////////////
// Byte Swapping Functions (C++20 constexpr)
//
// Convert between little-endian and big-endian byte order
////////////////////////////////////////////////////////////////////////

constexpr uint32_p byteswap32(uint32_p value) noexcept {
    return ((value & 0xFF000000u) >> 24) |
           ((value & 0x00FF0000u) >> 8) |
           ((value & 0x0000FF00u) << 8) |
           ((value & 0x000000FFu) << 24);
}

constexpr uint64_p byteswap64(uint64_p value) noexcept {
    return ((value & 0xFF00000000000000ull) >> 56) |
           ((value & 0x00FF000000000000ull) >> 40) |
           ((value & 0x0000FF0000000000ull) >> 24) |
           ((value & 0x000000FF00000000ull) >> 8) |
           ((value & 0x00000000FF000000ull) << 8) |
           ((value & 0x0000000000FF0000ull) << 24) |
           ((value & 0x000000000000FF00ull) << 40) |
           ((value & 0x00000000000000FFull) << 56);
}

////////////////////////////////////////////////////////////////////////
// Endianness Conversion Functions
//
// Convert to/from little-endian format (Pixie file format standard)
// On little-endian systems, these are no-ops
// On big-endian systems, these perform byte swapping
////////////////////////////////////////////////////////////////////////

template <typename T>
T toLittleEndian(T value) noexcept {
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                  "Type must be integral or floating point");

#ifdef PIXIE_LITTLE_ENDIAN
    // Already little-endian, no conversion needed
    return value;
#else
    // Big-endian system, need to byte swap
    if constexpr (sizeof(T) == 4) {
        // 32-bit value (int32_p, uint32_p, float32_p)
        T result;
        uint32_p temp;
        std::memcpy(&temp, &value, sizeof(T));
        temp = byteswap32(temp);
        std::memcpy(&result, &temp, sizeof(T));
        return result;
    } else if constexpr (sizeof(T) == 8) {
        // 64-bit value (int64_p, uint64_p, float64_p)
        T result;
        uint64_p temp;
        std::memcpy(&temp, &value, sizeof(T));
        temp = byteswap64(temp);
        std::memcpy(&result, &temp, sizeof(T));
        return result;
    } else if constexpr (sizeof(T) == 1) {
        // Single byte, no swapping needed
        return value;
    } else {
        static_assert(sizeof(T) == 4 || sizeof(T) == 8 || sizeof(T) == 1,
                      "Unsupported type size for endianness conversion");
        return value;
    }
#endif
}

template <typename T>
T fromLittleEndian(T value) noexcept {
    // Conversion is symmetric (little→big == big→little)
    return toLittleEndian(value);
}

////////////////////////////////////////////////////////////////////////
// Portable Binary I/O Functions
//
// Write functions: Convert to little-endian and write to file
// Read functions: Read from file and convert from little-endian
//
// Return: true on success, false on I/O error
////////////////////////////////////////////////////////////////////////

// 32-bit signed integer
inline bool writeInt32(FILE *file, int32_p value) noexcept {
    if (!file)
        return false;
    value = toLittleEndian(value);
    return fwrite(&value, sizeof(int32_p), 1, file) == 1;
}

inline bool readInt32(FILE *file, int32_p &value) noexcept {
    if (!file)
        return false;
    if (fread(&value, sizeof(int32_p), 1, file) != 1)
        return false;
    value = fromLittleEndian(value);
    return true;
}

// 32-bit unsigned integer
inline bool writeUInt32(FILE *file, uint32_p value) noexcept {
    if (!file)
        return false;
    value = toLittleEndian(value);
    return fwrite(&value, sizeof(uint32_p), 1, file) == 1;
}

inline bool readUInt32(FILE *file, uint32_p &value) noexcept {
    if (!file)
        return false;
    if (fread(&value, sizeof(uint32_p), 1, file) != 1)
        return false;
    value = fromLittleEndian(value);
    return true;
}

// 64-bit signed integer
inline bool writeInt64(FILE *file, int64_p value) noexcept {
    if (!file)
        return false;
    value = toLittleEndian(value);
    return fwrite(&value, sizeof(int64_p), 1, file) == 1;
}

inline bool readInt64(FILE *file, int64_p &value) noexcept {
    if (!file)
        return false;
    if (fread(&value, sizeof(int64_p), 1, file) != 1)
        return false;
    value = fromLittleEndian(value);
    return true;
}

// 32-bit float (IEEE 754 single precision)
inline bool writeFloat32(FILE *file, float32_p value) noexcept {
    if (!file)
        return false;
    value = toLittleEndian(value);
    return fwrite(&value, sizeof(float32_p), 1, file) == 1;
}

inline bool readFloat32(FILE *file, float32_p &value) noexcept {
    if (!file)
        return false;
    if (fread(&value, sizeof(float32_p), 1, file) != 1)
        return false;
    value = fromLittleEndian(value);
    return true;
}

// 64-bit float (IEEE 754 double precision)
inline bool writeFloat64(FILE *file, float64_p value) noexcept {
    if (!file)
        return false;
    value = toLittleEndian(value);
    return fwrite(&value, sizeof(float64_p), 1, file) == 1;
}

inline bool readFloat64(FILE *file, float64_p &value) noexcept {
    if (!file)
        return false;
    if (fread(&value, sizeof(float64_p), 1, file) != 1)
        return false;
    value = fromLittleEndian(value);
    return true;
}

////////////////////////////////////////////////////////////////////////
// Vector I/O Functions
//
// Pixie uses 3D vectors (float[3]) extensively
// These functions read/write vector data portably
////////////////////////////////////////////////////////////////////////

// Write vector of floats (typically 3D vector: x,y,z)
inline bool writeVector(FILE *file, const float *vec, std::size_t count) noexcept {
    if (!file || !vec)
        return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!writeFloat32(file, vec[i]))
            return false;
    }
    return true;
}

// Read vector of floats (typically 3D vector: x,y,z)
inline bool readVector(FILE *file, float *vec, std::size_t count) noexcept {
    if (!file || !vec)
        return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!readFloat32(file, vec[i]))
            return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////
// Array I/O Functions
//
// For reading/writing arrays of fixed-width types
////////////////////////////////////////////////////////////////////////

// Write array of int32_p values
inline bool writeInt32Array(FILE *file, const int32_p *arr, std::size_t count) noexcept {
    if (!file || !arr)
        return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!writeInt32(file, arr[i]))
            return false;
    }
    return true;
}

// Read array of int32_p values
inline bool readInt32Array(FILE *file, int32_p *arr, std::size_t count) noexcept {
    if (!file || !arr)
        return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!readInt32(file, arr[i]))
            return false;
    }
    return true;
}

// Write array of float32_p values
inline bool writeFloat32Array(FILE *file, const float32_p *arr, std::size_t count) noexcept {
    if (!file || !arr)
        return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!writeFloat32(file, arr[i]))
            return false;
    }
    return true;
}

// Read array of float32_p values
inline bool readFloat32Array(FILE *file, float32_p *arr, std::size_t count) noexcept {
    if (!file || !arr)
        return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!readFloat32(file, arr[i]))
            return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////
// Utility Functions
////////////////////////////////////////////////////////////////////////

// Check if current platform is little-endian at runtime
constexpr bool isLittleEndian() noexcept {
#ifdef PIXIE_LITTLE_ENDIAN
    return true;
#else
    return false;
#endif
}

// Check if current platform is big-endian at runtime
constexpr bool isBigEndian() noexcept {
    return !isLittleEndian();
}

#endif // PORTABLE_IO_H
