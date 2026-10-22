// Shared helpers for the spec 039 Phase 4 scene tests (test_object_ids.cpp,
// test_cell_keys.cpp): an in-process Ri session that records every object the
// scene creates, and a collector for the per-point keys the shading engine
// reports.
//
// These tests are written before the production code (tests first). They use
// two things that do not exist yet and are defined by this phase:
//
//   CObject::stableId                     (T044) the object's stable identity
//   CShadingContext::pointKeyObserver     (T049/T050) a test hook. When non-null,
//       shade() calls it once per grid, after it has filled the per-point
//       keys, with a CPointKeyReport:
//
//           struct CPointKeyReport {
//               const CSurface *surface;  // the surface being shaded
//               int numVertices;          // real vertices (no derivative tail)
//               const openrender::sampling::SampleKey *keys;
//               const float *u, *v;       // the parametric coordinates
//           };
//           typedef void (*CPointKeyObserver)(const CPointKeyReport &);
//           static CPointKeyObserver pointKeyObserver;   // null in production
//
//       Keys are filled only when the sampler is not Legacy, so the scenes ask
//       for "sobol". The hook stands in for the "probe shader that writes its
//       key to an AOV" of tasks.md T042: no shading-language accessor for a
//       point key exists, and random() does not draw from the keys until
//       Phase 6, so an image-based probe would pass on legacy noise.
#ifndef SCENE_PROBE_H
#define SCENE_PROBE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "common/sampling.h"
#include "delayed.h"
#include "object.h"
#include "renderer.h"
#include "rendererContext.h"
#include "ri.h"
#include "riHooks.h"
#include "shading.h"
#include "surface.h"

namespace probe {

    namespace sp = openrender::sampling;

    // --- the point-key collector -------------------------------------------

    struct Report {
            const CSurface *surface;
            uint64_t stableId;
            std::vector<sp::SampleKey> keys;
            std::vector<float> u, v;
    };

    inline std::mutex &reportMutex() {
        static std::mutex m;
        return m;
    }
    inline std::vector<Report> &reports() {
        static std::vector<Report> r;
        return r;
    }

    inline void observe(const CPointKeyReport &r) {
        std::lock_guard<std::mutex> lock(reportMutex());
        Report rep;
        rep.surface = r.surface;
        rep.stableId = r.surface->stableId;
        rep.keys.assign(r.keys, r.keys + r.numVertices);
        rep.u.assign(r.u, r.u + r.numVertices);
        rep.v.assign(r.v, r.v + r.numVertices);
        reports().push_back(std::move(rep));
    }

    // --- the recording context ---------------------------------------------

    class CCapture : public CRendererContext {
        public:
            std::vector<CObject *> objects; // every object added, in order, with a reference held
            std::mutex mutex;

            void addObject(CObject *o) override {
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    o->attach();
                    objects.push_back(o);
                }
                CRendererContext::addObject(o);
            }
    };

    inline CCapture *&captureSlot() {
        static CCapture *slot = nullptr;
        return slot;
    }
    inline CRiInterface *makeCapture() {
        captureSlot() = new CCapture();
        return captureSlot();
    }

    // --- a session -----------------------------------------------------------

    // One frame: options, camera, then the caller emits the world and calls
    // render(). Use one Session per process-level case (GitHub #2).
    class Session {
        public:
            Session(int threads, const char *sampler, int xres = 64, int yres = 48, int gridSize = 0) {
                reports().clear();
                RiSetContextFactory(makeCapture);
                RiBegin(RI_NULL);
                RtString name = const_cast<RtString>(sampler);
                RiOption((RtToken) "sampling", (RtToken) "sampler", &name, RI_NULL);
                RiOption((RtToken) "limits", (RtToken) "numthreads", &threads, RI_NULL);
                if (gridSize > 0)
                    RiOption((RtToken) "limits", (RtToken) "gridsize", &gridSize, RI_NULL);
                RiFormat(xres, yres, 1.0f);
                RiPixelSamples(1, 1);
                RiDisplay("scene_probe.tif", "file", "rgba", RI_NULL);
                RiProjection("perspective", RI_NULL);
                RiTranslate(0, 0, 5);
                RiWorldBegin();
                CShadingContext::pointKeyObserver = &observe;
            }

            CCapture *capture() const { return captureSlot(); }
            CShadingContext *ctx() const { return CRenderer::contexts != nullptr ? CRenderer::contexts[0] : nullptr; }

            // Run the Ri calls and return the objects they added.
            std::vector<CObject *> emit(const std::function<void()> &calls) {
                const size_t before = capture()->objects.size();
                calls();
                return std::vector<CObject *>(capture()->objects.begin() + before, capture()->objects.end());
            }

            void render() { RiWorldEnd(); }

            void close() {
                CShadingContext::pointKeyObserver = nullptr;
                for (CObject *o : capture()->objects)
                    o->detach();
                capture()->objects.clear();
                RiEnd();
                RiSetContextFactory(nullptr);
                captureSlot() = nullptr;
            }
    };

    // --- scenes ----------------------------------------------------------------

    // A 3x3 grid of control points in the z = 0 plane, scaled by s.
    inline void gridPoints(float *P, int nu, int nv, float s) {
        for (int j = 0; j < nv; ++j)
            for (int i = 0; i < nu; ++i) {
                float *p = P + 3 * (j * nu + i);
                p[0] = s * (2.0f * i / (nu - 1) - 1.0f);
                p[1] = s * (2.0f * j / (nv - 1) - 1.0f);
                p[2] = 0.0f;
            }
    }

} // namespace probe

#endif
