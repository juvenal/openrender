/**
 * tests/shading_system/test_cache.cpp
 *
 * ShadingSystem_Cache (spec 033, FR-005, GitHub #167, research R10). The description cache:
 *   - one load per name, however often it is asked for;
 *   - the first format that loaded a name wins for every later request, whatever the preference says then (#167, kept);
 *   - the cache key is the description's own name, never the request's pointer: the RIB parser rewinds the memory a
 *     request's name points into before the next statement, so the key must not alias it;
 *   - two concurrent first requests for one name load it once (find() serialises find, load and insert).
 * Uses two test backends ("tsa", "tsb") so that "which format won" is visible. Red on the starting tree: shadingSystem.h and
 * shaderBackend.h do not exist (a build failure).
 */
#include "test_backend.h"

using namespace sstest;

int main() {
    CContractServices svc;
    CHarness h(svc, 4);
    CTstBackend a("tsa"), b("tsb");
    registerShaderBackend(&a, 5);
    registerShaderBackend(&b, 4);
    tstLog().reset();
    {
        ShadingSystem system;
        CMapResolver resolver;
        resolver.add("cached", "tsa", writeTstShader("cached", "surface", 1, 0, 0));
        resolver.add("cached", "tsb", writeTstShader("cached-b", "surface", 0, 1, 0));

        // one load per name; the first format wins for the later requests whatever the preference says
        CShader *first = system.find({"cached", SL_SURFACE, "tsa", nullptr, &resolver});
        CT_EXPECT(first != nullptr && strcmp(first->backend->key(), "tsa") == 0);
        CShader *second = system.find({"cached", SL_SURFACE, "tsb", nullptr, &resolver});
        CT_EXPECT(second == first);
        CT_EXPECT(tstLog().loads == 1);
        CT_EXPECT(resolver.asked("cached.tsb") == 0); // not even asked for the other format

        // the key is the description's own name: overwrite the memory the first request's name pointed into
        char scratch[16];
        strcpy(scratch, "volatile");
        resolver.add("volatile", "tsa", writeTstShader("volatile", "surface", 0, 0, 1));
        CShader *v = system.find({scratch, SL_SURFACE, "tsa", nullptr, &resolver});
        CT_EXPECT(v != nullptr);
        strcpy(scratch, "xxxxxxxx"); // what a rewound arena would hold now
        CT_EXPECT(system.find({"volatile", SL_SURFACE, "tsa", nullptr, &resolver}) == v);
        CT_EXPECT(tstLog().loads == 2);
        CT_EXPECT(system.find({scratch, SL_SURFACE, "tsa", nullptr, &resolver}) == nullptr); // no such shader, and no stale hit

        // two concurrent first requests load once
        resolver.add("racing", "tsa", writeTstShader("racing", "surface", 1, 1, 1));
        tstLog().loadDelayMs = 100;
        const int before = tstLog().loads;
        CShader *seen[2] = {nullptr, nullptr};
        std::thread t0([&] { seen[0] = system.find({"racing", SL_SURFACE, "tsa", nullptr, &resolver}); });
        std::thread t1([&] { seen[1] = system.find({"racing", SL_SURFACE, "tsa", nullptr, &resolver}); });
        t0.join();
        t1.join();
        CT_EXPECT(seen[0] != nullptr && seen[0] == seen[1]);
        CT_EXPECT(tstLog().loads == before + 1);
    }
    unregisterShaderBackend("tsa");
    unregisterShaderBackend("tsb");
    return contract_test::finish();
}
