/**
 * Project: openRender
 *
 * File: traceCalls.cpp
 *
 * Description:
 *   See traceCalls.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "traceCalls.h"

#include <atomic>

#include "common/diagnostics.h"
#include "ray.h"
#include "rendererServices.h"
#include "shading.h"
#include "tracer.h"

namespace libshader {

    namespace {

        // One warning per kind of request per process: the first request of a kind made with no tracer reports it
        std::atomic<bool> s_warned[6] = {false, false, false, false, false, false};

        const char *kindName(TraceKind kind) {
            switch (kind) {
                case TraceKind::Trace:
                    return "trace";
                case TraceKind::Transmission:
                    return "transmission";
                case TraceKind::Visibility:
                    return "visibility";
                case TraceKind::Gather:
                    return "gather";
                case TraceKind::Occlusion:
                    return "occlusion";
                case TraceKind::IndirectDiffuse:
                    return "indirectdiffuse";
            }
            return "trace";
        }

        int requestKind(TraceKind kind) {
            switch (kind) {
                case TraceKind::Trace:
                    return TRACEREQUEST_TRACE;
                case TraceKind::Transmission:
                    return TRACEREQUEST_TRANSMISSION;
                case TraceKind::Visibility:
                    return TRACEREQUEST_VISIBILITY;
                case TraceKind::Gather:
                    return TRACEREQUEST_GATHER;
                case TraceKind::Occlusion:
                    return TRACEREQUEST_OCCLUSION;
                case TraceKind::IndirectDiffuse:
                    return TRACEREQUEST_INDIRECTDIFFUSE;
            }
            return TRACEREQUEST_TRACE;
        }

        openrender::shading::ITracer *tracerOf(CShadingContext &context, TraceKind kind) {
            CRendererServices *svc = context.getServices();
            openrender::shading::ITracer *tracer = svc != nullptr ? svc->tracer() : nullptr;
            if (tracer == nullptr && !s_warned[(int)kind].exchange(true))
                warning(CODE_NOTSTARTED, "renderer services provide no tracer: %s rays are treated as misses\n", kindName(kind));
            return tracer;
        }

    } // namespace

    void traceRay(CShadingContext &context, CRay *ray, TraceKind kind, const char *label) {
        if (CShadingContext::traceRequestObserver != nullptr)
            context.reportTraceRequest(requestKind(kind), label, 0, 1, &ray);

        openrender::shading::ITracer *tracer = tracerOf(context, kind);
        if (tracer != nullptr)
            tracer->traceRay(context, ray);
        else
            ray->object = nullptr;
    }

    void traceBundle(CShadingContext &context, CRayBundle *bundle, TraceKind kind) {
        if (CShadingContext::traceRequestObserver != nullptr)
            context.reportTraceRequest(requestKind(kind), bundle->label, bundle->depth, bundle->numRays, bundle->rays);

        openrender::shading::ITracer *tracer = tracerOf(context, kind);
        if (tracer != nullptr)
            tracer->traceBundle(context, bundle);
        else
            bundle->postShade(bundle->numRays, bundle->rays);
    }

} // namespace libshader
