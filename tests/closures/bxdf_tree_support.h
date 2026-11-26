/*
 * Spec 035, Phase 4: what the Bxdf tests share: an arena, the shading point of the built-in tests, the flattened lobes of a built tree
 * and the hand-built lobes the trees are compared with. Header-only, no renderer needed.
 */
#ifndef TESTS_CLOSURES_BXDF_TREE_SUPPORT_H
#define TESTS_CLOSURES_BXDF_TREE_SUPPORT_H

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "bxdfAbi.h"
#include "closureBuiltins.h"
#include "closureTree.h"
#include "common/memory.h"
#include "test_support.h"

namespace bxtree {

    using namespace openrender::shading;

    struct Arena {
            CMemPage *page = nullptr;
            Arena() { memoryInit(page); }
            ~Arena() { memoryTini(page); }
    };
    inline void *allocate(void *context, size_t size, size_t align) {
        CMemPage *&page = *(CMemPage **)context;
        char *raw = (char *)ralloc((int)(size + align), page);
        return (void *)(((uintptr_t)raw + (align - 1)) & ~(uintptr_t)(align - 1));
    }

    using F3 = std::array<float, 3>;

    // The point of the built-in tests: Cs = (0.8, 0.2, 0.1), the given Os, N = the faceforwarded shading normal
    inline OrBxdfPoint pointWith(F3 Os) {
        OrBxdfPoint p;
        std::memset(&p, 0, sizeof(p));
        p.structSize = sizeof(p);
        p.N[2] = 1.0f;
        p.Ng[2] = 1.0f;
        p.dPdu[0] = 1.0f;
        p.I[2] = -1.0f;
        p.u = 0.25f;
        p.v = 0.75f;
        p.Cs[0] = 0.8f;
        p.Cs[1] = 0.2f;
        p.Cs[2] = 0.1f;
        for (int k = 0; k < 3; ++k)
            p.Os[k] = Os[(size_t)k];
        return p;
    }
    const F3 kCs = {0.8f, 0.2f, 0.1f};

    struct Lobe {
            int32_t id;
            F3 weight;
            std::vector<float> block;           // the component's block as floats (the distribution pointer excluded)
            const void *distribution = nullptr; // microfacet only
    };

    // Flatten a tree already built into `arena` into plain lobes
    inline std::vector<Lobe> lobesOfTree(const OrClosureNode *tree, Arena &arena) {
        std::vector<Lobe> out;
        const ClosureLobeList list = closureFlatten(tree, arena.page);
        for (int i = 0; i < list.count; ++i) {
            Lobe l;
            l.id = list.lobes[i].id;
            list.lobes[i].weight.toArray3(l.weight.data());
            if (l.id == kClosureDiffuse) {
                const auto *b = static_cast<const ClosureDiffuseBlock *>(list.lobes[i].params);
                l.block = {b->N[0], b->N[1], b->N[2]};
            }
            else if (l.id == kClosureOrenNayar) {
                const auto *b = static_cast<const ClosureOrenNayarBlock *>(list.lobes[i].params);
                l.block = {b->N[0], b->N[1], b->N[2], b->sigma};
            }
            else if (l.id == kClosureMicrofacet) {
                const auto *b = static_cast<const ClosureMicrofacetBlock *>(list.lobes[i].params);
                l.block = {b->N[0], b->N[1], b->N[2], b->alpha, b->eta, (float)b->refract};
                l.distribution = b->distribution;
            }
            out.push_back(l);
        }
        return out;
    }

    // Build with a table's own data at `point` (no renderer: the build is declarative) and flatten
    inline std::vector<Lobe> lobesOfData(const OrBxdfTable *t, const OrBxdfData *data, const OrBxdfPoint &point, bool &ok) {
        Arena arena;
        OrClosureAllocator alloc = {&arena.page, allocate};
        const OrClosureNode *tree = t->build(data, &point, &alloc);
        ok = tree != nullptr;
        return ok ? lobesOfTree(tree, arena) : std::vector<Lobe>();
    }

    inline bool sameLobe(const Lobe &a, const Lobe &b) {
        if (a.id != b.id || a.block.size() != b.block.size() || a.distribution != b.distribution)
            return false;
        for (int k = 0; k < 3; ++k)
            if (!cltest::bitsEqual(a.weight[(size_t)k], b.weight[(size_t)k]))
                return false;
        for (size_t i = 0; i < a.block.size(); ++i)
            if (!cltest::bitsEqual(a.block[i], b.block[i]))
                return false;
        return true;
    }
    inline bool sameLobes(const std::vector<Lobe> &a, const std::vector<Lobe> &b) {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (!sameLobe(a[i], b[i]))
                return false;
        return true;
    }
    // Approximate comparison for weights that went through a colour conversion
    inline bool nearLobes(const std::vector<Lobe> &a, const std::vector<Lobe> &b, float tol) {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i].id != b[i].id || a[i].block.size() != b[i].block.size())
                return false;
            for (int k = 0; k < 3; ++k)
                if (std::fabs(a[i].weight[(size_t)k] - b[i].weight[(size_t)k]) > tol)
                    return false;
            for (size_t j = 0; j < a[i].block.size(); ++j)
                if (std::fabs(a[i].block[j] - b[i].block[j]) > tol)
                    return false;
        }
        return true;
    }

    inline Lobe diffuse(F3 w) { return {kClosureDiffuse, w, {0, 0, 1}, nullptr}; }
    inline Lobe orenNayar(F3 w, float sigma) { return {kClosureOrenNayar, w, {0, 0, 1, sigma}, nullptr}; }
    inline Lobe ggx(F3 w, float alpha, float eta, int refract) { return {kClosureMicrofacet, w, {0, 0, 1, alpha, eta, (float)refract}, internClosureString("ggx")}; }
    inline F3 times(float k, const F3 &c) { return {k * c[0], k * c[1], k * c[2]}; }

} // namespace bxtree

#endif
