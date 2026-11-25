/*
 * Spec 035, Phase 3: the support header of the closuremode render tests (T058, T059).
 *
 *   CConsumerDouble   an IClosureConsumer that counts the trees it is given, hashes each tree's flattened bytes, and records what the batch
 *                     record said (ray types, shapes, modes, the thread that consumed). Thread safe: consume() is called from every render
 *                     thread (contracts/consumer-and-backend.md rule 2).
 *   SScene / render() one in-process session: a frame of a quad shaded through the delegating cldbl backend, with the options a test
 *                     varies (hider, format, thread count, the scene-wide and the attribute mode, a light, a second object), and everything
 *                     it produced: the image, the consumer's record, the wrapper's counters and outputs, the warnings it reported.
 */
#ifndef TESTS_CLOSURES_MODE_SUPPORT_H
#define TESTS_CLOSURES_MODE_SUPPORT_H

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "cldbl_backend.h"
#include "closureAbi.h"
#include "closureConsumer.h"
#include "closureFrame.h"
#include "closureMode.h"
#include "closureRegistry.h"
#include "closureTree.h"
#include "common/memory.h"
#include "libshader/shading/shading.h"
#include "ri/parse/ri.h"
#include "test_support.h"

namespace clmode {

    using openrender::shading::BatchShape;
    using openrender::shading::ClosureBatch;
    using openrender::shading::ClosureMode;
    using openrender::shading::RayType;

    // FNV-1a over the flattened lobes of a tree: the id, the accumulated weight and the bytes of each component's parameter block
    inline uint64_t hashTree(const OrClosureNode *tree) {
        CMemPage *arena = nullptr;
        memoryInit(arena);
        uint64_t h = 1469598103934665603ull;
        auto mix = [&h](const void *p, size_t n) {
            const unsigned char *b = static_cast<const unsigned char *>(p);
            for (size_t i = 0; i < n; ++i) {
                h ^= b[i];
                h *= 1099511628211ull;
            }
        };
        const openrender::shading::ClosureLobeList list = openrender::shading::closureFlatten(tree, arena);
        for (int i = 0; i < list.count; ++i) {
            const openrender::shading::ClosureLobe &lobe = list.lobes[i];
            mix(&lobe.id, sizeof(lobe.id));
            float w[3];
            lobe.weight.toArray3(w);
            mix(w, sizeof(w));
            const OrClosureEntry *entry = openrender::shading::closureEntry(lobe.id);
            mix(lobe.params, entry != nullptr ? (size_t)entry->blockSize : 0);
        }
        memoryTini(arena);
        return h;
    }

    class CConsumerDouble : public openrender::shading::IClosureConsumer {
        public:
            void consume(CShadingContext &, const ClosureBatch &batch) override {
                std::lock_guard<std::mutex> lock(mutex_);
                ++batches_;
                points_ += batch.numPoints;
                rayTypes_.insert((int)batch.rayType);
                shapes_.insert((int)batch.shape);
                modes_.insert((int)batch.mode);
                threads_.insert(std::this_thread::get_id());
                if (batch.fromBxdf)
                    ++fromBxdf_;
                for (int i = 0; i < batch.numPoints; ++i)
                    if (batch.trees[i] != nullptr) {
                        ++trees_;
                        hashes_.push_back(hashTree(batch.trees[i]));
                        if ((int)batch.rayType == (int)RayType::Camera)
                            ++cameraTrees_;
                        else if ((int)batch.rayType == (int)RayType::Secondary)
                            secondaryHashes_.push_back(hashes_.back());
                    }
            }
            void reset() {
                std::lock_guard<std::mutex> lock(mutex_);
                batches_ = points_ = trees_ = fromBxdf_ = cameraTrees_ = 0;
                rayTypes_.clear();
                shapes_.clear();
                modes_.clear();
                threads_.clear();
                hashes_.clear();
                secondaryHashes_.clear();
            }
            long batches() const { return batches_; }
            long points() const { return points_; }
            long trees() const { return trees_; }
            long cameraTrees() const { return cameraTrees_; }
            long fromBxdf() const { return fromBxdf_; }
            std::set<int> rayTypes() const { return rayTypes_; }
            std::set<int> shapes() const { return shapes_; }
            std::set<int> modes() const { return modes_; }
            size_t threadCount() const { return threads_.size(); }
            std::vector<uint64_t> sortedHashes() const {
                std::vector<uint64_t> h = hashes_;
                std::sort(h.begin(), h.end());
                return h;
            }
            std::vector<uint64_t> secondaryHashes() const { return secondaryHashes_; }

        private:
            mutable std::mutex mutex_;
            long batches_ = 0, points_ = 0, trees_ = 0, fromBxdf_ = 0, cameraTrees_ = 0;
            std::set<int> rayTypes_, shapes_, modes_;
            std::set<std::thread::id> threads_;
            std::vector<uint64_t> hashes_, secondaryHashes_;
    };

