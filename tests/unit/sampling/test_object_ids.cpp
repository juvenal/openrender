// Sampling_ObjectIds (spec 039, Phase 4, T038; research R3.2, FR-016): every
// object has a stable 64-bit identity, assigned by rule from the scene and
// never from render-time order.
//
//   scene_loads   two loads of one scene, at one and at four threads, give the
//                 same identities; they are all distinct
//   instances     two ObjectInstances of one master are two objects, and their
//                 copies (made at render time) are mix(instance, k): four different
//                 identities, equal across loads
//   delayed       a DelayedReadArchive's children are mix(parent, k)
//   tessellation  a mesh's children are mix(parent, childIndex)
//   splits        the pieces dicing splits a patch into keep the patch's identity
//
// The rule tested for a child is the pinned formula
// openrender::sampling::detail::mix(parent.stableId, k). The identity of a
// top-level object (mix(scope, ordinal)) is not pinned by value: the tests
// require it to be independent of thread count, reproducible and distinct.
//
// One case per process (a render in a second Ri session has crashed before,
// GitHub #2), except scene_loads, which needs two loads by definition.
#include <algorithm>
#include <cstring>
#include <set>
#include <unistd.h>

#include "scene_probe.h"

using namespace probe;

static int failures = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

static uint64_t mixChild(uint64_t parent, uint64_t k) {
    return sp::detail::mix(parent, k);
}

// A small scene: a sphere, a bicubic patch and a polygon mesh.
static void emitScene() {
    RiSphere(0.5f, -0.5f, 0.5f, 360.0f, RI_NULL);

    float P[16 * 3];
    gridPoints(P, 4, 4, 0.4f);
    RiAttributeBegin();
    RiTranslate(-1.0f, 0, 0);
    RiPatch(RI_BICUBIC, RI_P, P, RI_NULL);
    RiAttributeEnd();

    int nverts[2] = {4, 4};
    int verts[8] = {0, 1, 4, 3, 1, 2, 5, 4};
    float M[6 * 3];
    gridPoints(M, 3, 2, 0.4f);
    RiAttributeBegin();
    RiTranslate(1.0f, 0, 0);
    RiPointsPolygons(2, nverts, verts, RI_P, M, RI_NULL);
    RiAttributeEnd();
}

static std::vector<uint64_t> idsOf(const std::vector<CObject *> &objects) {
    std::vector<uint64_t> ids;
    for (const CObject *o : objects)
        ids.push_back(o->stableId);
    return ids;
}

static std::vector<uint64_t> loadScene(int threads) {
    Session s(threads, "sobol");
    emitScene();
    s.render();
    std::vector<uint64_t> ids = idsOf(s.capture()->objects);
    s.close();
    return ids;
}

static void sceneLoads() {
    const std::vector<uint64_t> a = loadScene(1);
    const std::vector<uint64_t> b = loadScene(4);
    CHECK(a.size() >= 3);

    // Reproducible, and independent of the thread count. Compared as sets: the
    // order objects are added in may differ with threads.
    std::vector<uint64_t> sa = a, sb = b;
    std::sort(sa.begin(), sa.end());
    std::sort(sb.begin(), sb.end());
    CHECK(sa == sb);

    // Distinct and set.
    CHECK(std::set<uint64_t>(a.begin(), a.end()).size() == a.size());
    for (uint64_t id : a)
        CHECK(id != 0);
}

// An ObjectInstance adds one delayed-instance object to the scene; its copies of
// the master's objects are made at render time, as its children.
struct InstanceIds {
        std::vector<uint64_t> instances;     // the two instance objects
        std::vector<uint64_t> copiesOfFirst; // children of the first instance
        std::vector<uint64_t> copiesOfSecond;
        bool operator==(const InstanceIds &o) const {
            return instances == o.instances && copiesOfFirst == o.copiesOfFirst && copiesOfSecond == o.copiesOfSecond;
        }
};

