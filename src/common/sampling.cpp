/**
 * Project: openRender
 *
 * File: sampling.cpp
 *
 * Description:
 *   The non-inline parts of the sampler library: the camera slots, the Morton
 *   ordering of the Sobol camera sequence and the unscrambled Sobol reference
 *   (see sampling.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "sampling.h"

namespace openrender::sampling {

    namespace {

        // Smallest k with 2^k >= n.
        unsigned ceilLog2(int n) {
            unsigned k = 0;
            while ((1ll << k) < n)
                ++k;
            return k;
        }

        // The Sobol camera sequence is one sequence for the whole frame, seeded by the
        // frame and the seed only: a pixel is a block of the sequence, not a sequence
        // of its own (research R2.1). The other backends keep one sub-stream per pixel,
        // seeded by the pixel's path and indexed by the sample's position in the pixel.
        constexpr uint64_t kCameraSequenceSalt = 0xCA3E5ull;

        template <class Draw>
        auto cameraDraw(const CSamplingConfig &cfg, const SampleId &id, uint32_t slot, Draw draw) {
            if (cfg.backend == Backend::Sobol)
                return draw(true, detail::mix(kCameraSequenceSalt, id.frame, id.seed), slot, detail::cameraSequenceIndex(cfg, id));
            return draw(false, cameraKey(id).path, slot, static_cast<uint64_t>(id.sampleIndex));
        }

    } // namespace

    Sample2D cameraFilm(const CSamplingConfig &cfg, const SampleId &id) {
        return cameraDraw(cfg, id, Film2D, [&](bool sequence, uint64_t seed, uint32_t slot, uint64_t index) {
            return sequence ? detail::sobolGet2D64(seed, slot, index)
                            : get2D(cfg.backend, SubstreamKey{seed, 0u}, slot, static_cast<uint32_t>(index));
        });
    }

    Sample2D cameraLens(const CSamplingConfig &cfg, const SampleId &id) {
        return cameraDraw(cfg, id, Lens2D, [&](bool sequence, uint64_t seed, uint32_t slot, uint64_t index) {
            return sequence ? detail::sobolGet2D64(seed, slot, index)
                            : get2D(cfg.backend, SubstreamKey{seed, 0u}, slot, static_cast<uint32_t>(index));
        });
    }

    float cameraTime(const CSamplingConfig &cfg, const SampleId &id) {
        return cameraDraw(cfg, id, Time1D, [&](bool sequence, uint64_t seed, uint32_t slot, uint64_t index) {
            return sequence ? detail::sobolGet1D64(seed, slot, index)
                            : get1D(cfg.backend, SubstreamKey{seed, 0u}, slot, static_cast<uint32_t>(index));
        });
    }

    float cameraImportance(const CSamplingConfig &cfg, const SampleId &id) {
        return cameraDraw(cfg, id, Importance1D, [&](bool sequence, uint64_t seed, uint32_t slot, uint64_t index) {
            return sequence ? detail::sobolGet1D64(seed, slot, index)
                            : get1D(cfg.backend, SubstreamKey{seed, 0u}, slot, static_cast<uint32_t>(index));
        });
    }

    namespace detail {

        uint32_t mortonIndex(int32_t px, int32_t py) {
            // Spreads the low 16 bits of v to the even bit positions.
            auto spread = [](uint32_t v) {
                v &= 0xFFFFu;
                v = (v | (v << 8)) & 0x00FF00FFu;
                v = (v | (v << 4)) & 0x0F0F0F0Fu;
                v = (v | (v << 2)) & 0x33333333u;
                v = (v | (v << 1)) & 0x55555555u;
                return v;
            };
            return spread(static_cast<uint32_t>(px)) | (spread(static_cast<uint32_t>(py)) << 1);
        }

        uint64_t cameraSequenceIndex(const CSamplingConfig &cfg, const SampleId &id) {
            if (cfg.backend != Backend::Sobol)
                return id.sampleIndex;
            return (static_cast<uint64_t>(mortonIndex(id.px, id.py)) << ceilLog2(cfg.nx * cfg.ny)) + id.sampleIndex;
        }

        Sample2D sobolUnscrambled(uint32_t index) {
            return Sample2D{toUnitFloat(sobolDim0(index)), toUnitFloat(sobolDim1(index))};
        }

    } // namespace detail

} // namespace openrender::sampling