    struct SImage {
            int width = 0, height = 0;
            std::vector<float> rgba; // 4 samples per pixel, row major
            bool operator==(const SImage &o) const { return width == o.width && height == o.height && rgba.size() == o.rgba.size() && std::memcmp(rgba.data(), o.rgba.data(), rgba.size() * sizeof(float)) == 0; }
            float at(int x, int y, int k) const { return rgba[((size_t)y * width + x) * 4 + k]; }
    };

    // What a session is asked to do
    struct SScene {
            std::string hider = "raytrace"; // raytrace, zbuffer or reyes
            std::string format = "rslo";    // rslo or slo: the wrapper files are <surface>_<format>.cldbl
            std::string surface = "cl_probe"; // wrapper base name of the front object's Surface: cl_probe (not capable), cl_cap (capable, diffuse),
                                              // cl_capconst (capable, constdiffuse), cl_gate (the gate probe, not capable), cl_gatecap (the gate probe, capable),
                                              // cl_cutcap (cl_cutout, capable); "" binds no Surface
            std::string optionMode;         // Option "shade" "closuremode"; "" = no statement
            std::string attributeMode;      // Attribute "shade" "closuremode" on the front object; "" = no statement
            std::string cameraHitMode;      // Attribute "shade" "camerahitmode" on the front object; "" = no statement
            float opacity = 1.0f;           // the object's Os
            int threads = 1;
            int size = 8;                   // the frame is size x size
            bool light = false;             // the counting light (cl_countlight, category "gate"), 0.5 in front of the object
            bool ambient = false;           // an ambient light (the shipped ambientlight)
            bool occluder = false;          // a second object, a quad at z = 0.5 that camera rays do not see: it catches the rays the front object traces
            std::string occluderSurface = "cl_probe"; // its Surface wrapper
            bool noFront = false;           // no front object (a test builds every object itself in `extra`)
            bool halfFront = false;         // the front object covers only the left half of the frame (x in [-4, 0]); a test adds its own object on the right
            bool consumer = false;          // install a CConsumerDouble as the process default consumer for the session
            std::function<void()> extra;    // extra statements inside the world, before the front object
    };

    struct SRun {
            SImage image;
            long executes = 0, realPoints = 0;  // the wrapper's counters (the point-count oracle: realPoints)
            long lightRuns = 0, lightPoints = 0;
            std::map<std::string, double> outputMean; // the gate probe's outputs (the 17 r_* variables), mean over the real points
            long trees = 0, cameraTrees = 0, batches = 0;
            std::vector<uint64_t> hashes, secondaryHashes;
            std::set<int> rayTypes, shapes, modes;
            size_t consumerThreads = 0;
            int warnings = 0, errors = 0;
            std::vector<std::string> warningTexts;
    };

    inline const char *kOutputs[] = {"r_ambient", "r_diffuse", "r_specular", "r_phong", "r_illum", "r_illumcat", "r_lightsource", "r_occlusion", "r_indirect",
                                     "r_gather", "r_trace", "r_transmission", "r_visibility", "r_environment", "r_shadow", "r_texture", "r_specbrdf"};

    namespace detail {
        struct SState {
                SImage image;
                int samples = 0;
                std::mutex lock;
                std::vector<std::string> warnings;
                int errors = 0;
        };
        inline SState &state() {
            static SState s;
            return s;
        }
        inline void reset(SState &s) {
            std::lock_guard<std::mutex> lock(s.lock);
            s.image = SImage();
            s.samples = 0;
            s.warnings.clear();
            s.errors = 0;
        }
        inline int handleTarget;
        inline void *start(const char *, int w, int h, int n, const char *, RtDisplayParameterFunction) {
            SState &s = state();
            s.image.width = w;
            s.image.height = h;
            s.image.rgba.assign((size_t)w * h * 4, -1.0f);
            s.samples = n;
            return &handleTarget;
        }
        inline int data(void *handle, int x, int y, int w, int h, float *d) {
            if (handle != &handleTarget)
                return 0;
            SState &s = state();
            for (int j = 0; j < h; ++j)
                for (int i = 0; i < w; ++i)
                    for (int k = 0; k < 4 && k < s.samples; ++k)
                        s.image.rgba[((size_t)(y + j) * s.image.width + (x + i)) * 4 + k] = d[(j * w + i) * s.samples + k];
            return 1;
        }
        inline void finish(void *) {}
        inline void handler(RtInt, RtInt severity, const char *message) {
            SState &s = state();
            std::lock_guard<std::mutex> lock(s.lock);
            if (severity == RIE_WARNING)
                s.warnings.push_back(message);
            else if (severity >= RIE_ERROR)
                ++s.errors;
        }
    } // namespace detail

