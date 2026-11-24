/**
 * Project: openRender
 *
 * File: closureTree.cpp
 *
 * Description:
 *   The closure tree builder, flatten, copy and accessors (spec 035, closureTree.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "closureTree.h"

#include <string.h>

#include "closureBuiltins.h"
#include "common/diagnostics.h"

namespace openrender::shading {

    using openrender::Spectrum;

    int closureDiagnosticCode(ClosureDiagKind kind) {
        switch (kind) {
            case ClosureDiagKind::BadWeight:
                return CODE_RANGE;
            case ClosureDiagKind::BudgetExceeded:
                return CODE_LIMIT;
            case ClosureDiagKind::UnknownClosure:
                return CODE_BADTOKEN;
            case ClosureDiagKind::BadBlockSize:
                return CODE_RANGE;
            case ClosureDiagKind::BadRefractionIndex:
                return CODE_RANGE;
            case ClosureDiagKind::PointOutOfRange:
                return CODE_RANGE;
            case ClosureDiagKind::NumKinds:
                break;
        }
        return CODE_BUG;
    }

    namespace {

        size_t roundUp(size_t n, size_t to) { return (n + to - 1) / to * to; }

        bool validWeight(const float w[3]) {
            const Spectrum s = Spectrum::fromArray3(w);
            return s.isFinite() && s.isNonNegative();
        }

        // The process-wide fallback for a builder with no hook: each kind once per process
        void reportToProcess(ClosureDiagKind kind, int code, const char *message) {
            static std::atomic<int> reported[(int)ClosureDiagKind::NumKinds];
            if (reported[(int)kind].fetch_add(1) == 0)
                error((EErrorCode)code, "%s", message);
        }

    } // namespace

    ClosureTreeBuilder::ClosureTreeBuilder(CMemPage *&arena, int numRealVertices, IClosureDiagnostics *diagnostics)
        : arena_(arena), numRealVertices_(numRealVertices < 0 ? 0 : numRealVertices), diagnostics_(diagnostics),
          budget_((size_t)(numRealVertices < 0 ? 0 : numRealVertices) * kClosureTreeBytesPerPoint), used_(0), overflowed_(false), roots_(nullptr) {
        if (numRealVertices_ > 0) {
            roots_ = (const OrClosureNode **)ralloc(numRealVertices_ * (int)sizeof(const OrClosureNode *), arena_);
            for (int i = 0; i < numRealVertices_; ++i)
                roots_[i] = nullptr;
        }
    }

    void ClosureTreeBuilder::report(ClosureDiagKind kind, const char *message) {
        const int code = closureDiagnosticCode(kind);
        if (diagnostics_ != nullptr)
            diagnostics_->report(kind, code, message);
        else
            reportToProcess(kind, code, message);
    }

    // Takes `size` bytes at `align` from the arena (align is 8 or 16: ralloc hands out 8-aligned memory). The padding that gets the bump pointer
    // to the alignment counts against the budget with the node. Returns nullptr, with the overflow flag set, when it does not fit.
    void *ClosureTreeBuilder::allocate(size_t size, size_t align) {
        for (int attempt = 0; attempt < 4; ++attempt) {
            size_t pad = 0;
            if (align > 8) {
                const size_t misalignment = (size_t)((uintptr_t)arena_->memory & (align - 1));
                pad = misalignment == 0 ? 0 : align - misalignment;
            }
            if (used_ + pad + size > budget_) {
                if (!overflowed_)
                    report(ClosureDiagKind::BudgetExceeded, "The closure tree of a batch exceeds its memory budget; the affected point shades with an empty tree");
                overflowed_ = true;
                return nullptr;
            }
            if (pad != 0)
                (void)ralloc((int)pad, arena_);
            void *p = ralloc((int)size, arena_);
            if (((uintptr_t)p & (align - 1)) == 0) {
                used_ += pad + size;
                return p;
            }
            // the arena moved to a page whose base is not aligned: the bytes taken are lost to the budget, and the next attempt pads again
            used_ += pad + size;
        }
        return nullptr;
    }

    const OrClosureNode *ClosureTreeBuilder::component(int32_t id, const float w[3], const void *block, size_t blockSize) {
        const OrClosureEntry *entry = closureEntry(id);
        if (entry == nullptr) {
            report(ClosureDiagKind::UnknownClosure, "A closure node names an unregistered closure id; the node is dropped");
            return nullptr;
        }
        if (blockSize != entry->blockSize || (blockSize > 0 && block == nullptr)) {
            report(ClosureDiagKind::BadBlockSize, "A closure node's parameter block does not have the size its closure declares; the node is dropped");
            return nullptr;
        }
        if (!validWeight(w)) {
            report(ClosureDiagKind::BadWeight, "A closure node has a negative or non-finite weight; the node is dropped");
            return nullptr;
        }
        if (id == kClosureMicrofacet) {
            const ClosureMicrofacetBlock *m = (const ClosureMicrofacetBlock *)block;
            if (m->eta == 0.0f && (m->refract == kMicrofacetRefract || m->refract == kMicrofacetBoth)) {
                report(ClosureDiagKind::BadRefractionIndex, "A microfacet closure that refracts needs an index of refraction (eta > 0); the node is dropped");
                return nullptr;
            }
        }
        void *mem = allocate(roundUp(sizeof(OrClosureComponent) + blockSize, 16), 16);
        if (mem == nullptr)
            return nullptr;
        OrClosureComponent *c = (OrClosureComponent *)mem;
        c->id = id;
        c->w[0] = w[0];
        c->w[1] = w[1];
        c->w[2] = w[2];
        if (blockSize > 0)
            memcpy((char *)(c + 1), block, blockSize);
        return (const OrClosureNode *)c;
    }

    const OrClosureNode *ClosureTreeBuilder::mul(const float w[3], const OrClosureNode *child) {
        if (!validWeight(w)) {
            report(ClosureDiagKind::BadWeight, "A closure node has a negative or non-finite weight; the node is dropped");
            return nullptr;
        }
        if (child == nullptr)
            return nullptr;
        void *mem = allocate(sizeof(OrClosureMul), 8);
        if (mem == nullptr)
            return nullptr;
        OrClosureMul *m = (OrClosureMul *)mem;
        m->id = OR_CLOSURE_MUL;
        m->weight[0] = w[0];
        m->weight[1] = w[1];
        m->weight[2] = w[2];
        m->closure = child;
        return (const OrClosureNode *)m;
    }

    const OrClosureNode *ClosureTreeBuilder::add(const OrClosureNode *a, const OrClosureNode *b) {
        if (a == nullptr)
            return b;
        if (b == nullptr)
            return a;
        void *mem = allocate(sizeof(OrClosureAdd), 8);
        if (mem == nullptr)
            return nullptr;
        OrClosureAdd *n = (OrClosureAdd *)mem;
        n->id = OR_CLOSURE_ADD;
        n->closureA = a;
        n->closureB = b;
        return (const OrClosureNode *)n;
    }

    void ClosureTreeBuilder::setRoot(int point, const OrClosureNode *root) {
        if (point < 0 || point >= numRealVertices_) {
            report(ClosureDiagKind::PointOutOfRange, "A closure tree root was set for a point outside the batch; it is ignored");
            return;
        }
        if (overflowed_) {
            roots_[point] = nullptr; // this point's tree ran over the budget: it shades with an empty tree
            overflowed_ = false;
            return;
        }
        roots_[point] = root;
    }

    const OrClosureNode *ClosureTreeBuilder::root(int point) const {
        if (point < 0 || point >= numRealVertices_)
            return nullptr;
        return roots_[point];
    }

    // ---- flatten ----

    namespace {

        struct CountVisit {
                int count = 0;
                void operator()(int32_t, const Spectrum &, const void *) { ++count; }
        };
        struct FillVisit {
                ClosureLobe *out;
                int next = 0;
                void operator()(int32_t id, const Spectrum &w, const void *params) { out[next++] = ClosureLobe{id, w, params}; }
        };

    } // namespace

    ClosureLobeList closureFlatten(const OrClosureNode *tree, CMemPage *&arena) {
        ClosureLobeList list = {nullptr, 0, false};
        CountVisit counter;
        closureEvaluateGeneric(tree, counter);
        if (counter.count == 0)
            return list;
        list.lobes = (ClosureLobe *)ralloc(counter.count * (int)sizeof(ClosureLobe), arena);
        FillVisit filler{list.lobes};
        closureEvaluateGeneric(tree, filler);
        list.count = filler.next;
        return list;
    }

    // ---- copy ----

    namespace {

        const OrClosureNode *copyNode(const OrClosureNode *node, CMemPage *&arena) {
            if (node == nullptr)
                return nullptr;
            if (node->id == OR_CLOSURE_MUL) {
                const OrClosureMul *m = (const OrClosureMul *)node;
                OrClosureMul *c = (OrClosureMul *)ralloc((int)sizeof(OrClosureMul), arena);
                *c = *m;
                c->closure = copyNode(m->closure, arena);
                return (const OrClosureNode *)c;
            }
            if (node->id == OR_CLOSURE_ADD) {
                const OrClosureAdd *a = (const OrClosureAdd *)node;
                OrClosureAdd *c = (OrClosureAdd *)ralloc((int)sizeof(OrClosureAdd), arena);
                *c = *a;
                c->closureA = copyNode(a->closureA, arena);
                c->closureB = copyNode(a->closureB, arena);
                return (const OrClosureNode *)c;
            }
            const OrClosureEntry *entry = closureEntry(node->id);
            const size_t size = roundUp(sizeof(OrClosureComponent) + (entry != nullptr ? entry->blockSize : 0), 16);
            // 16-aligned: ralloc is 8-aligned, so ask for 8 more and align inside
            char *raw = (char *)ralloc((int)(size + 8), arena);
            char *aligned = (char *)(((uintptr_t)raw + 15) & ~(uintptr_t)15);
            memcpy(aligned, node, size);
            return (const OrClosureNode *)aligned;
        }

        struct SumVisit {
                int32_t wanted;
                Spectrum sum;
                void operator()(int32_t id, const Spectrum &w, const void *) {
                    if (id == wanted)
                        sum = sum + w;
                }
        };

        struct EmissionVisit {
                Spectrum sum;
                void operator()(int32_t id, const Spectrum &w, const void *params) {
                    if (id == kClosureUniformEdf) {
                        const ClosureUniformEdfBlock *e = (const ClosureUniformEdfBlock *)params;
                        sum = sum + w * Spectrum::fromArray3(e->emittance);
                    }
                }
        };

    } // namespace

    const OrClosureNode *closureTreeCopy(const OrClosureNode *tree, CMemPage *&arena) { return copyNode(tree, arena); }

    float closureTreePresence(const OrClosureNode *tree) {
        SumVisit v{kClosureTransparent, Spectrum()};
        closureEvaluateGeneric(tree, v);
        return v.sum.mean();
    }

    Spectrum closureTreeEmission(const OrClosureNode *tree) {
        EmissionVisit v;
        closureEvaluateGeneric(tree, v);
        return v.sum;
    }

    float closureTreeHoldout(const OrClosureNode *tree) {
        SumVisit v{kClosureHoldout, Spectrum()};
        closureEvaluateGeneric(tree, v);
        return v.sum.mean();
    }

} // namespace openrender::shading
