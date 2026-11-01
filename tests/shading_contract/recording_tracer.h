/**
 * tests/shading_contract/recording_tracer.h
 *
 * The two tracer doubles of spec 026 (contracts/tracer.md, "Doubles"):
 *
 *   CRecordingTracer   appends every request (label, bundle depth, path depth, every ray's inputs)
 *                      to a vector. For traceRay it may answer canned hits: `object` a sentinel
 *                      pointer and `t`. It never runs the bundle protocol, so the standalone program
 *                      (whose load stub traps in the bundles) can use it: constructing a bundle on the
 *                      stack only writes a vtable pointer.
 *
 *   CCannedTracer      rendered program only. Drives the real bundle protocol with canned answers, as
 *                      trace.cpp does: postTraceAction(), then postShade(n, rays, varying) with the
 *                      canned Ci/Oi for "hit" rays or postShade(n, rays) for "miss" rays, then post(),
 *                      repeating while bundle->numRays > 0.
 */
#ifndef TESTS_SHADING_CONTRACT_RECORDING_TRACER_H
#define TESTS_SHADING_CONTRACT_RECORDING_TRACER_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "libshader/shading/rendererServices.h"
#include "libshader/shading/shading.h"
#include "libshader/shading/tracer.h"
#include "ri/core/ray.h"

static_assert(openrender::shading::kRendererServicesVersion.major == 2, "the tracer doubles are written against services version 2");

///////////////////////////////////////////////////////////////////////
// One ray's inputs, as handed to the tracer
///////////////////////////////////////////////////////////////////////
struct STraceRay {
        float from[3], dir[3];
        float t, tmin, time;
        int flags;
        float da, db;
        openrender::sampling::SampleKey key;
        uint16_t depth;
};

struct STraceRequest {
        bool bundle = false;        // traceBundle(), or traceRay()
        std::string label;          // "-" when none
        int bundleDepth = 0;        // the bundle's transparency depth
        int pathDepth = 0;          // the first ray's path depth (CRay::depth); the context's own depth is private
        std::vector<STraceRay> rays;
};

inline STraceRay copyRayInputs(const CRay *r) {
    STraceRay o;
    memcpy(o.from, r->from, sizeof(o.from));
    memcpy(o.dir, r->dir, sizeof(o.dir));
    o.t = r->t;
    o.tmin = r->tmin;
    o.time = r->time;
    o.flags = r->flags;
    o.da = r->da;
    o.db = r->db;
    o.key = r->key;
    o.depth = r->depth;
    return o;
}

///////////////////////////////////////////////////////////////////////
// Class         : CRecordingTracer
///////////////////////////////////////////////////////////////////////
class CRecordingTracer : public openrender::shading::ITracer {
    public:
        std::vector<STraceRequest> requests;

        // Canned answers for traceRay(): a hit has `object` set to the sentinel and `t` to hitT
        bool hit = false;
        float hitT = 1.0f;
        // What `object` is set to on a hit: a sentinel pointer, or a real object when the consumer dereferences
        // it (the irradiance cache reads the hit object's attributes); the rendered program sets hitObject
        CSurface *hitObject = nullptr;
        CSurface *sentinel() const { return hitObject != nullptr ? hitObject : reinterpret_cast<CSurface *>(const_cast<CRecordingTracer *>(this)); }

        // Optional: pass requests on to another tracer
        openrender::shading::ITracer *inner = nullptr;
        // The bundle protocol is only run when `runBundles` is set (the rendered program installs an inner tracer)
        bool runBundles = false;

        int numRayRequests() const {
            int n = 0;
            for (const STraceRequest &r : requests)
                if (!r.bundle)
                    ++n;
            return n;
        }
        int numBundleRequests() const { return (int)requests.size() - numRayRequests(); }

        void traceRay(CShadingContext &context, CRay *ray) override {
            STraceRequest q;
            q.bundle = false;
            q.label = "-";
            q.pathDepth = ray->depth;
            q.rays.push_back(copyRayInputs(ray));
            requests.push_back(q);
            if (inner != nullptr) {
                inner->traceRay(context, ray);
            }
            else if (hit) {
                ray->object = sentinel();
                ray->t = hitT;
                ray->u = 0.5f;
                ray->v = 0.5f;
                ray->N[0] = 0;
                ray->N[1] = 0;
                ray->N[2] = 1;
            }
            else {
                ray->object = nullptr;
            }
        }

