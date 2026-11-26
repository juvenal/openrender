/*
 * Spec 035, Closure_Flatten_* (label closures): the builder, closureFlatten and closureEvaluateGeneric of contracts/closure-abi.md
 * "Building, flattening, lifetime".
 *
 * The rule the answers are compared under (pinned here, in closure-abi.md's words "walks depth first, A before B, with an
 * accumulated weight"): the accumulated weight of a node is its parent's accumulated weight times the node's own weight, one
 * float multiplication per node, root to leaf, the root's accumulated weight being exactly 1. A lobe is the accumulated weight of a
 * component times nothing else: lobe.weight = acc * component.w. A lobe whose weight is zero in every channel is dropped.
 * The reference below (generated together with the tree) does exactly that, so "bit for bit" is a comparison of float bits.
 *
 *   Closure_Flatten_Random    1000 seeded random trees: the flattened list, the generic evaluator and the reference agree bit for bit
 *   Closure_Flatten_Nested    hand-built nesting: a multiply of a multiply, a zero weight, a nested add, the order A before B
 *   Closure_Flatten_Collapse  a nullptr child collapses; flatten(nullptr) is empty
 *   Closure_Flatten_Refusals  bad weights, an unknown id, a wrong block size, microfacet eta 0 with refract 1 or 2
 *
 * Written red first: closureAbi.h, closureRegistry.h and closureTree.h do not exist.
 */
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "common/memory.h"
#include "closureAbi.h"
#include "closureBuiltins.h"
#include "closureRegistry.h"
#include "closureTree.h"
#include "closures/test_support.h"

using namespace openrender::shading;
using openrender::Spectrum;

// what a frame's diagnostics hook saw: the count of reports per kind
struct Seen {
        int count[(int)ClosureDiagKind::NumKinds] = {};
        int lastCode[(int)ClosureDiagKind::NumKinds] = {};
};
static void sinkFn(void *context, ClosureDiagKind kind, int code, const char *) {
    Seen *s = (Seen *)context;
    ++s->count[(int)kind];
    s->lastCode[(int)kind] = code;
}

static Spectrum S(float a, float b, float c) { return Spectrum::fromRGB(a, b, c); }

// ---- the reference: lobes in order, generated together with the tree
struct RefLobe {
        int32_t id;
        Spectrum weight;
        std::vector<unsigned char> block;
};

struct Gen {
        ClosureTreeBuilder &builder;
        cltest::KeyedRandom &rnd;
        std::vector<RefLobe> &out;

        static const int kIds[5];

        const OrClosureNode *leaf(const Spectrum &acc) {
            const int id = kIds[rnd.nextInt(5)];
            const OrClosureEntry *e = closureEntry(id);
            float w[3];
            const bool zero = rnd.next() < 0.10f; // some zero weights: the lobe is dropped
            for (float &c : w)
                c = zero ? 0.0f : rnd.range(0.0f, 2.0f);
            std::vector<unsigned char> block(e->blockSize);
            for (unsigned char &b : block)
                b = (unsigned char)rnd.nextInt(256);
            // a block of floats must not hold NaN payloads the comparison would trip on: they are copied as bytes only, never read
            const OrClosureNode *node = builder.component(id, w, block.data(), block.size());
            Spectrum lobeWeight = acc * Spectrum::fromArray3(w);
            if (!lobeWeight.isZero())
                out.push_back({id, lobeWeight, block});
            return node;
        }

        const OrClosureNode *tree(int depth, const Spectrum &acc) {
            // a richer population (maintainer, 2026-11-24, Option 2 of tools/proposed/test_flatten_random_floors.md): below depth 0 a node is a
            // leaf with probability 1/6, a multiply with 1/3 and an add with 1/2, so a tree has several lobes and few are empty
            int kind = 0;
            if (depth > 0) {
                const float pick = rnd.next();
                kind = pick < 1.0f / 6.0f ? 0 : (pick < 0.5f ? 1 : 2);
            }
            if (kind == 0)
                return leaf(acc);
            if (kind == 1) { // multiply
                float w[3];
                const bool zero = rnd.next() < 0.06f; // a zero multiply drops its whole subtree
                for (float &c : w)
                    c = zero ? 0.0f : rnd.range(0.0f, 1.5f);
                const Spectrum next = acc * Spectrum::fromArray3(w);
                return builder.mul(w, tree(depth - 1, next));
            }
            const OrClosureNode *a = tree(depth - 1, acc); // A first: its lobes are pushed first
            const OrClosureNode *b = tree(depth - 1, acc);
            return builder.add(a, b);
        }
};
const int Gen::kIds[5] = {0, 1, 3, 5, 7};

