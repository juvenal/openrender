/*
 * Spec 035, Phase 4: the session driver of the Bxdf statement and shading tests (T083, T084). Like clmode::render() (mode_support.h, which is an
 * approved Phase 3 file and is not edited) it renders a quad through the delegating cldbl backend and returns the same SRun, and it adds the
 * three places a Bxdf test needs statements: before WorldBegin (a Bxdf or a Surface bound ahead of the world, analyze B2), inside the world before
 * the front object, and inside the front object's own attribute block (after Color and Opacity, before the polygon).
 */
#ifndef TESTS_CLOSURES_BXDF_SUPPORT_H
#define TESTS_CLOSURES_BXDF_SUPPORT_H

#include <functional>
#include <string>

#include "mode_support.h"

namespace clbx {

    using clmode::CConsumerDouble;
    using clmode::SImage;
    using clmode::SRun;

    struct SBxdfScene {
            std::string hider = "raytrace"; // raytrace, zbuffer or reyes
            std::string format = "rslo";    // rslo or slo
            int threads = 1;
            int size = 8;                   // the frame is size x size
            float opacity = 1.0f;           // the front object's Os
            bool consumer = false;          // install a CConsumerDouble as the process default consumer
            float tilt = 0.0f;              // degrees: the front quad is rotated about x, so I.N < 1 and a default surface differs from the no-shader path
            bool noFront = false;           // no front object: the test builds every object itself in `world`
            std::function<void()> preWorld; // before RiWorldBegin (after the projection)
            std::function<void()> world;    // inside the world, before the front object
            std::function<void()> front;    // inside the front object's attribute block, after Color and Opacity, before the polygon
    };

    inline SRun render(const SBxdfScene &scene, CConsumerDouble *consumerOut = nullptr) {
        clmode::detail::SState &st = clmode::detail::state();
        clmode::detail::reset(st);
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
        RiBegin(RI_NULL);
        RiErrorHandler(clmode::detail::handler);
        RiOption(RI_LIMITS, RI_NUMTHREADS, &threads, RI_NULL);
        RiOption(RI_SHADERFORMAT, "default", &format, RI_NULL);
        RiHider(scene.hider.c_str(), RI_NULL);
        RiFormat(scene.size, scene.size, 1);
        RiPixelSamples(1, 1);
        RiPixelFilter(RiBoxFilter, 1, 1);
        RiCustomDisplay("cl-bxdf", RI_RGBA, clmode::detail::start, clmode::detail::data, clmode::detail::finish, RI_NULL);
        RiProjection("orthographic", RI_NULL);
        if (scene.preWorld)
            scene.preWorld();
        RiWorldBegin();
        if (scene.world)
            scene.world();
        if (!scene.noFront) {
            RiAttributeBegin();
            RiColor(colour);
            RiOpacity(opacity);
            if (scene.front)
                scene.front();
            if (scene.tilt != 0.0f)
                RiRotate(scene.tilt, 1, 0, 0);
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

    // A quad of the frame (orthographic, screen units of the front object: the frame is [-4, 4] on both axes), tilted like the front object
    inline void quad(float x0, float y0, float x1, float y1, float tilt = 0.0f) {
        RtPoint p[4] = {{x0, y0, 1}, {x1, y0, 1}, {x1, y1, 1}, {x0, y1, 1}};
        if (tilt != 0.0f)
            RiRotate(tilt, 1, 0, 0);
        RiPolygon(4, RI_P, (RtPointer)p, RI_NULL);
    }

    // `Bxdf "name"` with no handle and no parameters
    inline void bxdf(const char *name) { RiBxdf(name, RI_NULL, RI_NULL); }

} // namespace clbx

#endif
