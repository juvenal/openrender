/**
 * Project: openRender
 *
 * File: areaLightSampler.h
 *
 * Description:
 *   Spec 057 (area-light geometry), services 2.6.0. How Shading asks the hider that owns its context for a point on a bound
 *   area light's emitters, and for what the light's controls are. Shading owns the interface; CHider implements it and installs
 *   it on its context (CShadingContext::setAreaLightSampler) at the start of its rendering loop, only when the frame has a
 *   bound light. A context with no sampler runs every light as a classic light.
 *
 *   The light shader runs once per sample, 0-D, with the light-surface globals (P, N, Ng, dPdu, dPdv, u, v, s, t) of the
 *   sampled point; Shading weights what it returns by exactly V / (n p_A) (V the binary visibility, n the sample count, p_A the
 *   density by area). Nothing else of the emitter's geometry crosses this interface.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef AREALIGHTSAMPLER_H
#define AREALIGHTSAMPLER_H

#include <cstdint>

#include "stateHandles.h" // CShaderGroupRefTarget: the identity of a light (Shading -> Graphics State)

class CShadingContext;

namespace openrender::shading {

    // The area-light mode of the "arealight" attribute and option. The values equal Graphics State's EAreaLightMode
    // (stateVocabulary.h), value for value; State_AreaLightVocabularyMirror compares the two.
    enum class AreaLightMode : uint8_t { Unset, Full, Fast, Off };

    // What the hider knows about a light: whether it is bound, and its controls at the light and scene levels
    struct AreaLightInfo {
            bool bound = false;                          // the light was created by AreaLightSource
            int lightSamples = 0;                        // light level sample count (0: unset)
            AreaLightMode lightMode = AreaLightMode::Unset;
            int sceneSamples = 0;                        // Option level (0: unset in the header; the hider always fills it from the registry)
            AreaLightMode sceneMode = AreaLightMode::Full;
            double totalArea = 0;                        // the emitters' total area; 0: no emitting geometry (the light contributes nothing)
    };

    class IAreaLightSampler {
        public:
            virtual ~IAreaLightSampler() = default;

            // Is `light` bound, and its light and scene controls. Resolves the light's emitter set on first use.
            // Thread-safety: per render thread, its own context. Re-enters Shading: never.
            virtual AreaLightInfo areaLight(CShadingContext &context, const CShaderGroupRefTarget *light) = 0;

            // For m points: draw a point on the light's emitters from (u1[i], u2[i]) in [0, 1), evaluate the light-surface
            // globals at time[i] (camera space, the emitter's own orientation, normals and st, no displacement) into
            // out[VARIABLE_P], [VARIABLE_N], [VARIABLE_NG], [VARIABLE_DPDU], [VARIABLE_DPDV], [VARIABLE_U], [VARIABLE_V],
            // [VARIABLE_S], [VARIABLE_T] (each buffer holds m points, 3 floats for the vectors and 1 for u, v, s, t; the other
            // entries of `out` are not touched) and write pdfA[i], the density by area of that point (0: no point; the caller
            // then contributes nothing for it). The draws are the caller's: a pure function of (u1, u2), so the same inputs give
            // the same points on any thread.
            // Thread-safety: per render thread, its own context. Re-enters Shading: never.
            virtual void sampleEmitters(CShadingContext &context, const CShaderGroupRefTarget *light, int m, const float *u1, const float *u2, const float *time,
                                        float **out, float *pdfA) = 0;
    };

} // namespace openrender::shading

#endif