static bool sameLobes(const std::vector<RefLobe> &ref, const ClosureLobeList &list, const char *what) {
    bool ok = true;
    if ((int)ref.size() != list.count) {
        std::printf("FAIL %s:%d: %s: %d lobes, expected %d\n", __FILE__, __LINE__, what, list.count, (int)ref.size());
        ++cltest::failureCount();
        return false;
    }
    for (int i = 0; i < list.count; ++i) {
        const RefLobe &r = ref[i];
        const ClosureLobe &l = list.lobes[i];
        ok = ok && CL_CHECK_INT(l.id, r.id);
        for (int c = 0; c < 3; ++c)
            ok = ok && CL_CHECK_BITS(l.weight.c[c], r.weight.c[c]);
        if (!r.block.empty())
            ok = ok && (std::memcmp(l.params, r.block.data(), r.block.size()) == 0 || (cltest::fail(__FILE__, __LINE__, "parameter block differs"), false));
        if (!ok)
            return false;
    }
    return ok;
}

static void sectionRandom() {
    cltest::KeyedRandom rnd(0xC10507EEull, 1);
    int totalLobes = 0, nonEmpty = 0;
    CMemPage *arena;
    memoryInit(arena);
    for (int t = 0; t < 1000; ++t) {
        {
            memBegin(arena); // every tree lives on the page until the end of its iteration, as a batch's tree does
            Seen seen;
            ClosureFrameDiagnostics diag(sinkFn, &seen);
            ClosureTreeBuilder builder(arena, 4096, &diag); // a budget of 4 MiB: a depth-6 tree always fits
            std::vector<RefLobe> ref;
            Gen gen{builder, rnd, ref};
            const OrClosureNode *tree = gen.tree(3 + rnd.nextInt(4), Spectrum::fromRGB(1, 1, 1)); // depth 3..6
            CL_CHECK_INT(seen.count[(int)ClosureDiagKind::BudgetExceeded], 0);

            // the flattened list against the reference
            const ClosureLobeList list = closureFlatten(tree, arena);
            sameLobes(ref, list, "flatten");

            // the generic evaluator visits the same lobes in the same order with the same bits
            std::vector<RefLobe> visited;
            closureEvaluateGeneric(tree, [&](int32_t id, const Spectrum &w, const void *params) {
                RefLobe l;
                l.id = id;
                l.weight = w;
                l.block.assign((const unsigned char *)params, (const unsigned char *)params + closureEntry(id)->blockSize);
                visited.push_back(l);
            });
            CL_CHECK_INT((int)visited.size(), (int)ref.size());
            for (size_t i = 0; i < visited.size() && i < ref.size(); ++i) {
                CL_CHECK_INT(visited[i].id, ref[i].id);
                for (int c = 0; c < 3; ++c)
                    CL_CHECK_BITS(visited[i].weight.c[c], ref[i].weight.c[c]);
                CL_CHECK(visited[i].block == ref[i].block);
            }

            // and a weighted sum over the lobes, in lobe order, is the same number by either path (the evaluator's use)
            Spectrum viaList, viaGeneric;
            for (int i = 0; i < list.count; ++i)
                viaList = viaList + list.lobes[i].weight * S((float)list.lobes[i].id + 1.0f, 0.5f, 2.0f);
            closureEvaluateGeneric(tree, [&](int32_t id, const Spectrum &w, const void *) { viaGeneric = viaGeneric + w * S((float)id + 1.0f, 0.5f, 2.0f); });
            for (int c = 0; c < 3; ++c)
                CL_CHECK_BITS(viaList.c[c], viaGeneric.c[c]);

            totalLobes += list.count;
            nonEmpty += list.count > 0 ? 1 : 0;
            memEnd(arena);
        }
    }
    memoryTini(arena);
    CL_CHECK(totalLobes > 3000); // the random trees are not trivially empty
    CL_CHECK(nonEmpty > 900);
}