    // One session. The wrappers (cldbl files) and the shaders are the build's (tests/closures/CMakeLists.txt); the cldbl backend is registered by main().
    inline SRun render(const SScene &scene, CConsumerDouble *consumerOut = nullptr) {
        detail::SState &st = detail::state();
        detail::reset(st);
        cldbl::counters().reset();
        cldbl::resetOutputs();
        CConsumerDouble local;
        CConsumerDouble &consumer = consumerOut != nullptr ? *consumerOut : local;
        consumer.reset();
        if (scene.consumer)
            CShadingContext::setDefaultClosureConsumer(&consumer);

        RtInt threads = scene.threads;
        const char *format = scene.format.c_str();
        RtColor colour = {0.25f, 0.5f, 0.75f};
        RtColor opacity = {scene.opacity, scene.opacity, scene.opacity};
        RtPoint front[4] = {{-4, -4, 1}, {4, -4, 1}, {4, 4, 1}, {-4, 4, 1}};
        if (scene.halfFront)
            front[1][0] = front[2][0] = 0;
        RtPoint behind[4] = {{-9, -9, 0.5f}, {9, -9, 0.5f}, {9, 9, 0.5f}, {-9, 9, 0.5f}};
        RiBegin(RI_NULL);
        RiErrorHandler(detail::handler);
        RiOption(RI_LIMITS, RI_NUMTHREADS, &threads, RI_NULL);
        RiOption(RI_SHADERFORMAT, "default", &format, RI_NULL);
        if (!scene.optionMode.empty()) {
            const char *m = scene.optionMode.c_str();
            RiOption(RI_SHADE, RI_CLOSUREMODE, &m, RI_NULL);
        }
        RiHider(scene.hider.c_str(), RI_NULL);
        RiFormat(scene.size, scene.size, 1);
        RiPixelSamples(1, 1);
        RiPixelFilter(RiBoxFilter, 1, 1);
        RiCustomDisplay("cl-mode", RI_RGBA, detail::start, detail::data, detail::finish, RI_NULL);
        RiProjection("orthographic", RI_NULL);
        RiWorldBegin();
        if (scene.light) {
            // a transform block, not an attribute block: AttributeEnd would switch the light off again (RISpec: a light is on for the rest of the
            // attribute block it was made in)
            RiTransformBegin();
            RiTranslate(0, 0, 0.5f);
            RiLightSource((std::string("cl_countlight_") + scene.format).c_str(), RI_NULL);
            RiTransformEnd();
        }
        if (scene.ambient)
            RiLightSource("ambientlight", RI_NULL);
        if (scene.occluder) {
            RiAttributeBegin();
            RtInt hidden = 0;
            RiAttribute("visibility", "int camera", &hidden, RI_NULL);
            RtInt one = 1;
            RiAttribute("visibility", "int specular", &one, "int diffuse", &one, "int transmission", &one, RI_NULL);
            RiColor(colour);
            RiSurface((scene.occluderSurface + "_" + scene.format).c_str(), RI_NULL);
            RiPolygon(4, RI_P, (RtPointer)behind, RI_NULL);
            RiAttributeEnd();
        }
        if (scene.extra)
            scene.extra();
        if (!scene.noFront) {
        RiAttributeBegin();
        if (!scene.attributeMode.empty()) {
            const char *m = scene.attributeMode.c_str();
            RiAttribute(RI_SHADE, RI_CLOSUREMODE, &m, RI_NULL);
        }
        if (!scene.cameraHitMode.empty()) {
            const char *m = scene.cameraHitMode.c_str();
            RiAttribute(RI_SHADE, RI_CAMERAHITMODE, &m, RI_NULL);
        }
        RiColor(colour);
        RiOpacity(opacity);
        RiSurface(scene.surface.empty() ? "null" : (scene.surface + "_" + scene.format).c_str(), RI_NULL); // "" is no surface at all (E7)
        RiPolygon(4, RI_P, (RtPointer)front, RI_NULL);
        RiAttributeEnd();
        }
        RiWorldEnd();
        RiEnd();
        if (scene.consumer)
            CShadingContext::setDefaultClosureConsumer(nullptr);

        SRun run;
        run.image = st.image;
        run.executes = cldbl::counters().executes;
        run.realPoints = cldbl::counters().realPoints;
        run.lightRuns = cldbl::lightRuns();
        run.lightPoints = cldbl::lightPoints();
        for (const char *name : kOutputs)
            run.outputMean[name] = cldbl::outputMean(name);
        run.trees = consumer.trees();
        run.cameraTrees = consumer.cameraTrees();
        run.batches = consumer.batches();
        run.hashes = consumer.sortedHashes();
        run.secondaryHashes = consumer.secondaryHashes();
        run.rayTypes = consumer.rayTypes();
        run.shapes = consumer.shapes();
        run.modes = consumer.modes();
        run.consumerThreads = consumer.threadCount();
        run.warnings = (int)st.warnings.size();
        run.warningTexts = st.warnings;
        run.errors = st.errors;
        return run;
    }

    // How many of the run's warnings name `text` (a shader name or a mode)
    inline int warningsNaming(const SRun &run, const std::string &a, const std::string &b = "") {
        int n = 0;
        for (const std::string &w : run.warningTexts)
            if (w.find(a) != std::string::npos && (b.empty() || w.find(b) != std::string::npos))
                ++n;
        return n;
    }

} // namespace clmode

#endif
