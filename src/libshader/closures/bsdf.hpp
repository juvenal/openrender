/**
 * Project: openRender
 *
 * File: bsdf.hpp
 *
 * Description:
 *   The C++ wrapper of the BSDF table (spec 035, contracts/closure-abi.md "BSDF table"): prepare a tree once, then evaluate, sample and ask the density
 *   as often as needed; the answers carry a Spectrum f. The tree-level table itself is the C function table orBsdfTable() of closureAbi.h; this
 *   header only gives it a typed face. Internal to Shading (not installed).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef BSDF_HPP
#define BSDF_HPP

#include <stdint.h>

#include "common/memory.h"
#include "common/spectrum.h"
#include "closureAbi.h"

namespace openrender::shading {

    struct BsdfEvalResult {
            openrender::Spectrum f;
            float pdfForward, pdfReverse;
            uint32_t flags;
            OrStatus status;
    };

    struct BsdfSampleResult {
            openrender::Spectrum f;
            float pdfForward, pdfReverse;
            uint32_t flags;
            float wi[3];
            float eta;
            OrStatus status;
    };

    class Bsdf {
        public:
            Bsdf() : handle_(nullptr) {}

            // Prepares `tree` (nullptr is the empty tree) into `arena`; the result is valid until the arena's batch ends. An invalid Bsdf is returned
            // when the table refuses the tree.
            static Bsdf prepare(const OrClosureNode *tree, const OrShadingFrame &frame, CMemPage *&arena);

            bool valid() const { return handle_ != nullptr; }
            const OrBsdf *handle() const { return handle_; }

            BsdfEvalResult evaluate(const OrBsdfQuery &q) const;
            BsdfEvalResult pdf(const OrBsdfQuery &q) const; // f is left 0
            BsdfSampleResult sample(const OrBsdfQuery &q, float u1, float u2, float uLobe) const;

        private:
            explicit Bsdf(OrBsdf *h) : handle_(h) {}
            OrBsdf *handle_;
    };

} // namespace openrender::shading

#endif