static void sectionNested() {
    CMemPage *arena;
    memoryInit(arena);
    Seen seen;
    ClosureFrameDiagnostics diag(sinkFn, &seen);
    ClosureTreeBuilder builder(arena, 64, &diag);
    ClosureDiffuseBlock d = {{0, 0, 1}};
    ClosureReflectionBlock r = {{0, 0, 1}, 1.5f};
    ClosureUniformEdfBlock e = {{4, 2, 1}};

    // a multiply of a multiply: ((1 * w1) * w2) * cw, root to leaf
    {
        const float cw[3] = {1, 2, 4}, w1[3] = {0.5f, 0.25f, 1}, w2[3] = {0.5f, 0.5f, 0.5f};
        const OrClosureNode *comp = builder.component(0, cw, &d, sizeof(d));
        const OrClosureNode *tree = builder.mul(w1, builder.mul(w2, comp));
        const ClosureLobeList list = closureFlatten(tree, arena);
        CL_CHECK_INT(list.count, 1);
        if (list.count == 1) {
            CL_CHECK_BITS(list.lobes[0].weight.c[0], 0.25f);
            CL_CHECK_BITS(list.lobes[0].weight.c[1], 0.25f);
            CL_CHECK_BITS(list.lobes[0].weight.c[2], 2.0f);
            CL_CHECK(std::memcmp(list.lobes[0].params, &d, sizeof(d)) == 0);
        }
    }
    // a zero weight: the multiply is a valid node and flattens to nothing; a zero-weight component is dropped from an add
    {
        const float zero[3] = {0, 0, 0}, one[3] = {1, 1, 1};
        const OrClosureNode *comp = builder.component(0, one, &d, sizeof(d));
        const OrClosureNode *zeroMul = builder.mul(zero, comp);
        CL_CHECK(zeroMul != nullptr);
        CL_CHECK_INT(closureFlatten(zeroMul, arena).count, 0);
        const OrClosureNode *zeroComp = builder.component(3, zero, &r, sizeof(r));
        CL_CHECK(zeroComp != nullptr);
        const OrClosureNode *sum = builder.add(zeroComp, builder.component(0, one, &d, sizeof(d)));
        const ClosureLobeList list = closureFlatten(sum, arena);
        CL_CHECK_INT(list.count, 1);
        if (list.count == 1)
            CL_CHECK_INT(list.lobes[0].id, 0);
        // a weight that is zero in only some channels is kept
        const float some[3] = {0, 0, 0.5f};
        const ClosureLobeList kept = closureFlatten(builder.component(7, some, &e, sizeof(e)), arena);
        CL_CHECK_INT(kept.count, 1);
    }
    // a nested add: add(add(a, b), add(c, d)) visits a, b, c, d (depth first, A before B)
    {
        const float one[3] = {1, 1, 1};
        const OrClosureNode *a = builder.component(0, one, &d, sizeof(d));
        const OrClosureNode *b = builder.component(3, one, &r, sizeof(r));
        const OrClosureNode *c = builder.component(7, one, &e, sizeof(e));
        const OrClosureNode *dd = builder.component(5, one, nullptr, 0);
        const OrClosureNode *tree = builder.add(builder.add(a, b), builder.add(c, dd));
        const ClosureLobeList list = closureFlatten(tree, arena);
        CL_CHECK_INT(list.count, 4);
        const int order[4] = {0, 3, 7, 5};
        for (int i = 0; i < list.count && i < 4; ++i)
            CL_CHECK_INT(list.lobes[i].id, order[i]);
        // a component is 16-aligned (the parameter block follows it at +16), a multiply and an add are 8-aligned
        CL_CHECK_INT(((uintptr_t)a) % 16, 0);
        CL_CHECK_INT(((uintptr_t)c) % 16, 0);
        CL_CHECK_INT(a->id, 0);
        CL_CHECK_INT(builder.add(a, b)->id, OR_CLOSURE_ADD);
        CL_CHECK_INT(builder.mul(one, a)->id, OR_CLOSURE_MUL);
        CL_CHECK_INT((((uintptr_t)builder.mul(one, a)) % 8), 0);
        // the block is at (const char *)(component + 1)
        const OrClosureComponent *ca = (const OrClosureComponent *)a;
        CL_CHECK(std::memcmp((const char *)(ca + 1), &d, sizeof(d)) == 0);
        CL_CHECK_BITS(ca->w[0], 1.0f);
    }
    CL_CHECK_INT(seen.count[(int)ClosureDiagKind::BudgetExceeded], 0);
    memoryTini(arena);
}

