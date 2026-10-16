/**
 * Project: openRender
 *
 * File: rslBuiltins.cpp
 *
 * Description:
 *   Vectorized C-linkage built-in functions for RSL shaders.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "rslBuiltins.h"
#include "activeContext.h"
#include "common/memory.h"
#include "noise.h"
#include "shader.h"
#include "shading.h"
#include "texture.h"
#include <cmath>
#include <cstring>

// --- Noise Helpers ---

void rsl_noise_f_f(float *out, const float *in, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = noiseFloat(in[i]);
        }
    }
}

void rsl_noise_f_ff(float *out, const float *in1, const float *in2, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = noiseFloat(in1[i], in2[i]);
        }
    }
}

void rsl_noise_f_p(float *out, const float *in, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = noiseFloat(in + i * 3);
        }
    }
}

void rsl_noise_f_pf(float *out, const float *in1, const float *in2, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = noiseFloat(in1 + i * 3, in2[i]);
        }
    }
}

void rsl_noise_v_f(float *out, const float *in, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            noiseVector(out + i * 3, in[i]);
        }
    }
}

void rsl_noise_v_ff(float *out, const float *in1, const float *in2, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            noiseVector(out + i * 3, in1[i], in2[i]);
        }
    }
}

void rsl_noise_v_p(float *out, const float *in, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            noiseVector(out + i * 3, in + i * 3);
        }
    }
}

void rsl_noise_v_pf(float *out, const float *in1, const float *in2, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            noiseVector(out + i * 3, in1 + i * 3, in2[i]);
        }
    }
}

// --- Cell Noise Helpers ---

void rsl_cellnoise_f_f(float *out, const float *in, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = cellNoiseFloat(in[i]);
        }
    }
}

void rsl_cellnoise_f_ff(float *out, const float *in1, const float *in2, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = cellNoiseFloat(in1[i], in2[i]);
        }
    }
}

void rsl_cellnoise_f_p(float *out, const float *in, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = cellNoiseFloat(in + i * 3);
        }
    }
}

void rsl_cellnoise_f_pf(float *out, const float *in1, const float *in2, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = cellNoiseFloat(in1 + i * 3, in2[i]);
        }
    }
}

void rsl_cellnoise_v_f(float *out, const float *in, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            cellNoiseVector(out + i * 3, in[i]);
        }
    }
}

void rsl_cellnoise_v_ff(float *out, const float *in1, const float *in2, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            cellNoiseVector(out + i * 3, in1[i], in2[i]);
        }
    }
}

void rsl_cellnoise_v_p(float *out, const float *in, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            cellNoiseVector(out + i * 3, in + i * 3);
        }
    }
}

void rsl_cellnoise_v_pf(float *out, const float *in1, const float *in2, int numVertices, const int *tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            cellNoiseVector(out + i * 3, in1 + i * 3, in2[i]);
        }
    }
}

// --- Scalar Helpers for JIT ---

float rsl_noise_f_f_scalar(float in) {
    return noiseFloat(in);
}

float rsl_noise_f_p_scalar(const float *in) {
    return noiseFloat(in);
}

void rsl_noise_v_f_scalar(float *out, float in) {
    noiseVector(out, in);
}

void rsl_noise_v_p_scalar(float *out, const float *in) {
    noiseVector(out, in);
}

void rsl_texture_c_scalar(float *out, const char *name, float s, float t) {
    CShadingContext *ctx = libshader::activeContext();
    CRendererServices *svc = ctx ? ctx->getServices() : nullptr;
    CTexture *tex = svc ? svc->getTexture(name) : nullptr;
    if (tex) {
        // CTexture::lookup takes a context for parameter list access (blur, etc.)
        // For simple lookup we might need a context anyway.
        // For now, let's use the active one.
        tex->lookup(out, s, t, libshader::activeContext());
    }
    else {
        out[0] = out[1] = out[2] = 0.0f;
    }
}

float rsl_attribute_scalar(const char *name, float *out) {
    if (libshader::activeContext())
        return libshader::activeContext()->queryAttribute(out, name);
    return 0.0f;
}

float rsl_option_scalar(const char *name, float *out) {
    if (libshader::activeContext())
        return libshader::activeContext()->queryOption(out, name);
    return 0.0f;
}

float rsl_rendererinfo_scalar(const char *name, float *out) {
    if (libshader::activeContext())
        return libshader::activeContext()->queryRendererInfo(out, name);
    return 0.0f;
}

float rsl_trace_scalar(const float *P, const float *R) {
    (void)P;
    (void)R;
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx)
        return 0.0f;
    // P is origin, R is direction
    // In openRender, we usually use CRayBundle or similar.
    // For a simple scalar trace, we can use the context's trace(CRay*) method if it returns float.
    // Actually, trace usually returns the hit distance or light intensity.
    // Let's assume it matches the RSL trace() which returns light.
    // TODO: Finalize the CRay usage once CRay header is fully analyzed.
    return 0.0f;
}

// --- Lighting Built-ins (ambient / diffuse) ---

void rsl_ambient(float *result) {
    CShadingContext *ctx = libshader::activeContext();
    if (ctx)
        ctx->callAmbient(result);
}

void rsl_diffuse(float *result, const float *Nf) {
    CShadingContext *ctx = libshader::activeContext();
    if (ctx)
        ctx->callDiffuse(result, Nf);
}

void rsl_ambient_1v(float *result_1v, int vtx) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) {
        result_1v[0] = result_1v[1] = result_1v[2] = 0.0f;
        return;
    }
    ctx->prepareAmbient();
    CShadingState *ss = ctx->currentShadingState;
    result_1v[0] = result_1v[1] = result_1v[2] = 0.0f;
    if (ss->alights && ss->alights->savedState && ss->alights->savedState[1]) {
        const float *Cl = ss->alights->savedState[1];
        result_1v[0] = Cl[3 * vtx];
        result_1v[1] = Cl[3 * vtx + 1];
        result_1v[2] = Cl[3 * vtx + 2];
    }
}

void rsl_diffuse_1v(float *result_1v, const float *Nf_1v, int vtx) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) {
        result_1v[0] = result_1v[1] = result_1v[2] = 0.0f;
        return;
    }
    ctx->prepareDiffuse();
    CShadingState *ss = ctx->currentShadingState;
    result_1v[0] = result_1v[1] = result_1v[2] = 0.0f;
    for (CShadedLight *light = ss->lights; light; light = light->next) {
        const float *L = light->savedState[0];
        const float *Cl = light->savedState[1];
        if (!L || !Cl)
            continue;
        float lx = L[3 * vtx], ly = L[3 * vtx + 1], lz = L[3 * vtx + 2];
        float lm = sqrtf(lx * lx + ly * ly + lz * lz);
        if (lm > 1e-8f) {
            lx /= lm;
            ly /= lm;
            lz /= lm;
        }
        float coeff = Nf_1v[0] * lx + Nf_1v[1] * ly + Nf_1v[2] * lz;
        if (coeff > 0.0f) {
            result_1v[0] += coeff * Cl[3 * vtx];
            result_1v[1] += coeff * Cl[3 * vtx + 1];
            result_1v[2] += coeff * Cl[3 * vtx + 2];
        }
    }
}

void rsl_illuminance_setup(float *P, float *N, float angle, int numVertices, int *tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx)
        return;
    ctx->setupIlluminance(P, N, angle, numVertices, tags);
}

int rsl_illuminance_next_jit(float **L_ptr, float **Cl_ptr, int numVertices, int *tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx)
        return 0;
    CShadingState *ss = ctx->currentShadingState;
    return rsl_illuminance_next(L_ptr, Cl_ptr, numVertices, tags, &ss->numActive, &ss->numPassive);
}

void rsl_illuminance_post_jit(int numVertices, int *tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx)
        return;
    CShadingState *ss = ctx->currentShadingState;
    rsl_illuminance_post(numVertices, tags, &ss->numActive, &ss->numPassive);
}

int rsl_illuminance_next_batch(int numVertices, int *tags) {
    // Instruction-outer variant: advance to next light and copy its L/Cl directly
    // into varying[VARIABLE_L] and varying[VARIABLE_CL] for all active vertices,
    // mirroring the interpreter's ILLUMINATION2EXPR_PRE copyback loop.
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx)
        return 0;
    CShadingState *ss = ctx->currentShadingState;

    // Advance currentLight (exit previous conditional if needed)
    if (ss->currentLight == nullptr) {
        ss->currentLight = ss->lights;
    }
    else {
        // Exit the per-vertex conditional for the current light.
        // The entry path did:  tags[i] += lightTags[i]
        // The exit path must undo that exactly: tags[i] -= lightTags[i]
        // This only affects NON-illuminated vertices (lightTags[i] != 0).
        // Illuminated vertices had lightTags[i] == 0, so no change is needed.
        if (ss->currentLight->lightTags) {
            const int *lt = ss->currentLight->lightTags;
            for (int i = 0; i < numVertices; i++) {
                if (lt[i] != 0) {
                    tags[i] -= lt[i];
                    if (tags[i] == 0) {
                        ss->numActive++;
                        ss->numPassive--;
                    }
                }
            }
        }
        ss->currentLight = ss->currentLight->next;
    }
    if (ss->currentLight == nullptr)
        return 0;

    // Enter per-vertex conditional for the new light
    if (ss->currentLight->lightTags) {
        const int *lt = ss->currentLight->lightTags;
        for (int i = 0; i < numVertices; i++) {
            int wasActive = (tags[i] == 0);
            tags[i] += lt[i];
            if (wasActive && tags[i] != 0) {
                ss->numActive--;
                ss->numPassive++;
            }
        }
    }

    // Copy L and Cl from light's saved state into varying[VARIABLE_L] / varying[VARIABLE_CL]
    float *L = ss->varying[VARIABLE_L];
    float *Cl = ss->varying[VARIABLE_CL];
    const float *Lsave = ss->currentLight->savedState[0];
    const float *Clsave = ss->currentLight->savedState[1];
    if (Lsave && Clsave) {
        for (int i = 0; i < numVertices; i++) {
            if (tags[i] == 0) {
                L[3 * i] = Lsave[3 * i];
                L[3 * i + 1] = Lsave[3 * i + 1];
                L[3 * i + 2] = Lsave[3 * i + 2];
                Cl[3 * i] = Clsave[3 * i];
                Cl[3 * i + 1] = Clsave[3 * i + 1];
                Cl[3 * i + 2] = Clsave[3 * i + 2];
            }
        }
    }
    return 1;
}

// --- Lighting Integration ---

void rsl_run_lights(const float *P, const float *N, const float *T, int numVertices, int *tags, int *numActive, int *numPassive, int inShadow, float **varying, void *cInstance) {
    (void)P;
    (void)N;
    (void)T;
    (void)numVertices;
    (void)tags;
    (void)numActive;
    (void)numPassive;
    (void)inShadow;
    (void)varying;
    (void)cInstance;
    if (libshader::activeContext()) {
        libshader::activeContext()->iterateLights(P, N, T, numVertices, tags, *numActive, *numPassive, inShadow, varying, (CShaderInstance *)cInstance);
    }
}

void rsl_run_category_lights(const float *P, const float *N, const float *T, const char *category, int numVertices, int *tags, int *numActive, int *numPassive, int inShadow, float **varying, void *cInstance) {
    (void)category;
    CShadingContext *ctx2 = libshader::activeContext();
    if (ctx2) {
        int runCat = 0;
        if (category && category[0] != '\0') {
            CRendererServices *svc = ctx2->getServices();
            if (svc) {
                runCat = (category[0] == '-') ? -svc->getGlobalID(category + 1) : svc->getGlobalID(category);
            }
        }
        ctx2->iterateLights(P, N, T, numVertices, tags, *numActive, *numPassive, runCat, inShadow, varying, (CShaderInstance *)cInstance);
    }
}

int rsl_illuminance_next(float **L_ptr, float **Cl_ptr, int numVertices, int *tags, int *numActive, int *numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx)
        return 0;
    CShadingState *ss = ctx->currentShadingState;

    if (ss->currentLight == nullptr) {
        ss->currentLight = ss->lights;
    }
    else {
        // Exit current conditional for the previous light
        const int *lightTags = ss->currentLight->lightTags;
        for (int i = 0; i < numVertices; ++i) {
            if (lightTags[i] == 0) {
                if (tags[i] == 0) {
                    (*numActive)++;
                    (*numPassive)--;
                }
                tags[i]--;
            }
        }
        ss->currentLight = ss->currentLight->next;
    }

    if (ss->currentLight == nullptr)
        return 0;

    *L_ptr = ss->currentLight->savedState[0];
    *Cl_ptr = ss->currentLight->savedState[1];

    // Enter conditional for the new light
    const int *lightTags = ss->currentLight->lightTags;
    for (int i = 0; i < numVertices; ++i) {
        const int wasActive = (tags[i] == 0);
        tags[i] += lightTags[i];
        if (wasActive && tags[i] != 0) {
            (*numActive)--;
            (*numPassive)++;
        }
    }

    return 1;
}

void rsl_illuminance_post(int numVertices, int *tags, int *numActive, int *numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx)
        return;
    CShadingState *ss = ctx->currentShadingState;
    if (ss->currentLight == nullptr)
        return;

    // Exit conditional for the final light in the loop.
    // Undo the entry's: tags[i] += lightTags[i]
    // Only non-illuminated vertices (lightTags[i] != 0) need decrementing.
    const int *lightTags = ss->currentLight->lightTags;
    for (int i = 0; i < numVertices; ++i) {
        if (lightTags[i] != 0) {
            tags[i] -= lightTags[i];
            if (tags[i] == 0) {
                (*numActive)++;
                (*numPassive)--;
            }
        }
    }
    ss->currentLight = nullptr;
}
