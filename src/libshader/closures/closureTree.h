/**
 * Project: openRender
 *
 * File: closureTree.h
 *
 * Description:
 *   The closure tree's C++ view (spec 035, contracts/closure-abi.md "Building, flattening, lifetime"): the builder, the flattened lobe
 *   list, the generic walk and the three accessors. Internal to Shading (not installed).
 *
 *   ACCUMULATED WEIGHT (the definition, maintainer 2026-11-24): the root's accumulated weight is exactly 1. The accumulated weight of a
 *   child is its parent's accumulated weight times the parent's own weight, ONE float multiplication per node, root to leaf. A component's
 *   lobe weight is its parent's accumulated weight times the component's own `w`. A lobe whose weight is zero in every channel is dropped,
 *   and a subtree under a zero multiply produces no lobe. closureFlatten and closureEvaluateGeneric both obey it, so they agree bit for bit.
 *
 *   A tree is built on the batch's threadMemory page (the arena) and is valid until that batch's memEnd; there is no per-node free.
 *   closureTreeCopy keeps one longer. The per-batch byte budget is numRealVertices * kClosureTreeBytesPerPoint.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CLOSURETREE_H
#define CLOSURETREE_H

#include <stddef.h>
#include <stdint.h>

#include <atomic>

#include "closureAbi.h"
#include "closureRegistry.h"
#include "common/memory.h"
#include "common/spectrum.h"

namespace openrender::shading {

    // The per-batch budget of a tree's bytes, per real vertex (Q12, approved)
    inline constexpr size_t kClosureTreeBytesPerPoint = 1024;

    // What the builder can report. Each kind is reported ONCE per frame (ClosureFrameDiagnostics); closureDiagnosticCode names the
    // EErrorCode (common/diagnostics.h) of the report: BudgetExceeded is CODE_LIMIT, BadWeight CODE_RANGE.
    enum class ClosureDiagKind : int { BadWeight,
                                       BudgetExceeded,
                                       UnknownClosure,
                                       BadBlockSize,
                                       BadRefractionIndex,
                                       PointOutOfRange,
                                       NumKinds };

    int closureDiagnosticCode(ClosureDiagKind kind);

    // Where the builder's reports go. The context supplies one per frame (Phase 3); a null hook means once per process, to common/diagnostics.h.
    class IClosureDiagnostics {
        public:
            virtual ~IClosureDiagnostics() = default;
            virtual void report(ClosureDiagKind kind, int code, const char *message) = 0;
    };

    // The once-per-kind-per-frame wrapper: thread-safe, so every thread's builder of one frame can share it. The first report of a kind goes
    // to `sink`; the others are counted and dropped.
    class ClosureFrameDiagnostics final : public IClosureDiagnostics {
        public:
            using Sink = void (*)(void *context, ClosureDiagKind kind, int code, const char *message);

            ClosureFrameDiagnostics(Sink sink, void *context) : sink_(sink), context_(context) {
                for (std::atomic<int> &n : count_)
                    n.store(0);
            }
            void report(ClosureDiagKind kind, int code, const char *message) override {
                if (count_[(int)kind].fetch_add(1) == 0 && sink_ != nullptr)
                    sink_(context_, kind, code, message);
            }
            // how many times a kind was reported, the dropped ones included
            int reported(ClosureDiagKind kind) const { return count_[(int)kind].load(); }

        private:
            Sink sink_;
            void *context_;
            std::atomic<int> count_[(int)ClosureDiagKind::NumKinds];
    };

    // One builder per batch. Every node it returns lives on the arena. A node a check refuses is nullptr, which collapses: mul(w, nullptr) is
    // nullptr and add(a, nullptr) is a.
    class ClosureTreeBuilder {
        public:
            // `arena` is the batch's threadMemory page (the reference follows the page as it grows); `numRealVertices` sizes the budget and the roots
            ClosureTreeBuilder(CMemPage *&arena, int numRealVertices, IClosureDiagnostics *diagnostics);

            // A component of closure `id` with weight `w` and a copy of the parameter block. nullptr (the node is dropped, one report per frame)
            // for an unknown id, a blockSize that is not the entry's, a negative or non-finite channel of `w`, microfacet with eta 0 and refract
            // 1 or 2, or a budget that is exceeded.
            const OrClosureNode *component(int32_t id, const float w[3], const void *block, size_t blockSize);
            const OrClosureNode *mul(const float w[3], const OrClosureNode *child);
            const OrClosureNode *add(const OrClosureNode *a, const OrClosureNode *b);

            // The root of a real vertex. A point outside [0, numRealVertices) is rejected (one report per frame). If the budget was exceeded since
            // the last setRoot, this point's tree is the one that ran over: its root is stored EMPTY (nullptr), and the flag is cleared.
            void setRoot(int point, const OrClosureNode *root);
            const OrClosureNode *root(int point) const;

            size_t bytesUsed() const { return used_; }
            size_t budget() const { return budget_; }

        private:
            void *allocate(size_t size, size_t align); // nullptr over budget; accounts for the alignment padding it takes
            void report(ClosureDiagKind kind, const char *message);

            CMemPage *&arena_;
            int numRealVertices_;
            IClosureDiagnostics *diagnostics_;
            size_t budget_;
            size_t used_;
            bool overflowed_;
            const OrClosureNode **roots_;
    };

    // The flattened form (FR-003): one lobe per component with a non-zero accumulated weight, in depth first order, A before B
    struct ClosureLobe {
            int32_t id;
            openrender::Spectrum weight; // the accumulated weight times the component's own weight
            const void *params;          // the component's parameter block, in the tree
    };

    struct ClosureLobeList {
            ClosureLobe *lobes;
            int count;
            bool hasDelta; // Phase 2 sets it from the lobes' operations; false until then
    };

    // The list is allocated on `arena`; the lobes' `params` point into the tree, which must outlive them
    ClosureLobeList closureFlatten(const OrClosureNode *tree, CMemPage *&arena);

    // A deep copy of a tree onto `arena` (typically a page that outlives the batch's)
    const OrClosureNode *closureTreeCopy(const OrClosureNode *tree, CMemPage *&arena);

    // The accessors (FR-006): no walking by the caller. 0 for an empty tree.
    float closureTreePresence(const OrClosureNode *tree);                // the channel mean of the summed `transparent` weights
    openrender::Spectrum closureTreeEmission(const OrClosureNode *tree); // the sum over `uniform_edf` of weight * emittance
    float closureTreeHoldout(const OrClosureNode *tree);                 // the channel mean of the summed `holdout` weights

    namespace detail {
        // The walk, depth first, A before B, under the accumulated weight rule at the top of this file
        template <class Visit>
        void walkLobes(const OrClosureNode *node, const openrender::Spectrum &acc, Visit &visit) {
            if (node == nullptr)
                return;
            if (node->id == OR_CLOSURE_MUL) {
                const OrClosureMul *m = (const OrClosureMul *)node;
                walkLobes(m->closure, acc * openrender::Spectrum::fromArray3(m->weight), visit);
            }
            else if (node->id == OR_CLOSURE_ADD) {
                const OrClosureAdd *a = (const OrClosureAdd *)node;
                walkLobes(a->closureA, acc, visit);
                walkLobes(a->closureB, acc, visit);
            }
            else {
                const OrClosureComponent *c = (const OrClosureComponent *)node;
                const openrender::Spectrum weight = acc * openrender::Spectrum::fromArray3(c->w);
                if (!weight.isZero())
                    visit(c->id, weight, (const void *)(c + 1));
            }
        }
    } // namespace detail

    // The generic evaluator of Phase 1: visits the lobes of `tree` in the order and with the weights closureFlatten gives, bit for bit.
    // `visit(int32_t id, const Spectrum &weight, const void *params)`. Phase 2 puts the BSDF query on top of this walk.
    template <class Visit>
    void closureEvaluateGeneric(const OrClosureNode *tree, Visit &&visit) {
        detail::walkLobes(tree, openrender::Spectrum::fromRGB(1.0f, 1.0f, 1.0f), visit);
    }

} // namespace openrender::shading

#endif
