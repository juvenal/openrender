/*
 * Spec 035, Phase 3 (T059), label closuremode: the determinism of the delivered trees.
 *
 *   ThreadIdentity_<hider>   the consumer double's tree hashes (the multiset) at -t:1 equal those at -t:4, and so do the images (SC-006)
 *   HiderParity              ClosureDet_HiderParity (C1, FR-022): a flat, constant-attribute object under a capable cldbl Surface whose
 *                            tree depends on Cs alone (cl_capconst) is shaded by reyes, zbuffer and raytrace: the SET of distinct tree
 *                            hashes is identical across the three hiders. The same wrapper kind on an occluder that only secondary rays
 *                            reach gives the same hash through a different ray type (the front object's batches are Camera for raytrace and
 *                            None for the grid hiders, the interim rule; the occluder's are Secondary) and, for reyes, a different batch
 *                            shape (a grid for the front object, a span for the occluder; raytrace shades every hit as a span): the same
 *                            input gives the same tree through a grid and a span batch (FR-022; ClosureDet_BatchShape)
 *
 * Red on the starting tree: does not compile (closureConsumer.h, the closuremode token).
 */
#include <cstdio>
#include <set>
#include <string>

#include "mode_support.h"

using namespace clmode;

namespace {

    void sectionThreadIdentity(const std::string &hider) {
        SScene s;
        s.hider = hider;
        s.surface = "cl_cap";
        s.attributeMode = "both";
        s.consumer = true;
        s.size = 16;
        s.threads = 1;
        const SRun one = render(s);
        s.threads = 4;
        const SRun four = render(s);
        CL_CHECK(one.trees > 0);
        CL_CHECK(one.trees == four.trees);
        CL_CHECK(one.hashes == four.hashes);
        CL_CHECK(one.image == four.image);
        // closures too: the trees and the (zero) image
        s.attributeMode = "closures";
        s.threads = 1;
        const SRun c1 = render(s);
        s.threads = 4;
        const SRun c4 = render(s);
        CL_CHECK(c1.hashes == c4.hashes && c1.image == c4.image);
    }

    void sectionHiderParity() {
        std::set<uint64_t> sets[3];
        int i = 0;
        for (const char *hider : {"reyes", "zbuffer", "raytrace"}) {
            SScene s;
            s.hider = hider;
            s.surface = "cl_capconst";
            s.attributeMode = "both";
            s.consumer = true;
            const SRun r = render(s);
            CL_CHECK(r.trees > 0);
            sets[i++] = std::set<uint64_t>(r.hashes.begin(), r.hashes.end());
        }
        CL_CHECK(sets[0].size() == 1 && sets[0] == sets[1] && sets[1] == sets[2]); // one distinct tree (one Cs), the same on all three hiders

        // the same input through another batch shape and ray type: the front object traces (gate probe, constdiffuse), an occluder with the same Cs
        // and the same wrapper kind is reached by the secondary rays only
        for (const char *hider : {"raytrace", "reyes"}) {
            SScene s;
            s.hider = hider;
            s.surface = "cl_gatecapconst";
            s.occluder = true;
            s.occluderSurface = "cl_capconst";
            s.attributeMode = "both";
            s.consumer = true;
            // the occluder must be shaded as `both` too: the scene-wide option
            s.optionMode = "both";
            const SRun r = render(s);
            // the front object's hits: a camera ray's for raytrace, a rasterised grid's (ray type None, the interim rule of
            // contracts/consumer-and-backend.md) for reyes; the occluder's: secondary
            CL_CHECK(r.rayTypes.count((int)(std::string(hider) == "raytrace" ? RayType::Camera : RayType::None)) == 1);
            CL_CHECK(r.rayTypes.count((int)RayType::Secondary) == 1);
            CL_CHECK(!r.secondaryHashes.empty());
            const std::set<uint64_t> secondary(r.secondaryHashes.begin(), r.secondaryHashes.end());
            const std::set<uint64_t> all(r.hashes.begin(), r.hashes.end());
            CL_CHECK(secondary.size() == 1 && all.size() == 1); // camera hits and secondary hits: one tree, the same
            if (std::string(hider) == "reyes")
                CL_CHECK(r.shapes.size() >= 2);                 // a grid and a span: the same input through two batch shapes (raytrace shades every hit as a span)
        }
    }

} // namespace

int main(int argc, char **argv) {
    cldbl::registerBackend();
    const std::string which = argc > 1 ? argv[1] : "";
    if (which.rfind("ThreadIdentity_", 0) == 0)
        sectionThreadIdentity(which.substr(strlen("ThreadIdentity_")));
    else if (which == "HiderParity")
        sectionHiderParity();
    else {
        std::printf("usage: test_mode_det <ThreadIdentity_<hider>|HiderParity>\n");
        cldbl::unregisterBackend();
        return 2;
    }
    cldbl::unregisterBackend();
    return CL_RESULT(("ClosureDet_" + which).c_str());
}
