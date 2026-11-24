/*
 * Spec 035, Closure_Registry_* (label closures): the closure registry of contracts/closure-abi.md "Registry" and data-model.md
 * "Parameter blocks". One program, one process: the registry is process-wide and `emission` is deprecated once per process, so the
 * order of the sections below matters (the deprecation is first seen in Registry_Alias).
 *
 *   Closure_Registry_Builtins    the eight built-ins, ids 0-7, categories, parameter lists and offsets
 *   Closure_Registry_Alias       `emission` -> `uniform_edf`, one deprecation message per process
 *   Closure_Registry_Unknown     an unregistered name returns nullptr with a diagnostic naming it
 *   Closure_Registry_Refusals    a duplicate, a BSDF entry without ops, a parameter that overruns the block: -1 and a diagnostic
 *   Closure_Registry_Open        open registration takes the next id; interned strings are stable
 *   Closure_Registry_Race        four threads registering distinct names get distinct ids
 *   Closure_Registry_NoLock      lookup after publication takes no lock (the registry's own lock counter)
 *
 * Written red first: closureAbi.h and closureRegistry.h do not exist.
 */
#include <atomic>
#include <cstddef>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "closureAbi.h"
#include "closureRegistry.h"
#include "closures/test_support.h"

using namespace openrender::shading;

static void dummyEvaluate(const void *, const float[3], const float[3], OrTransportMode, OrBsdfEval *) {}
static void dummySample(const void *, const float[3], float, float, OrTransportMode, OrBsdfSample *) {}
static void dummyPdf(const void *, const float[3], const float[3], OrTransportMode, OrBsdfEval *) {}
static float dummyAlbedo(const void *, const float[3]) { return 1.0f; }
static const OrBsdfComponentOps kDummyOps = {dummyEvaluate, dummySample, dummyPdf, dummyAlbedo};

struct Expected {
        const char *name;
        int32_t id;
        OrClosureCategory category;
        uint32_t blockSize, blockAlign;
        int numParams;
};

// data-model.md "Parameter blocks"
static const Expected kBuiltins[] = {
    {"diffuse", 0, OR_CLOSURE_BSDF, 12, 4, 1},       {"oren_nayar", 1, OR_CLOSURE_BSDF, 16, 4, 2},  {"microfacet", 2, OR_CLOSURE_BSDF, 32, 8, 5},
    {"reflection", 3, OR_CLOSURE_BSDF, 16, 4, 2},    {"refraction", 4, OR_CLOSURE_BSDF, 16, 4, 2},  {"transparent", 5, OR_CLOSURE_SIGNAL, 0, 0, 0},
    {"holdout", 6, OR_CLOSURE_SIGNAL, 0, 0, 0},      {"uniform_edf", 7, OR_CLOSURE_EDF, 12, 4, 1},
};

static const OrClosureParam *param(const OrClosureEntry *e, const char *name) {
    for (int i = 0; i < e->numParams; ++i)
        if (std::strcmp(e->params[i].name, name) == 0)
            return &e->params[i];
    return nullptr;
}

static void checkParam(const OrClosureEntry *e, const char *name, OrClosureParamType type, uint32_t offset) {
    const OrClosureParam *p = param(e, name);
    if (p == nullptr) {
        std::printf("FAIL %s:%d: closure %s has no parameter %s\n", __FILE__, __LINE__, e->name, name);
        ++cltest::failureCount();
        return;
    }
    CL_CHECK_INT(p->type, type);
    CL_CHECK_INT(p->offset, offset);
}

static OrClosureEntry makeEntry(const char *name, OrClosureCategory category, const OrClosureParam *params, int numParams, uint32_t blockSize, uint32_t blockAlign,
                                const OrBsdfComponentOps *ops) {
    OrClosureEntry e = {};
    e.structSize = sizeof(OrClosureEntry);
    e.id = -1;
    e.name = name;
    e.category = category;
    e.params = params;
    e.numParams = numParams;
    e.blockSize = blockSize;
    e.blockAlign = blockAlign;
    e.ops = ops;
    return e;
}