static void sectionCollapse() {
    CMemPage *arena;
    memoryInit(arena);
    ClosureTreeBuilder builder(arena, 64, nullptr);
    ClosureDiffuseBlock d = {{0, 0, 1}};
    const float one[3] = {1, 1, 1};
    const OrClosureNode *a = builder.component(0, one, &d, sizeof(d));
    CL_CHECK(a != nullptr);
    CL_CHECK(builder.mul(one, nullptr) == nullptr);
    CL_CHECK(builder.add(a, nullptr) == a);
    CL_CHECK(builder.add(nullptr, a) == a);
    CL_CHECK(builder.add(nullptr, nullptr) == nullptr);
    CL_CHECK_INT(closureFlatten(nullptr, arena).count, 0);
    int visits = 0;
    closureEvaluateGeneric(nullptr, [&](int32_t, const Spectrum &, const void *) { ++visits; });
    CL_CHECK_INT(visits, 0);
    // an empty root is valid
    builder.setRoot(0, nullptr);
    CL_CHECK(builder.root(0) == nullptr);
    memoryTini(arena);
}

static void sectionRefusals() {
    CMemPage *arena;
    memoryInit(arena);
    Seen seen;
    ClosureFrameDiagnostics diag(sinkFn, &seen);
    ClosureTreeBuilder builder(arena, 64, &diag);
    ClosureDiffuseBlock d = {{0, 0, 1}};
    const float one[3] = {1, 1, 1};
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();

    // a negative or non-finite channel drops the node, with ONE diagnostic per frame however many nodes are refused; no abort
    const float negative[3] = {1, -0.001f, 1}, notANumber[3] = {1, nan, 1}, infinite[3] = {inf, 1, 1}, negInf[3] = {1, 1, -inf};
    CL_CHECK(builder.component(0, negative, &d, sizeof(d)) == nullptr);
    CL_CHECK(builder.component(0, notANumber, &d, sizeof(d)) == nullptr);
    CL_CHECK(builder.component(0, infinite, &d, sizeof(d)) == nullptr);
    const OrClosureNode *good = builder.component(0, one, &d, sizeof(d));
    CL_CHECK(good != nullptr);
    CL_CHECK(builder.mul(negInf, good) == nullptr);
    CL_CHECK(builder.mul(negative, good) == nullptr);
    CL_CHECK_INT(seen.count[(int)ClosureDiagKind::BadWeight], 1);
    CL_CHECK_INT(seen.lastCode[(int)ClosureDiagKind::BadWeight], (int)CODE_RANGE);
    // the refusals did not damage the builder: a good node still builds and flattens
    CL_CHECK_INT(closureFlatten(builder.add(good, builder.component(0, one, &d, sizeof(d))), arena).count, 2);

    // an unknown id, a wrong block size
    CL_CHECK(builder.component(99, one, &d, sizeof(d)) == nullptr);
    CL_CHECK(builder.component(-3, one, &d, sizeof(d)) == nullptr);
    CL_CHECK(builder.component(0, one, &d, sizeof(d) + 4) == nullptr);
    CL_CHECK(builder.component(0, one, &d, 4) == nullptr);
    CL_CHECK(builder.component(5, one, &d, 4) == nullptr); // transparent has no block
    CL_CHECK_INT(seen.count[(int)ClosureDiagKind::UnknownClosure], 1);
    CL_CHECK_INT(seen.count[(int)ClosureDiagKind::BadBlockSize], 1);
    // a component() and a block pointer: blockSize 0 with a null block is the transparent component
    CL_CHECK(builder.component(5, one, nullptr, 0) != nullptr);

    // microfacet: eta = 0 with refract 0 has no Fresnel term and builds; eta = 0 with refract 1 or 2 is refused at build; eta > 0 builds
    const char *ggx = internClosureString("ggx");
    ClosureMicrofacetBlock m = {ggx, {0, 0, 1}, 0.3f, 0.0f, 0};
    CL_CHECK(builder.component(2, one, &m, sizeof(m)) != nullptr);
    m.refract = 1;
    CL_CHECK(builder.component(2, one, &m, sizeof(m)) == nullptr);
    m.refract = 2;
    CL_CHECK(builder.component(2, one, &m, sizeof(m)) == nullptr);
    CL_CHECK_INT(seen.count[(int)ClosureDiagKind::BadRefractionIndex], 1);
    m.eta = 1.5f;
    CL_CHECK(builder.component(2, one, &m, sizeof(m)) != nullptr);
    m.refract = 1;
    CL_CHECK(builder.component(2, one, &m, sizeof(m)) != nullptr);
    memoryTini(arena);
}

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "Random") sectionRandom();
    else if (which == "Nested") sectionNested();
    else if (which == "Collapse") sectionCollapse();
    else if (which == "Refusals") sectionRefusals();
    else {
        std::printf("usage: test_closure_flatten <Random|Nested|Collapse|Refusals>\n");
        return 2;
    }
    return CL_RESULT(("Closure_Flatten_" + which).c_str());
}
