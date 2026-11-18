/**
 * Project: openRender
 *
 * File: areaLights.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry): the Hiders' side of IAreaLightSampler (libshader/shading/areaLightSampler.h). A CHider answers
 *   whether a light is bound and what its controls are from the frame's emitter registry (Geometry), and draws points on a bound
 *   light's emitters: the registry's set chooses a leaf and a point uniformly by area (a pure function of the two numbers Shading
 *   draws), and this file evaluates the leaf's own globals at the drawn point, at the receiver's time, the way a ray hit does (the
 *   leaf's sample() and interpolate()) but without running a shader.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <string.h>

#include "common/algebra.h"
#include "common/memory.h"
#include "declarations.h"
#include "emitterRegistry.h"
#include "hider.h"
#include "object.h"

using namespace openrender::shading;

AreaLightInfo CHider::areaLight(CShadingContext &, const CShaderGroupRefTarget *light) {
    AreaLightInfo info;
    const CEmitterRegistry &registry = emitterRegistry();
    const CEmitterSet *set = registry.find(light);
    if (set == nullptr)
        return info; // a classic light

    // Resolution is done once for the frame by the renderer core before any thread renders; call_once makes a later call free (and
    // covers a context the core did not prepare)
    set->resolve(*this);

    info.bound = true;
    info.lightSamples = set->lightSamples();
    info.lightMode = static_cast<AreaLightMode>(static_cast<uint8_t>(set->lightMode()));
    info.sceneSamples = registry.sceneSamples();
    info.sceneMode = static_cast<AreaLightMode>(static_cast<uint8_t>(registry.sceneMode()));
    info.totalArea = set->totalArea();
    return info;
}

void CHider::sampleEmitters(CShadingContext &, const CShaderGroupRefTarget *light, int m, const float *u1, const float *u2, const float *time, float **out, float *pdfA) {
    const CEmitterSet *set = emitterRegistry().find(light);

    // One scratch table for the call: a buffer of one point for every global the leaf's sample() may write (it writes only what the
    // primitive carries, but never through a null pointer), the nine light-surface globals pointing at the output slots
    const int numGlobals = CDeclarations::numGlobals();
    memBegin(context.threadMemory);
    float **vary = (float **)ralloc(sizeof(float *) * numGlobals, context.threadMemory);
    float ***locals = (float ***)ralloc(sizeof(float **) * NUM_ACCESSORS, context.threadMemory); // no shader locals bound
    for (int a = 0; a < NUM_ACCESSORS; a++)
        locals[a] = nullptr;
    for (int g = 0; g < numGlobals; g++) {
        const CVariable *var = CDeclarations::global(g);
        const int bytes = (int)(var->numFloats * (sizeof(char *) > sizeof(float) ? sizeof(char *) : sizeof(float)));
        vary[g] = (float *)ralloc(bytes, context.threadMemory);
        memset(vary[g], 0, bytes);
    }

    for (int i = 0; i < m; i++) {
        CAreaSample s;
        if (set != nullptr)
            set->sample(u1[i], u2[i], s);
        if (set == nullptr || s.leaf == nullptr || !(s.pdfA > 0.0f)) {
            pdfA[i] = 0.0f;
            for (int c = 0; c < 3; c++) {
                out[VARIABLE_P][3 * i + c] = 0.0f;
                out[VARIABLE_N][3 * i + c] = 0.0f;
                out[VARIABLE_NG][3 * i + c] = 0.0f;
                out[VARIABLE_DPDU][3 * i + c] = 0.0f;
                out[VARIABLE_DPDV][3 * i + c] = 0.0f;
            }
            out[VARIABLE_U][i] = out[VARIABLE_V][i] = out[VARIABLE_S][i] = out[VARIABLE_T][i] = 0.0f;
            continue;
        }

        // Evaluate the leaf at the drawn (u, v) at the receiver's time, as a ray hit would (the surface's own sample and interpolate)
        float *saved[VARIABLE_CONSTANTWIDTH + 1];
        for (unsigned int var = 0; var <= VARIABLE_CONSTANTWIDTH; var++)
            saved[var] = vary[var];
        vary[VARIABLE_P] = out[VARIABLE_P] + 3 * i;
        vary[VARIABLE_N] = out[VARIABLE_N] + 3 * i;
        vary[VARIABLE_NG] = out[VARIABLE_NG] + 3 * i;
        vary[VARIABLE_DPDU] = out[VARIABLE_DPDU] + 3 * i;
        vary[VARIABLE_DPDV] = out[VARIABLE_DPDV] + 3 * i;
        vary[VARIABLE_U] = out[VARIABLE_U] + i;
        vary[VARIABLE_V] = out[VARIABLE_V] + i;
        vary[VARIABLE_S] = out[VARIABLE_S] + i;
        vary[VARIABLE_T] = out[VARIABLE_T] + i;
        float t0 = time[i];
        vary[VARIABLE_TIME] = &t0;
        *vary[VARIABLE_U] = s.u;
        *vary[VARIABLE_V] = s.v;
        out[VARIABLE_N][3 * i] = out[VARIABLE_N][3 * i + 1] = out[VARIABLE_N][3 * i + 2] = 0.0f;

        unsigned int up = PARAMETER_P | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_N | PARAMETER_S | PARAMETER_T;
        s.leaf->sample(0, 1, vary, locals, up);
        s.leaf->interpolate(1, vary, locals);

        // What the shading engine's complete() adds to a ray hit: s and t default to u and v (or the attribute's own s, t coordinates),
        // Orientation flips Ng, and N is Ng unless the primitive carries normals
        const CAttributes *attributes = s.leaf->attributes;
        const float u = *vary[VARIABLE_U], v = *vary[VARIABLE_V];
        if (up & PARAMETER_S) {
            if (attributes->flags & ATTRIBUTES_FLAGS_CUSTOM_ST) {
                const float *c = attributes->s;
                *vary[VARIABLE_S] = (float)((c[0] * (1.0 - u) + c[1] * u) * (1.0 - v) + (c[2] * (1.0 - u) + c[3] * u) * v);
            }
            else
                *vary[VARIABLE_S] = u;
        }
        if (up & PARAMETER_T) {
            if (attributes->flags & ATTRIBUTES_FLAGS_CUSTOM_ST) {
                const float *c = attributes->t;
                *vary[VARIABLE_T] = (float)((c[0] * (1.0 - u) + c[1] * u) * (1.0 - v) + (c[2] * (1.0 - u) + c[3] * u) * v);
            }
            else
                *vary[VARIABLE_T] = v;
        }
        if (attributes->flags & ATTRIBUTES_FLAGS_INSIDE)
            mulvf(vary[VARIABLE_NG], -1);
        if (up & PARAMETER_N) {
            movvv(vary[VARIABLE_N], vary[VARIABLE_NG]);
            if (dotvv(vary[VARIABLE_N], vary[VARIABLE_N]) < C_EPSILON * C_EPSILON)
                initv(vary[VARIABLE_N], 0.0f, 1.0f, 0.0f);
        }
        pdfA[i] = s.pdfA;

        for (unsigned int var = 0; var <= VARIABLE_CONSTANTWIDTH; var++)
            vary[var] = saved[var];
    }
    memEnd(context.threadMemory);
}