static std::vector<uint64_t> childIds(const CObject *parent) {
    std::vector<uint64_t> ids;
    for (const CObject *c = parent->children; c != nullptr; c = c->sibling)
        ids.push_back(c->stableId);
    std::sort(ids.begin(), ids.end());
    return ids;
}

static InstanceIds loadInstances() {
    Session s(1, "sobol");
    RtObjectHandle handle = RiObjectBegin();
    RiSphere(0.3f, -0.3f, 0.3f, 360.0f, RI_NULL);
    float P[4 * 3];
    gridPoints(P, 2, 2, 0.3f);
    RiPatch(RI_BILINEAR, RI_P, P, RI_NULL);
    RiObjectEnd();
    std::vector<CObject *> first = s.emit([&] {
        RiAttributeBegin();
        RiTranslate(-0.8f, 0, 0);
        RiObjectInstance(handle);
        RiAttributeEnd();
    });
    std::vector<CObject *> second = s.emit([&] {
        RiAttributeBegin();
        RiTranslate(0.8f, 0, 0);
        RiObjectInstance(handle);
        RiAttributeEnd();
    });
    CHECK(first.size() == 1 && second.size() == 1);
    s.render(); // makes the copies

    InstanceIds ids;
    if (first.size() == 1 && second.size() == 1) {
        ids.instances = {first[0]->stableId, second[0]->stableId};
        ids.copiesOfFirst = childIds(first[0]);
        ids.copiesOfSecond = childIds(second[0]);

        // Each copy is mix(instance, k) for k = 0, 1 (the master has two objects).
        for (const CObject *instance : {first[0], second[0]}) {
            std::vector<uint64_t> expected = {mixChild(instance->stableId, 0), mixChild(instance->stableId, 1)};
            std::sort(expected.begin(), expected.end());
            CHECK(childIds(instance) == expected);
        }
    }
    s.close();
    return ids;
}

static void instances() {
    const InstanceIds one = loadInstances();
    CHECK(one.instances.size() == 2 && one.instances[0] != one.instances[1]);
    CHECK(one.copiesOfFirst.size() == 2 && one.copiesOfSecond.size() == 2);

    // The four copies are four different objects: two instances of one master
    // do not share identities.
    std::set<uint64_t> all(one.copiesOfFirst.begin(), one.copiesOfFirst.end());
    all.insert(one.copiesOfSecond.begin(), one.copiesOfSecond.end());
    CHECK(all.size() == 4);

    // And a second load gives the same identities.
    CHECK(loadInstances() == one);
}

static void delayed() {
    char tmpl[] = "/tmp/object_ids_XXXXXX";
    const std::string dir = mkdtemp(tmpl);
    const std::string archive = dir + "/three.rib";
    {
        FILE *f = fopen(archive.c_str(), "w");
        fputs("Sphere 0.3 -0.3 0.3 360\nTranslate 0.8 0 0\nSphere 0.3 -0.3 0.3 360\nTranslate 0.8 0 0\nSphere 0.3 -0.3 0.3 360\n", f);
        fclose(f);
    }

    Session s(1, "sobol");
    std::vector<CObject *> added = s.emit([&] {
        CDelayedData *data = new CDelayedData();
        data->generator = strdup(archive.c_str());
        initv(data->bmin, -3.0f, -3.0f, -3.0f);
        initv(data->bmax, 3.0f, 3.0f, 3.0f);
        RtBound bound = {-3, 3, -3, 3, -3, 3};
        RiProcedural(data, bound, RiProcDelayedReadArchive, RiProcFree);
    });
    CHECK(added.size() == 1);
    s.render(); // expands the archive
    if (added.size() == 1) {
        const CObject *parent = added[0];
        std::vector<uint64_t> children;
        for (const CObject *c = parent->children; c != nullptr; c = c->sibling)
            children.push_back(c->stableId);
        CHECK(children.size() == 3);

        std::vector<uint64_t> expected;
        for (uint64_t k = 0; k < children.size(); ++k)
            expected.push_back(mixChild(parent->stableId, k));
        std::sort(children.begin(), children.end());
        std::sort(expected.begin(), expected.end());
        CHECK(children == expected);
    }
    s.close();
    unlink(archive.c_str());
    rmdir(dir.c_str());
}

