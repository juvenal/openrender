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

#include <cstring>
#include <initializer_list>

namespace openrender::sampling {

    namespace {


        template <class Draw>
        auto cameraDraw(const CSamplingConfig &cfg, const SampleId &id, uint32_t slot, Draw draw) {
            if (cfg.backend == Backend::Sobol)
                return draw(true, detail::mix(detail::kCameraSequenceSalt, id.frame, id.seed), slot, detail::cameraSequenceIndex(cfg, id));
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

    const char *samplerName(Backend b) {
        switch (b) {
            case Backend::Sobol:
                return "sobol";
            case Backend::PMJ02:
                return "pmj02";
            case Backend::Independent:
            default:
                return "independent";
        }
    }

    bool samplerFromName(const char *name, Backend &out) {
        if (name == nullptr)
            return false;
        for (Backend b : {Backend::Sobol, Backend::PMJ02, Backend::Independent})
            if (std::strcmp(name, samplerName(b)) == 0) {
                out = b;
                return true;
            }
        return false;
    }

    namespace detail {

        Sample2D sobolUnscrambled(uint32_t index) {
            return Sample2D{toUnitFloat(sobolDim0(index)), toUnitFloat(sobolDim1(index))};
        }

    } // namespace detail

} // namespace openrender::sampling