        void traceBundle(CShadingContext &context, CRayBundle *bundle) override {
            STraceRequest q;
            q.bundle = true;
            q.label = bundle->label != nullptr ? bundle->label : "-";
            q.bundleDepth = bundle->depth;
            q.pathDepth = bundle->numRays > 0 ? bundle->rays[0]->depth : 0;
            for (int i = 0; i < bundle->numRays; i++)
                q.rays.push_back(copyRayInputs(bundle->rays[i]));
            requests.push_back(q);
            if (inner != nullptr && runBundles)
                inner->traceBundle(context, bundle);
        }
};

///////////////////////////////////////////////////////////////////////
// Class         : CCannedTracer
// Description   : Drives the real bundle protocol with canned answers
///////////////////////////////////////////////////////////////////////
class CCannedTracer : public CRecordingTracer {
    public:
        // The answer for a ray: true = a hit with this Ci and Oi, false = a miss
        std::function<bool(const CRay &, float ci[3], float oi[3])> answer = [](const CRay &, float ci[3], float oi[3]) {
            ci[0] = 0.25f;
            ci[1] = 0.5f;
            ci[2] = 0.75f;
            oi[0] = oi[1] = oi[2] = 1.0f;
            return true;
        };

        void traceRay(CShadingContext &context, CRay *ray) override {
            CRecordingTracer::traceRay(context, ray);
            float ci[3], oi[3];
            if (answer(*ray, ci, oi)) {
                ray->object = sentinel();
                ray->t = hitT;
            }
            else {
                ray->object = nullptr;
            }
        }

        void traceBundle(CShadingContext &context, CRayBundle *bundle) override {
            (void)context;
            // Record the request exactly as the recording tracer does
            STraceRequest q;
            q.bundle = true;
            q.label = bundle->label != nullptr ? bundle->label : "-";
            q.bundleDepth = bundle->depth;
            q.pathDepth = bundle->numRays > 0 ? bundle->rays[0]->depth : 0;
            for (int i = 0; i < bundle->numRays; i++)
                q.rays.push_back(copyRayInputs(bundle->rays[i]));
            requests.push_back(q);

            CRay **rays = bundle->rays;
            int numRays = bundle->numRays;
            if (numRays == 0)
                return;

            // The scratch "varying" the bundle reads Ci and Oi from: one buffer per global, zero-filled
            const int kGlobals = (int)VARIABLE_CONSTANTWIDTH + 1;
            std::vector<std::vector<float>> storage(kGlobals);
            std::vector<float *> varying(kGlobals, nullptr);
            for (int g = 0; g < kGlobals; g++) {
                storage[g].assign((size_t)(numRays > 1 ? numRays : 1) * 4 + 16, 0.0f);
                varying[g] = storage[g].data();
            }

            while (true) {
                // The hits are shaded together, the misses reported together. Every ray is answered before the bundle looks at
                // the results: trace.cpp traces the rays first, and postTraceAction() counts the misses from ray->object
                std::vector<CRay *> hits, misses;
                std::vector<float> hitCi, hitOi;
                for (int i = 0; i < numRays; i++) {
                    float ci[3], oi[3];
                    CRay *r = rays[i];
                    if (answer(*r, ci, oi)) {
                        r->object = sentinel();
                        r->t = hitT;
                        hits.push_back(r);
                        hitCi.insert(hitCi.end(), ci, ci + 3);
                        hitOi.insert(hitOi.end(), oi, oi + 3);
                    }
                    else {
                        r->object = nullptr;
                        misses.push_back(r);
                    }
                }
                if (bundle->postTraceAction() == FALSE)
                    return;
                if (!hits.empty()) {
                    for (size_t k = 0; k < hits.size(); k++) {
                        memcpy(varying[VARIABLE_CI] + 3 * k, &hitCi[3 * k], 3 * sizeof(float));
                        memcpy(varying[VARIABLE_OI] + 3 * k, &hitOi[3 * k], 3 * sizeof(float));
                    }
                    bundle->postShade((int)hits.size(), hits.data(), varying.data());
                }
                if (!misses.empty())
                    bundle->postShade((int)misses.size(), misses.data());
                bundle->post();

                if ((numRays = bundle->numRays) == 0)
                    break;
                rays = bundle->rays;
            }
        }
};

#endif
