/**
 * Project: openRender
 *
 * File: giHash.h
 *
 * Description:
 *   The hashes of the global-illumination step that are not draws (spec 040, decision D1-A): the accept value of an irradiance record for a
 *   lookup, and the identity of a record. They wrap what sampling.h keeps in `detail` (mix, toUnitFloat) as public functions, so that
 *   Texturing (the store computes a record's identity) and Shading (the accept function it hands the store) name no `sampling::detail`.
 *   Everything here is a pure function of its arguments.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef GIHASH_H
#define GIHASH_H

#include <stdint.h>
#include <string.h>

#include "common/sampling.h"

namespace openrender::sampling {

    // Combines a hash with two more values (asymmetric; sampling.h's detail::mix)
    inline uint64_t giMix(uint64_t h, uint64_t a, uint64_t b) { return detail::mix(h, a, b); }

    // The accept value of the record `recordIdentity` for the lookup of the call whose sub-stream is (seedHash, indexBase): a pure function of
    // its arguments in [0, 1), taken from the top 24 bits of the hash as the sampler's own float conversion does. Not a draw: it consumes no
    // position of any stream and does not depend on how many records the lookup visited before this one.
    inline float giAcceptValue(uint64_t seedHash, uint32_t indexBase, uint64_t recordIdentity) {
        return detail::toUnitFloat(static_cast<uint32_t>(detail::mix(0xACCE7EDull, seedHash, indexBase, recordIdentity) >> 32));
    }

    // The identity of an irradiance record: its key when it has one (it was made in this process or came over the wire), else (a record read
    // from a cache file carries no key) a hash of the bit patterns of its P and N. Never 0.
    inline uint64_t giRecordIdentity(uint64_t key, const float *P, const float *N) {
        if (key != 0) {
            return key;
        }
        uint32_t bits[6];
        memcpy(bits, P, 3 * sizeof(float));
        memcpy(bits + 3, N, 3 * sizeof(float));
        uint64_t h = 0x1D3A7F11ull;
        for (int i = 0; i < 6; i += 2) {
            h = detail::mix(h, bits[i], bits[i + 1]);
        }
        return h != 0 ? h : 1;
    }

} // namespace openrender::sampling

#endif
