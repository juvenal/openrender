/**
 * Project: Pixie
 *
 * File: atomic_modern.h
 *
 * Description:
 *   Modern C++20 atomic operations using std::atomic
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
//  File				:	atomic_modern.h
//  Description			:	Modern C++20 atomic operations
//							Replaces platform-specific inline assembly
//							with standard std::atomic
//
//	Created				:	2025 (C++20 modernization)
//	Purpose				:	Provide thread-safe atomic operations using
//							C++20 std::atomic instead of:
//							  - Windows InterlockedIncrement/Decrement
//							  - macOS OSAtomic (deprecated since 10.12)
//							  - GCC x86/x86_64 inline assembly
//							  - GCC PowerPC inline assembly
//							  - Generic mutex fallback
//
//	Benefits			:	- Native ARM64 support (no mutex fallback)
//							- Explicit memory ordering semantics
//							- Cross-platform by default
//							- Better compiler optimization opportunities
//							- Removes ~140 lines of platform-specific code
//
////////////////////////////////////////////////////////////////////////
#ifndef ATOMIC_MODERN_H
#define ATOMIC_MODERN_H

#include <atomic>
#include <cstdint>

////////////////////////////////////////////////////////////////////////
// Modern C++20 Atomic Types
//
// Use std::atomic<T> for all atomic operations
// Provides lock-free atomic operations on all modern platforms
////////////////////////////////////////////////////////////////////////

// Primary atomic type for reference counting
using atomic_int32 = std::atomic<std::int32_t>;

////////////////////////////////////////////////////////////////////////
// Atomic Increment Operation
//
// Atomically increments the value and returns the NEW value (post-increment)
//
// Memory Ordering: acquire-release
//   - Acquire: Subsequent reads/writes cannot be reordered before this operation
//   - Release: Previous reads/writes cannot be reordered after this operation
//   - This ensures proper synchronization for reference counting
//
// Return: The value AFTER incrementing
//
// Example:
//   std::atomic<int32_t> refCount{0};
//   int32_t newCount = atomicIncrement(refCount);  // Returns 1
////////////////////////////////////////////////////////////////////////
inline std::int32_t atomicIncrement(atomic_int32& counter) noexcept {
    // fetch_add returns the OLD value, so we add 1 to get the NEW value
    // This matches the behavior of the old atomic.h implementation
    return counter.fetch_add(1, std::memory_order_acq_rel) + 1;
}

////////////////////////////////////////////////////////////////////////
// Atomic Decrement Operation
//
// Atomically decrements the value and returns the NEW value (post-decrement)
//
// Memory Ordering: acquire-release
//   - Same synchronization guarantees as atomicIncrement
//
// Return: The value AFTER decrementing
//
// Example:
//   std::atomic<int32_t> refCount{5};
//   int32_t newCount = atomicDecrement(refCount);  // Returns 4
//   if (newCount == 0) {
//       // Safe to delete - no other threads have a reference
//   }
////////////////////////////////////////////////////////////////////////
inline std::int32_t atomicDecrement(atomic_int32& counter) noexcept {
    // fetch_sub returns the OLD value, so we subtract 1 to get the NEW value
    // This matches the behavior of the old atomic.h implementation
    return counter.fetch_sub(1, std::memory_order_acq_rel) - 1;
}

////////////////////////////////////////////////////////////////////////
// Legacy Compatibility Wrapper (TEMPORARY)
//
// Provides compatibility with old code that uses volatile int* pointers
// This allows gradual migration from the old atomic.h
//
// WARNING: This is UNSAFE if the pointer doesn't actually point to
//          std::atomic<int32_t>. Only use during migration period.
//
// Migration Strategy:
//   1. Replace volatile int with std::atomic<int32_t>
//   2. Update call sites to pass by reference (not pointer)
//   3. Remove these wrapper functions once migration is complete
////////////////////////////////////////////////////////////////////////

// DEPRECATED: Legacy pointer-based increment
// TODO: Remove after migrating all code to use atomic_int32&
inline std::int32_t atomicIncrement(volatile std::int32_t* ptr) noexcept {
    // SAFETY WARNING: This assumes ptr actually points to std::atomic<int32_t>
    // This is only safe because std::atomic<int32_t> has the same layout as int32_t
    // on all supported platforms (guaranteed by standard for trivial types)
    auto* atomic_ptr = reinterpret_cast<atomic_int32*>(const_cast<std::int32_t*>(ptr));
    return atomicIncrement(*atomic_ptr);
}

// DEPRECATED: Legacy pointer-based decrement
// TODO: Remove after migrating all code to use atomic_int32&
inline std::int32_t atomicDecrement(volatile std::int32_t* ptr) noexcept {
    // SAFETY WARNING: Same as above - only safe if ptr points to std::atomic
    auto* atomic_ptr = reinterpret_cast<atomic_int32*>(const_cast<std::int32_t*>(ptr));
    return atomicDecrement(*atomic_ptr);
}

////////////////////////////////////////////////////////////////////////
// Memory Ordering Notes
//
// We use memory_order_acq_rel (acquire-release) for reference counting:
//
// Acquire semantics (on read):
//   - Prevents reordering of subsequent reads/writes before this operation
//   - Ensures we see all writes made by the releasing thread
//
// Release semantics (on write):
//   - Prevents reordering of previous reads/writes after this operation
//   - Ensures our writes are visible to threads that acquire
//
// For reference counting, this guarantees:
//   - When incrementing: We see the object in a valid state
//   - When decrementing to 0: All our modifications are visible before deletion
//
// Alternative memory orders (not used here):
//   - memory_order_relaxed: No synchronization (fastest, but unsafe for refcounts)
//   - memory_order_seq_cst: Sequential consistency (strongest, unnecessary here)
//   - memory_order_consume: Deprecated in C++17, use acquire instead
//
// Performance: acq_rel is typically as fast as relaxed on modern CPUs with
// strong memory models (x86/x64), while providing necessary guarantees on
// weaker memory models (ARM, PowerPC).
////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_MODERN_H
