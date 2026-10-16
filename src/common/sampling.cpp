/**
 * Project: openRender
 *
 * File: sampling.cpp
 *
 * Description:
 *   The non-inline parts of the sampler library: the camera slots and the
 *   unscrambled Sobol reference (see sampling.h).
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

        // All of a pixel's camera samples come from one sub-stream seeded by the
        // pixel's path, indexed by the sample's position inside the pixel, so they
        // form one stratified set. The slot is the dimension.
        SubstreamKey cameraStream(const SampleId &id) {
            return SubstreamKey{cameraKey(id).path, 0u};
        }

    } // namespace

    Sample2D cameraFilm(const CSamplingConfig &cfg, const SampleId &id) {
        return get2D(cfg.backend, cameraStream(id), Film2D, id.sampleIndex);
    }

    Sample2D cameraLens(const CSamplingConfig &cfg, const SampleId &id) {
        return get2D(cfg.backend, cameraStream(id), Lens2D, id.sampleIndex);
    }

    float cameraTime(const CSamplingConfig &cfg, const SampleId &id) {
        return get1D(cfg.backend, cameraStream(id), Time1D, id.sampleIndex);
    }

    float cameraImportance(const CSamplingConfig &cfg, const SampleId &id) {
        return get1D(cfg.backend, cameraStream(id), Importance1D, id.sampleIndex);
    }

    namespace detail {

        // mortonIndex is declared in sampling.h and defined with the Morton ordering
        // (spec 039 Phase 2, amendment).

        uint64_t cameraSequenceIndex(const CSamplingConfig &, const SampleId &id) {
            return id.sampleIndex;
        }

        Sample2D sobolUnscrambled(uint32_t index) {
            return Sample2D{toUnitFloat(sobolDim0(index)), toUnitFloat(sobolDim1(index))};
        }

    } // namespace detail

} // namespace openrender::sampling
