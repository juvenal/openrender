/**
 * Project: openRender
 *
 * File: traceCalls.h
 *
 * Description:
 *   The two helpers through which Shading asks for rays (spec 026, contracts/tracer.md): traceRay() for one probe
 *   ray, traceBundle() for a bundle that is traced and shaded. Each reports the request to the test hook
 *   CShadingContext::traceRequestObserver when it is set (null in production), then asks the services' tracer.
 *   With no services or no tracer a request warns once per kind and takes the miss path.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef TRACECALLS_H
#define TRACECALLS_H

#include <cstdint>

class CShadingContext;
class CRay;
class CRayBundle;

namespace libshader {

    // The call-site family of a request, in the order of ETraceRequestKind (the replay's kinds)
    enum class TraceKind : uint8_t { Trace, Transmission, Visibility, Gather, Occlusion, IndirectDiffuse };

    // Trace one ray and do not shade it. `label` is the ray label the request reports to the test hook (a probe carries
    // it nowhere else; may be nullptr). With no tracer: one warning for `kind`, then ray->object = nullptr (a miss).
    void traceRay(CShadingContext &context, CRay *ray, TraceKind kind, const char *label = nullptr);

    // Trace a bundle one level deeper and shade what it hits (the contract of CShadingContext::traceEx()). With no
    // tracer: one warning for `kind`, then bundle->postShade(numRays, rays), the path traceEx() takes past the maximum
    // ray depth.
    void traceBundle(CShadingContext &context, CRayBundle *bundle, TraceKind kind);

} // namespace libshader

#endif