static void sectionBuiltins() {
    cltest::CaptureSink sink;
    for (const Expected &x : kBuiltins) {
        const OrClosureEntry *e = findClosure(x.name);
        if (e == nullptr) {
            std::printf("FAIL %s:%d: the built-in %s does not resolve\n", __FILE__, __LINE__, x.name);
            ++cltest::failureCount();
            continue;
        }
        CL_CHECK(std::strcmp(e->name, x.name) == 0);
        CL_CHECK_INT(e->id, x.id);
        CL_CHECK_INT(e->category, x.category);
        CL_CHECK_INT(e->blockSize, x.blockSize);
        if (x.blockSize != 0)
            CL_CHECK_INT(e->blockAlign, x.blockAlign);
        CL_CHECK_INT(e->numParams, x.numParams);
        CL_CHECK(closureEntry(x.id) == e); // by id and by name, the same entry
    }
    CL_CHECK(closureEntry(-1) == nullptr);
    CL_CHECK(closureEntry(-2) == nullptr);
    CL_CHECK(closureEntry(1 << 20) == nullptr);

    // the parameter lists and offsets (data-model.md "Parameter blocks")
    checkParam(findClosure("diffuse"), "N", OR_CPARAM_NORMAL, 0);
    checkParam(findClosure("oren_nayar"), "N", OR_CPARAM_NORMAL, 0);
    checkParam(findClosure("oren_nayar"), "sigma", OR_CPARAM_FLOAT, 12);
    checkParam(findClosure("microfacet"), "distribution", OR_CPARAM_STRING, 0);
    checkParam(findClosure("microfacet"), "N", OR_CPARAM_NORMAL, 8);
    checkParam(findClosure("microfacet"), "alpha", OR_CPARAM_FLOAT, 20);
    checkParam(findClosure("microfacet"), "eta", OR_CPARAM_FLOAT, 24);
    checkParam(findClosure("microfacet"), "refract", OR_CPARAM_INT, 28);
    checkParam(findClosure("reflection"), "N", OR_CPARAM_NORMAL, 0);
    checkParam(findClosure("reflection"), "eta", OR_CPARAM_FLOAT, 12);
    checkParam(findClosure("refraction"), "N", OR_CPARAM_NORMAL, 0);
    checkParam(findClosure("refraction"), "eta", OR_CPARAM_FLOAT, 12);
    checkParam(findClosure("uniform_edf"), "emittance", OR_CPARAM_COLOR, 0);
    CL_CHECK_INT(sink.total(), 0); // none of this reported anything

}

static void sectionAlias() {
    cltest::CaptureSink sink;
    const OrClosureEntry *edf = findClosure("uniform_edf");
    const OrClosureEntry *viaAlias = findClosure("emission");
    CL_CHECK(viaAlias == edf);
    CL_CHECK(findClosure("emission") == edf);
    CL_CHECK(findClosure("emission") == edf);
    CL_CHECK_INT(sink.countContaining("emission"), 1); // exactly one deprecation message in this process
    CL_CHECK_INT(sink.severityOf("emission"), DIAGNOSTIC_WARNING);
    CL_CHECK(sink.countContaining("uniform_edf") >= 1); // the message names the replacement

}

static void sectionUnknown() {
    cltest::CaptureSink sink;
    sink.clear();
    const char *unknown[] = {"layer", "dielectric_bsdf", "subsurface_bssrdf"};
    for (const char *name : unknown) {
        CL_CHECK(findClosure(name) == nullptr);
        CL_CHECK(sink.countContaining(name) >= 1); // the diagnostic names the closure
    }
    CL_CHECK(findClosure(nullptr) == nullptr);
    CL_CHECK(findClosure("") == nullptr);

}

