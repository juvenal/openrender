/*
 * Spec 035, Closure_Accessors (label closures): the three accessors of contracts/closure-abi.md (FR-006), which never ask the
 * consumer to walk a tree. The accumulated weight is the one of test_flatten.cpp (root to leaf, one multiplication per node).
 * Every number below is exact in binary, so the comparisons are exact.
 *
 *   closureTreePresence(tree)  the channel mean of the summed `transparent` weights; 0 when there is none (D16)
 *   closureTreeEmission(tree)  the sum over `uniform_edf` of weight * emittance
 *   closureTreeHoldout(tree)   the channel mean of the summed `holdout` weights
 *
 * Written red first: closureAbi.h, closureRegistry.h and closureTree.h do not exist.
 */
#include "common/memory.h"
#include "closureAbi.h"
#include "closureBuiltins.h"
#include "closureRegistry.h"
#include "closureTree.h"
#include "closures/test_support.h"

using namespace openrender::shading;
using openrender::Spectrum;


int main() {
    CMemPage *arena;
    memoryInit(arena);
    ClosureTreeBuilder b(arena, 64, nullptr);
    ClosureDiffuseBlock d = {{0, 0, 1}};

    // a tree with none of them
    {
        const float one[3] = {1, 1, 1};
        const OrClosureNode *diffuseOnly = b.component(0, one, &d, sizeof(d));
        CL_CHECK_BITS(closureTreePresence(diffuseOnly), 0.0f);
        CL_CHECK_BITS(closureTreeHoldout(diffuseOnly), 0.0f);
        const Spectrum e = closureTreeEmission(diffuseOnly);
        CL_CHECK(e.isZero());
        // the empty tree
        CL_CHECK_BITS(closureTreePresence(nullptr), 0.0f);
        CL_CHECK_BITS(closureTreeHoldout(nullptr), 0.0f);
        CL_CHECK(closureTreeEmission(nullptr).isZero());
    }

    // presence: the mean of the summed transparent weights, through a multiply and an add
    {
        const float t1[3] = {0.75f, 0.25f, 0.5f};   // mean 0.5
        const float half[3] = {0.5f, 0.5f, 0.5f};
        const float t2[3] = {0.25f, 0.25f, 0.25f};
        const float one[3] = {1, 1, 1};
        const OrClosureNode *tr1 = b.component(5, t1, nullptr, 0);
        CL_CHECK_BITS(closureTreePresence(tr1), 0.5f);
        // under a multiply: (0.75, 0.25, 0.5) * 0.5 = (0.375, 0.125, 0.25), mean 0.25
        const OrClosureNode *scaled = b.mul(half, tr1);
        CL_CHECK_BITS(closureTreePresence(scaled), 0.25f);
        // two transparent lobes in an add are SUMMED: (0.375, 0.125, 0.25) + (0.25, 0.25, 0.25) = (0.625, 0.375, 0.5), mean 0.5
        const OrClosureNode *tr2 = b.component(5, t2, nullptr, 0);
        const OrClosureNode *both = b.add(scaled, tr2);
        CL_CHECK_BITS(closureTreePresence(both), 0.5f);
        // a diffuse lobe beside them changes nothing
        const OrClosureNode *mixed = b.add(b.component(0, one, &d, sizeof(d)), both);
        CL_CHECK_BITS(closureTreePresence(mixed), 0.5f);
        CL_CHECK_BITS(closureTreeHoldout(mixed), 0.0f);
        CL_CHECK(closureTreeEmission(mixed).isZero());
    }

    // emission: weight * emittance of uniform_edf, summed, through a multiply and an add
    {
        const float w[3] = {0.5f, 1, 2};
        ClosureUniformEdfBlock e1 = {{4, 2, 1}}; // weight * emittance = (2, 2, 2)
        const OrClosureNode *edf1 = b.component(7, w, &e1, sizeof(e1));
        Spectrum s = closureTreeEmission(edf1);
        CL_CHECK_BITS(s.c[0], 2.0f);
        CL_CHECK_BITS(s.c[1], 2.0f);
        CL_CHECK_BITS(s.c[2], 2.0f);
        const float half[3] = {0.5f, 0.5f, 0.5f};
        s = closureTreeEmission(b.mul(half, edf1)); // (1, 1, 1)
        CL_CHECK_BITS(s.c[0], 1.0f);
        CL_CHECK_BITS(s.c[2], 1.0f);
        ClosureUniformEdfBlock e2 = {{1, 0, 8}};
        const float one[3] = {1, 1, 1};
        const OrClosureNode *edf2 = b.component(7, one, &e2, sizeof(e2)); // (1, 0, 8)
        s = closureTreeEmission(b.add(b.mul(half, edf1), edf2)); // (1,1,1) + (1,0,8) = (2, 1, 9)
        CL_CHECK_BITS(s.c[0], 2.0f);
        CL_CHECK_BITS(s.c[1], 1.0f);
        CL_CHECK_BITS(s.c[2], 9.0f);
        // the same answer in the old name: `emission` resolves to uniform_edf, so a node built through it is the same node
        CL_CHECK(findClosure("emission") != nullptr);
        CL_CHECK_INT(findClosure("emission")->id, 7);
        // a diffuse lobe contributes nothing to emission
        s = closureTreeEmission(b.add(edf2, b.component(0, one, &d, sizeof(d))));
        CL_CHECK_BITS(s.c[0], 1.0f);
        CL_CHECK_BITS(s.c[2], 8.0f);
    }

    // holdout: the channel mean of the summed holdout weights
    {
        const float h1[3] = {1, 0.5f, 0};   // mean 0.5
        const float h2[3] = {0.25f, 0.25f, 0.25f};
        const OrClosureNode *a = b.component(6, h1, nullptr, 0);
        CL_CHECK_BITS(closureTreeHoldout(a), 0.5f);
        const OrClosureNode *sum = b.add(a, b.component(6, h2, nullptr, 0)); // (1.25, 0.75, 0.25), mean 0.75
        CL_CHECK_BITS(closureTreeHoldout(sum), 0.75f);
        const float half[3] = {0.5f, 0.5f, 0.5f};
        CL_CHECK_BITS(closureTreeHoldout(b.mul(half, sum)), 0.375f);
        // a holdout does not count as presence, and presence does not count as holdout
        CL_CHECK_BITS(closureTreePresence(sum), 0.0f);
        const float t[3] = {1, 1, 1};
        CL_CHECK_BITS(closureTreeHoldout(b.component(5, t, nullptr, 0)), 0.0f);
    }

    // the accessors read the tree as it is: a node with a zero weight adds nothing
    {
        const float zero[3] = {0, 0, 0};
        const float t[3] = {1, 1, 1};
        CL_CHECK_BITS(closureTreePresence(b.mul(zero, b.component(5, t, nullptr, 0))), 0.0f);
    }

    memoryTini(arena);
    return CL_RESULT("Closure_Accessors");
}
