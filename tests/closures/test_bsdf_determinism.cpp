/*
 * Spec 035, ClosureDet_BsdfRepeat (label closures): the BSDF table keeps no state and draws nothing (FR-008, convention 9, US2 AS6). One tree of
 * five kinds of lobe (a diffuse, an Oren-Nayar, a dielectric microfacet, a mirror and a refraction) is prepared once and shared by every thread;
 * 512 keyed queries run through evaluate, pdf and sample, and the raw bytes of every answer are hashed (FNV-1a over the bytes, in query order).
 * The hash is the same
 *   - for the first run and for the 99 that follow it, on one thread;
 *   - on four threads, each running all 512 queries on the one shared prepared BSDF, 100 times;
 *   - for a BSDF each thread prepared into its own arena from its own copy of the tree.
 * A tree is not changed by being read: its bytes are hashed before and after.
 *
 * Written red first: bsdfMath.h, bsdf.hpp and orBsdfTable do not exist yet.
 */
#include <atomic>
#include <thread>

#include "bsdf_checks.h"

using namespace clbsdf;
using namespace openrender::shading;

static uint64_t fnv(uint64_t h, const void *data, size_t n) {
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

struct Query {
        V3 wo, wi;
        float u1, u2, u3;
        OrTransportMode mode;
};

static std::vector<Query> makeQueries() {
    cltest::KeyedRandom rnd(0xDE7E2001ull);
    std::vector<Query> q;
    for (int i = 0; i < 512; ++i) {
        Query x;
        x.wo = (i % 8 == 0) ? uniformSphere(rnd.next(), rnd.next()) : uniformUpperHemisphere(rnd.next(), rnd.next());
        x.wi = uniformSphere(rnd.next(), rnd.next());
        x.u1 = rnd.next();
        x.u2 = rnd.next();
        x.u3 = rnd.next();
        x.mode = (i & 1) ? OR_TRANSPORT_RADIANCE : OR_TRANSPORT_IMPORTANCE;
        q.push_back(x);
    }
    return q;
}

// a hash of the answers of one prepared BSDF to every query
static uint64_t hashAnswers(const OrBsdf *bsdf, const std::vector<Query> &queries) {
    const Subject s = tableSubject("mixed", bsdf);
    uint64_t h = 0xcbf29ce484222325ull;
    for (const Query &q : queries) {
        const OrBsdfEval e = s.evaluate(q.wo, q.wi, q.mode), p = s.pdf(q.wo, q.wi, q.mode);
        const OrBsdfSample r = s.sample(q.wo, q.u1, q.u2, q.u3, q.mode);
        // the structs may hold padding the callee does not write: every answer was zeroed by the subject, so the raw bytes are defined
        h = fnv(h, &e, sizeof(e));
        h = fnv(h, &p, sizeof(p));
        h = fnv(h, &r, sizeof(r));
    }
    return h;
}

// builds the tree on `arena` and prepares it there
static const OrClosureNode *buildTree(CMemPage *&arena, ClosureTreeBuilder &builder) {
    const char *ggx = internClosureString("ggx");
    ClosureDiffuseBlock d = {{0, 0, 1}};
    ClosureOrenNayarBlock on = {{0, 0, 1}, 0.5f};
    ClosureMicrofacetBlock m = {ggx, {0, 0, 1}, 0.3f, 1.5f, 2};
    ClosureReflectionBlock r = {{0, 0, 1}, 1.5f};
    ClosureRefractionBlock t = {{0, 0, 1}, 1.5f};
    const float wd[3] = {0.4f, 0.3f, 0.2f}, wo[3] = {0.1f, 0.1f, 0.1f}, wm[3] = {0.3f, 0.3f, 0.3f}, wr[3] = {0.1f, 0.2f, 0.1f}, wt[3] = {0.1f, 0.1f, 0.2f};
    (void)arena;
    const OrClosureNode *a = builder.add(builder.component(kClosureDiffuse, wd, &d, sizeof(d)), builder.component(kClosureOrenNayar, wo, &on, sizeof(on)));
    const OrClosureNode *b = builder.add(builder.component(kClosureMicrofacet, wm, &m, sizeof(m)), builder.component(kClosureReflection, wr, &r, sizeof(r)));
    return builder.add(builder.add(a, b), builder.component(kClosureRefraction, wt, &t, sizeof(t)));
}

int main() {
    const std::vector<Query> queries = makeQueries();
    CMemPage *arena;
    memoryInit(arena);
    ClosureTreeBuilder builder(arena, 16, nullptr);
    const OrClosureNode *tree = buildTree(arena, builder);
    CL_CHECK(tree != nullptr);
    const ClosureLobeList list = closureFlatten(tree, arena);
    CL_CHECK_INT(list.count, 5);
    uint64_t treeBefore = 0xcbf29ce484222325ull;
    for (int i = 0; i < list.count; ++i)
        treeBefore = fnv(treeBefore, &list.lobes[i].weight, sizeof(list.lobes[i].weight));

    OrBsdf *shared = nullptr;
    CL_CHECK_INT(prepareTree(tree, arena, &shared), OR_STATUS_OK);
    const uint64_t reference = hashAnswers(shared, queries);

    // one thread, 100 repetitions
    for (int i = 0; i < 100; ++i)
        if (hashAnswers(shared, queries) != reference) {
            cltest::fail(__FILE__, __LINE__, "one thread: the answers of a repetition differ from the first run");
            break;
        }

    // four threads on the one shared prepared BSDF, 100 repetitions each; and a BSDF each thread prepares for itself
    std::atomic<int> ready{0}, mismatchShared{0}, mismatchOwn{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&]() {
            CMemPage *own;
            memoryInit(own);
            ClosureTreeBuilder b(own, 16, nullptr);
            const OrClosureNode *myTree = buildTree(own, b);
            OrBsdf *mine = nullptr;
            const OrStatus st = prepareTree(myTree, own, &mine);
            ready.fetch_add(1);
            while (ready.load() < 4) {
            }
            for (int i = 0; i < 100; ++i) {
                if (hashAnswers(shared, queries) != reference)
                    mismatchShared.fetch_add(1);
                if (st != OR_STATUS_OK || hashAnswers(mine, queries) != reference)
                    mismatchOwn.fetch_add(1);
            }
            memoryTini(own);
        });
    for (std::thread &th : threads)
        th.join();
    CL_CHECK_INT(mismatchShared.load(), 0);
    CL_CHECK_INT(mismatchOwn.load(), 0);

    // reading did not change the tree
    const ClosureLobeList after = closureFlatten(tree, arena);
    uint64_t treeAfter = 0xcbf29ce484222325ull;
    for (int i = 0; i < after.count; ++i)
        treeAfter = fnv(treeAfter, &after.lobes[i].weight, sizeof(after.lobes[i].weight));
    CL_CHECK(treeBefore == treeAfter);
    std::printf("  ClosureDet_BsdfRepeat: 512 queries, hash %016llx on 1 and on 4 threads, 100 repetitions\n", (unsigned long long)reference);
    memoryTini(arena);
    return CL_RESULT("ClosureDet_BsdfRepeat");
}