static void sectionRefusals() {
    cltest::CaptureSink sink;
    sink.clear();
    const OrClosureParam okParam[] = {{"x", OR_CPARAM_FLOAT, 0}};
    // a duplicate name (a built-in's)
    OrClosureEntry dup = makeEntry("diffuse", OR_CLOSURE_BSDF, okParam, 1, 4, 4, &kDummyOps);
    CL_CHECK_INT(registerClosure(dup), -1);
    CL_CHECK(sink.countContaining("diffuse") >= 1);
    // a duplicate of an alias
    OrClosureEntry dupAlias = makeEntry("emission", OR_CLOSURE_EDF, okParam, 1, 4, 4, nullptr);
    CL_CHECK_INT(registerClosure(dupAlias), -1);
    // a BSDF entry without ops
    sink.clear();
    OrClosureEntry noOps = makeEntry("t_bsdf_no_ops", OR_CLOSURE_BSDF, okParam, 1, 4, 4, nullptr);
    CL_CHECK_INT(registerClosure(noOps), -1);
    CL_CHECK(sink.countContaining("t_bsdf_no_ops") >= 1);
    CL_CHECK(findClosure("t_bsdf_no_ops") == nullptr); // and it was not registered
    // a parameter whose offset overruns the block, and one whose offset + size does
    sink.clear();
    const OrClosureParam overrunOffset[] = {{"x", OR_CPARAM_FLOAT, 8}};
    OrClosureEntry bad1 = makeEntry("t_overrun_offset", OR_CLOSURE_EDF, overrunOffset, 1, 8, 4, nullptr);
    CL_CHECK_INT(registerClosure(bad1), -1);
    CL_CHECK(sink.countContaining("t_overrun_offset") >= 1);
    const OrClosureParam overrunSize[] = {{"c", OR_CPARAM_COLOR, 4}}; // 4 + 12 = 16 > 12
    OrClosureEntry bad2 = makeEntry("t_overrun_size", OR_CLOSURE_EDF, overrunSize, 1, 12, 4, nullptr);
    CL_CHECK_INT(registerClosure(bad2), -1);
    CL_CHECK(sink.countContaining("t_overrun_size") >= 1);
    // a null name, a structSize smaller than the entry's
    OrClosureEntry noName = makeEntry(nullptr, OR_CLOSURE_EDF, okParam, 1, 4, 4, nullptr);
    CL_CHECK_INT(registerClosure(noName), -1);
    OrClosureEntry small = makeEntry("t_small_struct", OR_CLOSURE_EDF, okParam, 1, 4, 4, nullptr);
    small.structSize = 4;
    CL_CHECK_INT(registerClosure(small), -1);
    // none of the refusals took an id: the next registration gets 8
    // (the built-ins' own first-use fill is exempt from "BSDF: required"; it is the only registration that is)

}

static void sectionOpen() {
    cltest::CaptureSink sink;
    sink.clear();
    const OrClosureParam fourthParams[] = {{"strength", OR_CPARAM_FLOAT, 0}, {"tint", OR_CPARAM_COLOR, 4}};
    OrClosureEntry fourth = makeEntry("t_fourth_party_edf", OR_CLOSURE_EDF, fourthParams, 2, 16, 4, nullptr);
    const int fourthId = registerClosure(fourth);
    CL_CHECK_INT(fourthId, 8); // the next id after the eight built-ins
    const OrClosureEntry *found = findClosure("t_fourth_party_edf");
    CL_CHECK(found != nullptr);
    if (found != nullptr) {
        CL_CHECK_INT(found->id, 8);
        CL_CHECK_INT(found->blockSize, 16);
        CL_CHECK_INT(found->numParams, 2);
        CL_CHECK(closureEntry(8) == found);
    }
    // a BSDF with ops registers (the entry's own copy of the ops pointer is kept)
    const OrClosureParam okParam[] = {{"x", OR_CPARAM_FLOAT, 0}};
    OrClosureEntry fourthBsdf = makeEntry("t_fourth_party_bsdf", OR_CLOSURE_BSDF, okParam, 1, 4, 4, &kDummyOps);
    CL_CHECK_INT(registerClosure(fourthBsdf), 9);
    const OrClosureEntry *fb = findClosure("t_fourth_party_bsdf");
    CL_CHECK(fb != nullptr && fb->ops == &kDummyOps);
    // the registry copies what it needs: mutating the caller's entry afterwards changes nothing
    fourth.blockSize = 999;
    found = findClosure("t_fourth_party_edf");
    CL_CHECK(found != nullptr && found->blockSize == 16);
    CL_CHECK_INT(sink.total(), 0);

    // internClosureString: one stable pointer per content
    const char *g1 = internClosureString("ggx");
    char copy[8];
    std::strcpy(copy, "ggx");
    const char *g2 = internClosureString(copy);
    CL_CHECK(g1 != nullptr && g1 == g2);
    CL_CHECK(std::strcmp(g1, "ggx") == 0);
    const char *b1 = internClosureString("beckmann");
    CL_CHECK(b1 != nullptr && b1 != g1 && std::strcmp(b1, "beckmann") == 0);
    copy[0] = 'X'; // the interned copy is the registry's own
    CL_CHECK(std::strcmp(g1, "ggx") == 0);

}