static void tessellation() {
    Session s(1, "sobol");

    int nverts[2] = {4, 4};
    int verts[8] = {0, 1, 4, 3, 1, 2, 5, 4};
    float M[6 * 3];
    gridPoints(M, 3, 2, 0.8f);
    std::vector<CObject *> mesh = s.emit([&] { RiPointsPolygons(2, nverts, verts, RI_P, M, RI_NULL); });

    float P[7 * 4 * 3];
    gridPoints(P, 7, 4, 0.8f);
    std::vector<CObject *> patchMesh = s.emit([&] {
        RiAttributeBegin();
        RiTranslate(0, 1.5f, 0);
        RiPatchMesh(RI_BICUBIC, 7, RI_NONPERIODIC, 4, RI_NONPERIODIC, RI_P, P, RI_NULL);
        RiAttributeEnd();
    });

    for (const std::vector<CObject *> *group : {&mesh, &patchMesh}) {
        CHECK(group->size() == 1);
        if (group->size() != 1)
            continue;
        CObject *parent = (*group)[0];
        parent->materialize(s.ctx()); // builds the children

        std::vector<uint64_t> children;
        for (const CObject *c = parent->children; c != nullptr; c = c->sibling)
            children.push_back(c->stableId);
        CHECK(children.size() >= 2);

        // The child index is its position in the order the parent made them; the
        // sibling chain may be in either order, so compare as sets.
        std::vector<uint64_t> expected;
        for (uint64_t k = 0; k < children.size(); ++k)
            expected.push_back(mixChild(parent->stableId, k));
        std::sort(children.begin(), children.end());
        std::sort(expected.begin(), expected.end());
        CHECK(children == expected);
    }
    s.close();
}

static void splits() {
    // A bicubic patch that fills the view, with a small grid limit so dicing has
    // to split it.
    Session s(1, "sobol", 64, 48, /*gridSize*/ 8);
    float P[16 * 3];
    gridPoints(P, 4, 4, 3.0f);
    std::vector<CObject *> patch = s.emit([&] { RiPatch(RI_BICUBIC, RI_P, P, RI_NULL); });
    CHECK(patch.size() == 1);
    s.render();

    // RiPatch adds a patch mesh whose only child is the surface that gets shaded,
    // and dicing splits that surface into lightweight views of it: the pieces are
    // seen as separate grids, not as separate surfaces.
    std::set<uint64_t> ids;
    for (const Report &r : reports())
        ids.insert(r.stableId);
    std::fprintf(stderr, "  %zu shaded grid(s), %zu distinct identity(ies)\n", reports().size(), ids.size());
    CHECK(reports().size() > 1); // the patch was split
    CHECK(ids.size() == 1);      // and every piece kept the same identity
    if (patch.size() == 1 && ids.size() == 1)
        CHECK(*ids.begin() == mixChild(patch[0]->stableId, 0)); // that of the mesh's child
    s.close();
}

int main(int argc, char **argv) {
    const std::string which = argc >= 2 ? argv[1] : "";
    if (which == "scene_loads")
        sceneLoads();
    else if (which == "instances")
        instances();
    else if (which == "delayed")
        delayed();
    else if (which == "tessellation")
        tessellation();
    else if (which == "splits")
        splits();
    else {
        std::fprintf(stderr, "usage: %s scene_loads|instances|delayed|tessellation|splits\n", argv[0]);
        return 2;
    }
    if (failures) {
        std::fprintf(stderr, "FAILED: %d check(s)\n", failures);
        return 1;
    }
    return 0;
}
