/**
 * Project: openRender
 *
 * File: bsdf.cpp
 *
 * Description:
 *   The tree-level BSDF table and its C++ wrapper (spec 035, contracts/closure-abi.md "BSDF table", conventions 1 to 10).
 *
 *   prepare() walks the tree once (closureEvaluateGeneric: the generic walk, so the lobes are in tree order with their accumulated weights) and
 *   copies every BSDF lobe into the caller's allocator with its own local frame, built from the lobe's own normal N (or the shading frame's
 *   when the block has none). evaluate(), sample() and pdf() are pure functions of the prepared lobes and their arguments: no state, no random
 *   numbers (u1, u2 and uLobe come from the caller), so the same inputs give the same bytes on any thread.
 *
 *   Lobe choice (convention 4): the probability of lobe i is mean(weight_i) * albedo_i(wo) over the sum of that over the requested lobes. A
 *   non-delta density is the mixture of the lobes' densities with those probabilities; the reverse density uses the probabilities of the swapped
 *   query (wi as the outgoing direction). A delta lobe adds nothing to f or to the density: evaluate and pdf report it with OR_BSDF_HAS_DELTA, and
 *   sample may choose it, returning the Fresnel-weighted delta value and the discrete choice probability.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "bsdf.hpp"

#include <math.h>
#include <string.h>

#include "bsdfComponents.h"
#include "bsdfMath.h"
#include "closureBuiltins.h"
#include "closureRegistry.h"
#include "closureTree.h"

using namespace openrender::shading;

namespace {

    struct Lobe {
            int32_t id;
            const OrBsdfComponentOps *ops;
            const void *block;
            float weight[3];
            float mean; // the channel mean of the weight
            uint32_t kind;
            bool delta;
            float t[3], b[3], n[3]; // the lobe's local frame: its own normal is n
    };

    bool finite3(const float v[3]) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }

    void toLocal(const Lobe &l, const float v[3], float out[3]) {
        out[0] = (float)((double)v[0] * l.t[0] + (double)v[1] * l.t[1] + (double)v[2] * l.t[2]);
        out[1] = (float)((double)v[0] * l.b[0] + (double)v[1] * l.b[1] + (double)v[2] * l.b[2]);
        out[2] = (float)((double)v[0] * l.n[0] + (double)v[1] * l.n[1] + (double)v[2] * l.n[2]);
    }
    void toWorld(const Lobe &l, const float v[3], float out[3]) {
        for (int i = 0; i < 3; ++i)
            out[i] = (float)((double)v[0] * l.t[i] + (double)v[1] * l.b[i] + (double)v[2] * l.n[i]);
    }

    // a lobe is requested when the mask holds one of its scattering bits (reflection, transmission) and one of its kind bits (diffuse, glossy, specular)
    bool requestedLobe(const Lobe &l, uint32_t requested) {
        return (requested & l.kind & (OR_BSDF_REFLECTION | OR_BSDF_TRANSMISSION)) != 0 && (requested & l.kind & (OR_BSDF_DIFFUSE | OR_BSDF_GLOSSY | OR_BSDF_SPECULAR)) != 0;
    }

} // namespace

struct OrBsdf {
        int count;
        Lobe *lobes;
};

namespace {

    // the sum over the requested lobes of mean(weight) * albedo(direction in the lobe's frame)
    double selectionTotal(const OrBsdf *bsdf, const float dir[3], uint32_t requested) {
        double total = 0.0;
        for (int i = 0; i < bsdf->count; ++i) {
            const Lobe &l = bsdf->lobes[i];
            if (!requestedLobe(l, requested))
                continue;
            float local[3];
            toLocal(l, dir, local);
            total += (double)l.mean * (double)l.ops->albedo(l.block, local);
        }
        return total;
    }
    double selectionWeight(const Lobe &l, const float dir[3]) {
        float local[3];
        toLocal(l, dir, local);
        return (double)l.mean * (double)l.ops->albedo(l.block, local);
    }

    OrStatus checkQuery(const OrBsdfQuery *q, bool needWi) {
        if (q == nullptr || q->structSize < sizeof(OrBsdfQuery))
            return OR_STATUS_BAD_ARGUMENT;
        if (!finite3(q->wo) || (needWi && !finite3(q->wi)))
            return OR_STATUS_BAD_ARGUMENT;
        return OR_STATUS_OK;
    }

    void clearEval(OrBsdfEval *o) {
        memset(o, 0, sizeof(*o));
        o->numChannels = 3;
    }

    // the mixture answer for (wo, wi): f, both densities and the flags
    void evaluateMixture(const OrBsdf *bsdf, const OrBsdfQuery *q, bool wantF, OrBsdfEval *out) {
        clearEval(out);
        if (bsdf->count == 0) {
            out->flags = OR_BSDF_NOT_SCATTERER;
            return;
        }
        const double A = selectionTotal(bsdf, q->wo, q->requested), Ar = selectionTotal(bsdf, q->wi, q->requested);
        if (!(A > 0.0))
            return;
        double f[3] = {0, 0, 0}, pdfF = 0.0, pdfR = 0.0;
        uint32_t flags = 0;
        for (int i = 0; i < bsdf->count; ++i) {
            const Lobe &l = bsdf->lobes[i];
            if (!requestedLobe(l, q->requested))
                continue;
            if (l.delta) {
                flags |= OR_BSDF_HAS_DELTA;
                continue;
            }
            float wo[3], wi[3];
            toLocal(l, q->wo, wo);
            toLocal(l, q->wi, wi);
            OrBsdfEval e;
            l.ops->evaluate(l.block, wo, wi, q->mode, &e);
            if (!(e.f[0] > 0.0f) && !(e.pdfForward > 0.0f) && !(e.pdfReverse > 0.0f))
                continue;
            const double p = (double)l.mean * (double)l.ops->albedo(l.block, wo) / A;
            const double pr = Ar > 0.0 ? (double)l.mean * (double)l.ops->albedo(l.block, wi) / Ar : 0.0;
            if (wantF)
                for (int c = 0; c < 3; ++c)
                    f[c] += (double)l.weight[c] * (double)e.f[c];
            pdfF += p * (double)e.pdfForward;
            pdfR += pr * (double)e.pdfReverse;
            if (e.f[0] > 0.0f)
                flags |= e.flags;
        }
        for (int c = 0; c < 3; ++c)
            out->f[c] = (float)f[c];
        out->pdfForward = (float)pdfF;
        out->pdfReverse = (float)pdfR;
        out->flags = flags;
        if (!std::isfinite(out->f[0]) || !std::isfinite(out->f[1]) || !std::isfinite(out->f[2]) || !std::isfinite(out->pdfForward) || !std::isfinite(out->pdfReverse)) {
            clearEval(out); // never a non-finite value (convention 6)
            out->flags = OR_BSDF_FAILED;
        }
    }

    OrStatus bsdfEvaluate(const OrBsdf *bsdf, const OrBsdfQuery *q, OrBsdfEval *out) {
        if (bsdf == nullptr || out == nullptr)
            return OR_STATUS_BAD_ARGUMENT;
        if (OrStatus s = checkQuery(q, true))
            return s;
        evaluateMixture(bsdf, q, true, out);
        return OR_STATUS_OK;
    }

    OrStatus bsdfPdf(const OrBsdf *bsdf, const OrBsdfQuery *q, OrBsdfEval *out) {
        if (bsdf == nullptr || out == nullptr)
            return OR_STATUS_BAD_ARGUMENT;
        if (OrStatus s = checkQuery(q, true))
            return s;
        evaluateMixture(bsdf, q, false, out);
        return OR_STATUS_OK;
    }

    void failedSample(OrBsdfSample *out, uint32_t flags) {
        memset(out, 0, sizeof(*out));
        out->numChannels = 3;
        out->flags = flags;
        out->eta = 1.0f;
    }

    OrStatus bsdfSample(const OrBsdf *bsdf, const OrBsdfQuery *q, float u1, float u2, float uLobe, OrBsdfSample *out) {
        if (bsdf == nullptr || out == nullptr)
            return OR_STATUS_BAD_ARGUMENT;
        if (OrStatus s = checkQuery(q, false))
            return s;
        const auto bad = [](float u) { return !(u >= 0.0f && u <= 1.0f); }; // also true for NaN
        if (bad(u1) || bad(u2) || bad(uLobe))
            return OR_STATUS_BAD_ARGUMENT;
        if (bsdf->count == 0) {
            failedSample(out, OR_BSDF_NOT_SCATTERER);
            return OR_STATUS_OK;
        }
        const double A = selectionTotal(bsdf, q->wo, q->requested);
        if (!(A > 0.0)) {
            failedSample(out, OR_BSDF_FAILED);
            return OR_STATUS_OK;
        }
        // the lobe: the first whose cumulative weight passes uLobe * A
        const double target = (double)uLobe * A;
        double cumulative = 0.0;
        int chosen = -1, last = -1;
        for (int i = 0; i < bsdf->count; ++i) {
            const Lobe &l = bsdf->lobes[i];
            if (!requestedLobe(l, q->requested))
                continue;
            last = i;
            cumulative += selectionWeight(l, q->wo);
            if (target < cumulative) {
                chosen = i;
                break;
            }
        }
        if (chosen < 0)
            chosen = last; // rounding at the top of the interval
        const Lobe &l = bsdf->lobes[chosen];
        float wo[3];
        toLocal(l, q->wo, wo);
        OrBsdfSample s;
        memset(&s, 0, sizeof(s));
        l.ops->sample(l.block, wo, u1, u2, q->mode, &s);
        if ((s.flags & OR_BSDF_FAILED) || !(s.f[0] > 0.0f) || !finite3(s.wi)) {
            failedSample(out, OR_BSDF_FAILED);
            return OR_STATUS_OK;
        }
        float wiWorld[3];
        toWorld(l, s.wi, wiWorld);
        const double p = selectionWeight(l, q->wo) / A;
        if (s.flags & OR_BSDF_DELTA) {
            // the discrete choice: the density fields hold probabilities
            const double Ar = selectionTotal(bsdf, wiWorld, q->requested);
            const double pr = Ar > 0.0 ? selectionWeight(l, wiWorld) / Ar : 0.0;
            memset(out, 0, sizeof(*out));
            out->numChannels = 3;
            for (int c = 0; c < 3; ++c)
                out->f[c] = (float)((double)l.weight[c] * (double)s.f[0]);
            out->pdfForward = (float)(p * (double)s.pdfForward);
            out->pdfReverse = (float)(pr * (double)s.pdfReverse);
            out->flags = s.flags;
            memcpy(out->wi, wiWorld, sizeof(wiWorld));
            out->eta = s.eta;
            if (!(out->pdfForward > 0.0f) || !std::isfinite(out->f[0])) {
                failedSample(out, OR_BSDF_FAILED);
            }
            return OR_STATUS_OK;
        }
        // a non-delta direction: the answer is the whole mixture's at (wo, wi), so f and the density agree with evaluate and pdf of that direction
        OrBsdfQuery inner = *q;
        memcpy(inner.wi, wiWorld, sizeof(wiWorld));
        OrBsdfEval e;
        evaluateMixture(bsdf, &inner, true, &e);
        if (!(e.pdfForward > 0.0f) || !(e.f[0] > 0.0f) || (e.flags & OR_BSDF_FAILED)) {
            failedSample(out, OR_BSDF_FAILED);
            return OR_STATUS_OK;
        }
        memset(out, 0, sizeof(*out));
        out->numChannels = 3;
        memcpy(out->f, e.f, sizeof(out->f));
        out->pdfForward = e.pdfForward;
        out->pdfReverse = e.pdfReverse;
        out->flags = e.flags | (s.flags & (OR_BSDF_REFLECTION | OR_BSDF_TRANSMISSION | OR_BSDF_DIFFUSE | OR_BSDF_GLOSSY | OR_BSDF_SPECULAR));
        memcpy(out->wi, wiWorld, sizeof(wiWorld));
        out->eta = s.eta;
        return OR_STATUS_OK;
    }

    // ---- prepare ----

    // the lobe's own normal: the block's `N` parameter, or the shading frame's when it has none or it is unusable
    void lobeNormal(const OrClosureEntry *entry, const void *block, const OrShadingFrame &frame, double n[3]) {
        n[0] = frame.N[0];
        n[1] = frame.N[1];
        n[2] = frame.N[2];
        for (int i = 0; i < entry->numParams; ++i) {
            const OrClosureParam &p = entry->params[i];
            if (p.type == OR_CPARAM_NORMAL && strcmp(p.name, "N") == 0) {
                float v[3];
                memcpy(v, (const char *)block + p.offset, sizeof(v));
                const double l = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
                if (finite3(v) && l > 1e-6) {
                    n[0] = v[0] / l;
                    n[1] = v[1] / l;
                    n[2] = v[2] / l;
                }
                break;
            }
        }
        const double l = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (std::isfinite(l) && l > 1e-6) {
            n[0] /= l;
            n[1] /= l;
            n[2] /= l;
        }
        else {
            n[0] = 0.0;
            n[1] = 0.0;
            n[2] = 1.0;
        }
    }

    struct Counter {
            int count = 0;
            bool refused = false;
            void operator()(int32_t id, const openrender::Spectrum &, const void *params) {
                const OrClosureEntry *e = closureEntry(id);
                if (e == nullptr || e->category != OR_CLOSURE_BSDF || e->ops == nullptr)
                    return;
                if (id == kClosureMicrofacet) {
                    const ClosureMicrofacetBlock *m = (const ClosureMicrofacetBlock *)params;
                    if (!(m->eta > 0.0f) && (m->refract == kMicrofacetRefract || m->refract == kMicrofacetBoth))
                        refused = true; // a refraction needs an index (the builder refuses this node; a forged one is refused here)
                }
                ++count;
            }
    };

    struct Filler {
            const OrShadingFrame *frame;
            const OrClosureAllocator *alloc;
            Lobe *out;
            int next = 0;
            bool outOfMemory = false;
            void operator()(int32_t id, const openrender::Spectrum &w, const void *params) {
                const OrClosureEntry *e = closureEntry(id);
                if (e == nullptr || e->category != OR_CLOSURE_BSDF || e->ops == nullptr || outOfMemory)
                    return;
                Lobe &l = out[next++];
                memset(&l, 0, sizeof(l));
                l.id = id;
                l.ops = e->ops;
                void *copy = nullptr;
                if (e->blockSize > 0) {
                    copy = alloc->allocate(alloc->context, e->blockSize, e->blockAlign > 8 ? e->blockAlign : 8);
                    if (copy == nullptr) {
                        outOfMemory = true;
                        return;
                    }
                    memcpy(copy, params, e->blockSize);
                }
                l.block = copy;
                for (int c = 0; c < 3; ++c)
                    l.weight[c] = w.c[c];
                l.mean = w.mean();
                l.kind = bsdfLobeKind(id, l.block, &l.delta);
                double n[3], t[3], b[3];
                lobeNormal(e, l.block, *frame, n);
                bsdf::orthonormalBasis(n, t, b);
                for (int i = 0; i < 3; ++i) {
                    l.n[i] = (float)n[i];
                    l.t[i] = (float)t[i];
                    l.b[i] = (float)b[i];
                }
            }
    };

    OrStatus bsdfPrepare(const OrClosureNode *tree, const OrShadingFrame *frame, const OrClosureAllocator *alloc, OrBsdf **out) {
        if (frame == nullptr || alloc == nullptr || alloc->allocate == nullptr || out == nullptr)
            return OR_STATUS_BAD_ARGUMENT;
        *out = nullptr;
        Counter counter;
        closureEvaluateGeneric(tree, counter);
        if (counter.refused)
            return OR_STATUS_BAD_ARGUMENT;
        OrBsdf *b = (OrBsdf *)alloc->allocate(alloc->context, sizeof(OrBsdf), alignof(OrBsdf));
        if (b == nullptr)
            return OR_STATUS_OUT_OF_MEMORY;
        b->count = 0;
        b->lobes = nullptr;
        if (counter.count > 0) {
            b->lobes = (Lobe *)alloc->allocate(alloc->context, sizeof(Lobe) * (size_t)counter.count, alignof(Lobe));
            if (b->lobes == nullptr)
                return OR_STATUS_OUT_OF_MEMORY;
            Filler filler{frame, alloc, b->lobes};
            closureEvaluateGeneric(tree, filler);
            if (filler.outOfMemory)
                return OR_STATUS_OUT_OF_MEMORY;
            b->count = filler.next;
        }
        *out = b;
        return OR_STATUS_OK;
    }

    const OrBsdfTable kTable = {{(uint32_t)sizeof(OrBsdfTable), (uint32_t)OR_CLOSURE_ABI_VERSION}, bsdfPrepare, bsdfEvaluate, bsdfSample, bsdfPdf};

    void *arenaAllocate(void *context, size_t size, size_t align) {
        CMemPage *&arena = *(CMemPage **)context;
        char *raw = (char *)ralloc((int)(size + align), arena);
        return (void *)(((uintptr_t)raw + (align - 1)) & ~(uintptr_t)(align - 1));
    }

} // namespace

OR_EXTERN_C_BEGIN
const OrBsdfTable *orBsdfTable(void) { return &kTable; }
OR_EXTERN_C_END

namespace openrender::shading {

    Bsdf Bsdf::prepare(const OrClosureNode *tree, const OrShadingFrame &frame, CMemPage *&arena) {
        OrClosureAllocator alloc = {&arena, arenaAllocate};
        OrBsdf *h = nullptr;
        if (kTable.prepare(tree, &frame, &alloc, &h) != OR_STATUS_OK)
            return Bsdf();
        return Bsdf(h);
    }

    BsdfEvalResult Bsdf::evaluate(const OrBsdfQuery &q) const {
        OrBsdfEval e;
        memset(&e, 0, sizeof(e));
        BsdfEvalResult r;
        r.status = kTable.evaluate(handle_, &q, &e);
        r.f = openrender::Spectrum::fromArray3(e.f);
        r.pdfForward = e.pdfForward;
        r.pdfReverse = e.pdfReverse;
        r.flags = e.flags;
        return r;
    }

    BsdfEvalResult Bsdf::pdf(const OrBsdfQuery &q) const {
        OrBsdfEval e;
        memset(&e, 0, sizeof(e));
        BsdfEvalResult r;
        r.status = kTable.pdf(handle_, &q, &e);
        r.f = openrender::Spectrum::fromArray3(e.f);
        r.pdfForward = e.pdfForward;
        r.pdfReverse = e.pdfReverse;
        r.flags = e.flags;
        return r;
    }

    BsdfSampleResult Bsdf::sample(const OrBsdfQuery &q, float u1, float u2, float uLobe) const {
        OrBsdfSample s;
        memset(&s, 0, sizeof(s));
        BsdfSampleResult r;
        r.status = kTable.sample(handle_, &q, u1, u2, uLobe, &s);
        r.f = openrender::Spectrum::fromArray3(s.f);
        r.pdfForward = s.pdfForward;
        r.pdfReverse = s.pdfReverse;
        r.flags = s.flags;
        memcpy(r.wi, s.wi, sizeof(r.wi));
        r.eta = s.eta;
        return r;
    }

} // namespace openrender::shading