static void sectionRace() {
    cltest::CaptureSink sink;
    const int kThreads = 4, kPerThread = 8;
    std::vector<std::vector<int>> ids(kThreads);
    std::atomic<int> ready{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
        threads.emplace_back([&, t]() {
            ready.fetch_add(1);
            while (ready.load() < kThreads) {
            }
            for (int i = 0; i < kPerThread; ++i) {
                char name[64];
                std::snprintf(name, sizeof(name), "t_race_%d_%d", t, i);
                const OrClosureParam p[] = {{"x", OR_CPARAM_FLOAT, 0}};
                OrClosureEntry e = makeEntry(name, OR_CLOSURE_EDF, p, 1, 4, 4, nullptr);
                ids[t].push_back(registerClosure(e)); // the registry copies the name
            }
        });
    for (std::thread &th : threads)
        th.join();
    std::set<int> seen;
    for (const std::vector<int> &v : ids)
        for (int id : v) {
            CL_CHECK(id >= 8); // after the eight built-ins
            CL_CHECK(seen.insert(id).second); // distinct
        }
    CL_CHECK_INT((int)seen.size(), kThreads * kPerThread);
    for (int t = 0; t < kThreads; ++t)
        for (int i = 0; i < kPerThread; ++i) {
            char name[64];
            std::snprintf(name, sizeof(name), "t_race_%d_%d", t, i);
            const OrClosureEntry *e = findClosure(name);
            CL_CHECK(e != nullptr && e->id == ids[t][i]);
        }

}

static void sectionNoLock() {
    cltest::CaptureSink sink;
    // The registry counts the times it takes its registration lock (closureRegistryLockCount). A lookup of a published entry takes
    // none: the first use fills the built-ins (it may lock); a registration publishes a new entry; after that only lookups run
    const OrClosureParam lp[] = {{"x", OR_CPARAM_FLOAT, 0}};
    OrClosureEntry published = makeEntry("t_nolock_probe_a", OR_CLOSURE_EDF, lp, 1, 4, 4, nullptr);
    CL_CHECK_INT(registerClosure(published), 8);
    CL_CHECK(findClosure("microfacet") != nullptr);
    const uint64_t locked = closureRegistryLockCount();
    CL_CHECK(locked > 0); // the registration above took it
    for (int i = 0; i < 1000; ++i) {
        CL_CHECK(findClosure("microfacet") != nullptr);
        CL_CHECK(findClosure("t_nolock_probe_a") != nullptr);
        CL_CHECK(closureEntry(i % 9) != nullptr);
    }
    CL_CHECK_INT(closureRegistryLockCount(), locked); // 3000 lookups, no lock
    // a registration does take it
    OrClosureEntry another = makeEntry("t_nolock_probe_b", OR_CLOSURE_EDF, lp, 1, 4, 4, nullptr);
    CL_CHECK_INT(registerClosure(another), 9);
    CL_CHECK(closureRegistryLockCount() > locked);
}

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "Builtins") sectionBuiltins();
    else if (which == "Alias") sectionAlias();
    else if (which == "Unknown") sectionUnknown();
    else if (which == "Refusals") sectionRefusals();
    else if (which == "Open") sectionOpen();
    else if (which == "Race") sectionRace();
    else if (which == "NoLock") sectionNoLock();
    else {
        std::printf("usage: test_closure_registry <Builtins|Alias|Unknown|Refusals|Open|Race|NoLock>\n");
        return 2;
    }
    return CL_RESULT(("Closure_Registry_" + which).c_str());
}
