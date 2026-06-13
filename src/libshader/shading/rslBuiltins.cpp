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
#include "noise.h"
#include "shading.h"
#include "texture.h"
#include "shader.h"
#include "memory.h"
#include <cstring>
#include <cmath>

// --- Noise Helpers ---

void rsl_noise_f_f(float* out, const float* in, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = noiseFloat(in[i]);
        }
    }
}

void rsl_noise_f_ff(float* out, const float* in1, const float* in2, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = noiseFloat(in1[i], in2[i]);
        }
    }
}

void rsl_noise_f_p(float* out, const float* in, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = noiseFloat(in + i * 3);
        }
    }
}

void rsl_noise_f_pf(float* out, const float* in1, const float* in2, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = noiseFloat(in1 + i * 3, in2[i]);
        }
    }
}

void rsl_noise_v_f(float* out, const float* in, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            noiseVector(out + i * 3, in[i]);
        }
    }
}

void rsl_noise_v_ff(float* out, const float* in1, const float* in2, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            noiseVector(out + i * 3, in1[i], in2[i]);
        }
    }
}

void rsl_noise_v_p(float* out, const float* in, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            noiseVector(out + i * 3, in + i * 3);
        }
    }
}

void rsl_noise_v_pf(float* out, const float* in1, const float* in2, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            noiseVector(out + i * 3, in1 + i * 3, in2[i]);
        }
    }
}

// --- Cell Noise Helpers ---

void rsl_cellnoise_f_f(float* out, const float* in, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = cellNoiseFloat(in[i]);
        }
    }
}

void rsl_cellnoise_f_ff(float* out, const float* in1, const float* in2, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = cellNoiseFloat(in1[i], in2[i]);
        }
    }
}

void rsl_cellnoise_f_p(float* out, const float* in, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = cellNoiseFloat(in + i * 3);
        }
    }
}

void rsl_cellnoise_f_pf(float* out, const float* in1, const float* in2, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            out[i] = cellNoiseFloat(in1 + i * 3, in2[i]);
        }
    }
}

void rsl_cellnoise_v_f(float* out, const float* in, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            cellNoiseVector(out + i * 3, in[i]);
        }
    }
}

void rsl_cellnoise_v_ff(float* out, const float* in1, const float* in2, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            cellNoiseVector(out + i * 3, in1[i], in2[i]);
        }
    }
}

void rsl_cellnoise_v_p(float* out, const float* in, int numVertices, const int* tags) {
    for (int i = 0; i < numVertices; ++i) {
        if (tags[i] == 0) {
            cellNoiseVector(out + i * 3, in + i * 3);
        }
    }
}

void rsl_cellnoise_v_pf(float* out, const float* in1, const float* in2, int numVertices, const int* tags) {
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

float rsl_noise_f_p_scalar(const float* in) {
    return noiseFloat(in);
}

void rsl_noise_v_f_scalar(float* out, float in) {
    noiseVector(out, in);
}

void rsl_noise_v_p_scalar(float* out, const float* in) {
    noiseVector(out, in);
}

void rsl_texture_c_scalar(float* out, const char* name, float s, float t) {
    CShadingContext *ctx = libshader::activeContext();
    CRendererServices *svc = ctx ? ctx->getServices() : nullptr;
    CTexture *tex = svc ? svc->getTexture(name) : nullptr;
    if (tex) {
        // CTexture::lookup takes a context for parameter list access (blur, etc.)
        // For simple lookup we might need a context anyway.
        // For now, let's use the active one.
        tex->lookup(out, s, t, libshader::activeContext());
    } else {
        out[0] = out[1] = out[2] = 0.0f;
    }
}

float rsl_attribute_scalar(const char* name, float* out) {
    if (libshader::activeContext())
        return libshader::activeContext()->queryAttribute(out, name);
    return 0.0f;
}

float rsl_option_scalar(const char* name, float* out) {
    if (libshader::activeContext())
        return libshader::activeContext()->queryOption(out, name);
    return 0.0f;
}

float rsl_rendererinfo_scalar(const char* name, float* out) {
    if (libshader::activeContext())
        return libshader::activeContext()->queryRendererInfo(out, name);
    return 0.0f;
}

float rsl_trace_scalar(const float* P, const float* R) {
    (void)P; (void)R;
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0.0f;
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
    if (ctx) ctx->callAmbient(result);
}

void rsl_diffuse(float *result, const float *Nf) {
    CShadingContext *ctx = libshader::activeContext();
    if (ctx) ctx->callDiffuse(result, Nf);
}

void rsl_ambient_1v(float *result_1v, int vtx) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) { result_1v[0] = result_1v[1] = result_1v[2] = 0.0f; return; }
    ctx->prepareAmbient();
    CShadingState *ss = ctx->currentShadingState;
    result_1v[0] = result_1v[1] = result_1v[2] = 0.0f;
    if (ss->alights && ss->alights->savedState && ss->alights->savedState[1]) {
        const float *Cl = ss->alights->savedState[1];
        result_1v[0] = Cl[3*vtx];
        result_1v[1] = Cl[3*vtx+1];
        result_1v[2] = Cl[3*vtx+2];
    }
}

void rsl_diffuse_1v(float *result_1v, const float *Nf_1v, int vtx) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) { result_1v[0] = result_1v[1] = result_1v[2] = 0.0f; return; }
    ctx->prepareDiffuse();
    CShadingState *ss = ctx->currentShadingState;
    result_1v[0] = result_1v[1] = result_1v[2] = 0.0f;
    for (CShadedLight *light = ss->lights; light; light = light->next) {
        const float *L  = light->savedState[0];
        const float *Cl = light->savedState[1];
        if (!L || !Cl) continue;
        float lx = L[3*vtx], ly = L[3*vtx+1], lz = L[3*vtx+2];
        float lm = sqrtf(lx*lx + ly*ly + lz*lz);
        if (lm > 1e-8f) { lx /= lm; ly /= lm; lz /= lm; }
        float coeff = Nf_1v[0]*lx + Nf_1v[1]*ly + Nf_1v[2]*lz;
        if (coeff > 0.0f) {
            result_1v[0] += coeff * Cl[3*vtx];
            result_1v[1] += coeff * Cl[3*vtx+1];
            result_1v[2] += coeff * Cl[3*vtx+2];
        }
    }
}

void rsl_illuminance_setup(float* P, float* N, float angle, int numVertices, int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    ctx->setupIlluminance(P, N, angle, numVertices, tags);
}

int rsl_illuminance_next_jit(float** L_ptr, float** Cl_ptr, int numVertices, int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    CShadingState *ss = ctx->currentShadingState;
    return rsl_illuminance_next(L_ptr, Cl_ptr, numVertices, tags, &ss->numActive, &ss->numPassive);
}

void rsl_illuminance_post_jit(int numVertices, int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    CShadingState *ss = ctx->currentShadingState;
    rsl_illuminance_post(numVertices, tags, &ss->numActive, &ss->numPassive);
}

int rsl_illuminance_next_batch(int numVertices, int* tags) {
    // Instruction-outer variant: advance to next light and copy its L/Cl directly
    // into varying[VARIABLE_L] and varying[VARIABLE_CL] for all active vertices,
    // mirroring the interpreter's ILLUMINATION2EXPR_PRE copyback loop.
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    CShadingState *ss = ctx->currentShadingState;

    // Advance currentLight (exit previous conditional if needed)
    if (ss->currentLight == nullptr) {
        ss->currentLight = ss->lights;
    } else {
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
                    if (tags[i] == 0) { ss->numActive++; ss->numPassive--; }
                }
            }
        }
        ss->currentLight = ss->currentLight->next;
    }
    if (ss->currentLight == nullptr) return 0;

    // Enter per-vertex conditional for the new light
    if (ss->currentLight->lightTags) {
        const int *lt = ss->currentLight->lightTags;
        for (int i = 0; i < numVertices; i++) {
            int wasActive = (tags[i] == 0);
            tags[i] += lt[i];
            if (wasActive && tags[i] != 0) { ss->numActive--; ss->numPassive++; }
        }
    }

    // Copy L and Cl from light's saved state into varying[VARIABLE_L] / varying[VARIABLE_CL]
    float *L  = ss->varying[VARIABLE_L];
    float *Cl = ss->varying[VARIABLE_CL];
    const float *Lsave  = ss->currentLight->savedState[0];
    const float *Clsave = ss->currentLight->savedState[1];
    if (Lsave && Clsave) {
        for (int i = 0; i < numVertices; i++) {
            if (tags[i] == 0) {
                L[3*i]   = Lsave[3*i];   L[3*i+1] = Lsave[3*i+1];   L[3*i+2] = Lsave[3*i+2];
                Cl[3*i]  = Clsave[3*i];  Cl[3*i+1] = Clsave[3*i+1]; Cl[3*i+2] = Clsave[3*i+2];
            }
        }
    }
    return 1;
}

// --- Lighting Integration ---

void rsl_run_lights(const float* P, const float* N, const float* T, int numVertices, int* tags, int* numActive, int* numPassive, int inShadow, float** varying, void* cInstance) {
    (void)P; (void)N; (void)T; (void)numVertices; (void)tags; (void)numActive; (void)numPassive; (void)inShadow; (void)varying; (void)cInstance;
    if (libshader::activeContext()) {
        libshader::activeContext()->runLights(P, N, T, numVertices, tags, *numActive, *numPassive, inShadow, varying, (CShaderInstance*)cInstance);
    }
}

void rsl_run_category_lights(const float* P, const float* N, const float* T, const char* category, int numVertices, int* tags, int* numActive, int* numPassive, int inShadow, float** varying, void* cInstance) {
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
        ctx2->runCategoryLights(P, N, T, numVertices, tags, *numActive, *numPassive, runCat, inShadow, varying, (CShaderInstance*)cInstance);
    }
}

int rsl_illuminance_next(float** L_ptr, float** Cl_ptr, int numVertices, int* tags, int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    CShadingState *ss = ctx->currentShadingState;

    if (ss->currentLight == nullptr) {
        ss->currentLight = ss->lights;
    } else {
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

    if (ss->currentLight == nullptr) return 0;

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

void rsl_illuminance_post(int numVertices, int* tags, int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    CShadingState *ss = ctx->currentShadingState;
    if (ss->currentLight == nullptr) return;

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

int rsl_illuminate_begin_1(const float* Pl) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    if (ctx->getServices() && ctx->getServices()->hasIlluminationHook()) {
        ctx->callIlluminateBegin(Pl, nullptr, nullptr);
        return 1;
    }
    CShadingState *ss = ctx->currentShadingState;
    int n = ss->numVertices;
    int *tags = ss->tags;
    float *L = ss->varying[VARIABLE_L];
    const float *Ps = ss->varying[VARIABLE_PS];
    const float *Ns = ss->Ns;
    const float *ct = ss->costheta;
    // Pl is the UNIFORM light position (same for all vertices). Read once.
    float pl_x = Pl[0], pl_y = Pl[1], pl_z = Pl[2];
    for (int i = 0; i < n; ++i) {
        if (tags[i]) { tags[i]++; continue; }
        float lx = Ps[3*i+0] - pl_x;
        float ly = Ps[3*i+1] - pl_y;
        float lz = Ps[3*i+2] - pl_z;
        L[3*i+0] = lx; L[3*i+1] = ly; L[3*i+2] = lz;
        float lm = sqrtf(lx*lx + ly*ly + lz*lz);
        float ctval = (ct != nullptr) ? ct[i] : 0.0f;
        if (Ns[3*i+0]*lx + Ns[3*i+1]*ly + Ns[3*i+2]*lz > -ctval*lm) {
            tags[i]++;
            ss->numActive--;
            ss->numPassive++;
        }
    }
    return ss->numActive;
}

int rsl_illuminate_begin_3(const float* Pl, const float* Nf, const float* theta) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    if (ctx->getServices() && ctx->getServices()->hasIlluminationHook()) {
        ctx->callIlluminateBegin(Pl, Nf, theta);
        return 1;
    }
    CShadingState *ss = ctx->currentShadingState;
    int n = ss->numVertices;
    int *tags = ss->tags;
    float *L = ss->varying[VARIABLE_L];
    const float *Ps = ss->varying[VARIABLE_PS];
    const float *Ns = ss->Ns;
    const float *ct = ss->costheta;
    // Pl, Nf, theta are UNIFORM parameters (same for all vertices in a cone light).
    float pl_x = Pl[0], pl_y = Pl[1], pl_z = Pl[2];
    float nf_x = Nf[0], nf_y = Nf[1], nf_z = Nf[2];
    float costh = (theta != nullptr) ? cosf(theta[0]) : 0.0f;
    for (int i = 0; i < n; ++i) {
        if (tags[i]) { tags[i]++; continue; }
        float lx = Ps[3*i+0] - pl_x;
        float ly = Ps[3*i+1] - pl_y;
        float lz = Ps[3*i+2] - pl_z;
        L[3*i+0] = lx; L[3*i+1] = ly; L[3*i+2] = lz;
        float lm = sqrtf(lx*lx + ly*ly + lz*lz);
        float dotNfL = nf_x*lx + nf_y*ly + nf_z*lz;
        float dotNsL = Ns[3*i+0]*lx + Ns[3*i+1]*ly + Ns[3*i+2]*lz;
        float ctval  = (ct != nullptr) ? ct[i] : 0.0f;
        if (dotNfL < costh*lm || dotNsL > -ctval*lm) {
            tags[i]++;
            ss->numActive--;
            ss->numPassive++;
        }
    }
    return ss->numActive;
}

void rsl_illuminate_end(void) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    if (ctx->getServices() && ctx->getServices()->hasIlluminationHook()) {
        ctx->callIlluminateEnd();
        return;
    }
    CShadingState *ss = ctx->currentShadingState;
    // Do NOT early-return on numActive==0: tags must always be restored to avoid
    // permanent vertex deactivation if illuminate_begin deactivated too many vertices.
    int n = ss->numVertices;
    int *tags = ss->tags;
    const float *L = ss->varying[VARIABLE_L];
    auto *cInst = static_cast<CProgrammableShaderInstance*>(ss->currentShaderInstance);
    const int numGlobals = cInst->parent->numGlobals;
    CShadedLight *cLight = nullptr;
    float *Lsave = nullptr;
    if (ss->numActive > 0) {
        if (ss->freeLights) {
            cLight = ss->freeLights;
            ss->freeLights = cLight->next;
            float **s = (float**)ralloc((2 + numGlobals) * sizeof(float*), ctx->threadMemory);
            s[0] = cLight->savedState[0];
            s[1] = cLight->savedState[1];
            cLight->savedState = s;
        } else {
            cLight = (CShadedLight*)ralloc(sizeof(CShadedLight), ctx->threadMemory);
            cLight->lightTags  = (int*)ralloc(sizeof(int) * n, ctx->threadMemory);
            cLight->savedState = (float**)ralloc((2 + numGlobals) * sizeof(float*), ctx->threadMemory);
            cLight->savedState[0] = (float*)ralloc(3 * sizeof(float) * n, ctx->threadMemory);
            cLight->savedState[1] = (float*)ralloc(3 * sizeof(float) * n, ctx->threadMemory);
            cLight->instance = cInst;
        }
        cLight->next = ss->lights;
        ss->lights = cLight;
        memcpy(cLight->lightTags,     tags,                    sizeof(int) * n);
        memcpy(cLight->savedState[1], ss->varying[VARIABLE_CL], 3 * sizeof(float) * n);
        Lsave = cLight->savedState[0];
        if (numGlobals > 0) {
            int g = 0;
            for (CVariable *v = cInst->parameters; v; v = v->next) {
                if (v->storage == STORAGE_GLOBAL || v->storage == STORAGE_MUTABLEPARAMETER) {
                    bool uniform = (v->container == CONTAINER_UNIFORM ||
                                    v->container == CONTAINER_CONSTANT);
                    int bytes = v->numFloats * (int)sizeof(float) * (uniform ? 1 : n);
                    cLight->savedState[2 + g] = (float*)ralloc(bytes, ctx->threadMemory);
                    float *src = (v->storage == STORAGE_GLOBAL)
                        ? ss->varying[v->entry]
                        : (float*)ss->locals[ACCESSOR_LIGHTSOURCE][v->entry];
                    memcpy(cLight->savedState[2 + g], src, bytes);
                    g++;
                }
            }
        }
    }
    // Always restore tags (even if no active vertices were found above).
    for (int i = 0; i < n; ++i) {
        if (tags[i]) {
            tags[i]--;
            if (tags[i] == 0) { ss->numActive++; ss->numPassive--; }
        } else if (Lsave) {
            Lsave[3*i+0] = -L[3*i+0];
            Lsave[3*i+1] = -L[3*i+1];
            Lsave[3*i+2] = -L[3*i+2];
        }
        if (Lsave) Lsave += 3;
    }
}

int rsl_solar_begin_1(void) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    if (ctx->getServices() && ctx->getServices()->hasIlluminationHook()) {
        ctx->callSolarBegin(nullptr, nullptr);
        return 1;
    }
    CShadingState *ss = ctx->currentShadingState;
    int n = ss->numVertices;
    int *tags = ss->tags;
    float *L = ss->varying[VARIABLE_L];
    const float *Ps = ss->varying[VARIABLE_PS];
    for (int i = 0; i < n; ++i) {
        if (tags[i]) { tags[i]++; continue; }
        L[3*i+0] = Ps[3*i+0];
        L[3*i+1] = Ps[3*i+1];
        L[3*i+2] = Ps[3*i+2];
    }
    return ss->numActive;
}

int rsl_solar_begin_2(const float* Nf, const float* theta) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    if (ctx->getServices() && ctx->getServices()->hasIlluminationHook()) {
        ctx->callSolarBegin(Nf, theta);
        return 1;
    }
    CShadingState *ss = ctx->currentShadingState;
    int n = ss->numVertices;
    int *tags = ss->tags;
    float *L = ss->varying[VARIABLE_L];
    const float *Ns = ss->Ns;
    const float *ct = ss->costheta;
    CRendererServices *svc2 = ctx->getServices();
    const float *wbmax = svc2 ? svc2->worldBmax() : nullptr;
    const float *wbmin = svc2 ? svc2->worldBmin() : nullptr;
    float rx = (wbmax && wbmin) ? (wbmax[0] - wbmin[0]) : 0.0f;
    float ry = (wbmax && wbmin) ? (wbmax[1] - wbmin[1]) : 0.0f;
    float rz = (wbmax && wbmin) ? (wbmax[2] - wbmin[2]) : 0.0f;
    float wr = rx*rx + ry*ry + rz*rz;
    // Nf is a UNIFORM direction (same for all vertices in a directional/solar light).
    // Read it once as a constant rather than indexing per-vertex (Nf[3*i] would read
    // out of bounds past the 3-float uniform allocation for i >= 1).
    float nf_x = Nf[0], nf_y = Nf[1], nf_z = Nf[2];
    float lx = nf_x * wr, ly = nf_y * wr, lz = nf_z * wr;
    float lm = sqrtf(lx*lx + ly*ly + lz*lz);
    for (int i = 0; i < n; ++i) {
        if (tags[i]) { tags[i]++; continue; }
        L[3*i+0] = lx; L[3*i+1] = ly; L[3*i+2] = lz;
        float ctval = (ct != nullptr) ? ct[i] : 0.0f;
        if (Ns[3*i+0]*lx + Ns[3*i+1]*ly + Ns[3*i+2]*lz > -ctval*lm) {
            tags[i]++;
            ss->numActive--;
            ss->numPassive++;
        }
    }
    return ss->numActive;
}

void rsl_solar_end(void) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    if (ctx->getServices() && ctx->getServices()->hasIlluminationHook()) {
        ctx->callSolarEnd();
        return;
    }
    CShadingState *ss = ctx->currentShadingState;
    // Do NOT early-return on numActive==0: tags must always be restored.
    int n = ss->numVertices;
    int *tags = ss->tags;
    const float *L = ss->varying[VARIABLE_L];
    auto *cInst = static_cast<CProgrammableShaderInstance*>(ss->currentShaderInstance);
    const int numGlobals = cInst->parent->numGlobals;

    // Determine if this is an ambient light (no SHADERFLAGS_NONAMBIENT).
    // Ambient lights must NOT be linked into ss->lights (used by callDiffuse).
    auto *lightInst = static_cast<CProgrammableShaderInstance*>(ss->currentLightInstance);
    bool isAmbient = lightInst && !(lightInst->flags & SHADERFLAGS_NONAMBIENT);

    CShadedLight *cLight = nullptr;
    float *Lsave = nullptr;
    if (ss->numActive > 0) {
        if (isAmbient) {
            // Ambient: accumulate Cl into ss->alights (the list callAmbient reads from).
            // The callAmbient() function already handles this accumulation via
            // varying[VARIABLE_CL], so we skip the CShadedLight machinery here.
        } else {
            // Non-ambient directional/spot light: save to ss->lights.
            if (ss->freeLights) {
                cLight = ss->freeLights;
                ss->freeLights = cLight->next;
                float **s = (float**)ralloc((2 + numGlobals) * sizeof(float*), ctx->threadMemory);
                s[0] = cLight->savedState[0];
                s[1] = cLight->savedState[1];
                cLight->savedState = s;
            } else {
                cLight = (CShadedLight*)ralloc(sizeof(CShadedLight), ctx->threadMemory);
                cLight->lightTags  = (int*)ralloc(sizeof(int) * n, ctx->threadMemory);
                cLight->savedState = (float**)ralloc((2 + numGlobals) * sizeof(float*), ctx->threadMemory);
                cLight->savedState[0] = (float*)ralloc(3 * sizeof(float) * n, ctx->threadMemory);
                cLight->savedState[1] = (float*)ralloc(3 * sizeof(float) * n, ctx->threadMemory);
                cLight->instance = cInst;
            }
            cLight->next = ss->lights;
            ss->lights = cLight;
            memcpy(cLight->lightTags,     tags,                    sizeof(int) * n);
            memcpy(cLight->savedState[1], ss->varying[VARIABLE_CL], 3 * sizeof(float) * n);
            Lsave = cLight->savedState[0];
            if (numGlobals > 0) {
                int g = 0;
                for (CVariable *v = cInst->parameters; v; v = v->next) {
                    if (v->storage == STORAGE_GLOBAL || v->storage == STORAGE_MUTABLEPARAMETER) {
                        bool uniform = (v->container == CONTAINER_UNIFORM ||
                                        v->container == CONTAINER_CONSTANT);
                        int bytes = v->numFloats * (int)sizeof(float) * (uniform ? 1 : n);
                        cLight->savedState[2 + g] = (float*)ralloc(bytes, ctx->threadMemory);
                        float *src = (v->storage == STORAGE_GLOBAL)
                            ? ss->varying[v->entry]
                            : (float*)ss->locals[ACCESSOR_LIGHTSOURCE][v->entry];
                        memcpy(cLight->savedState[2 + g], src, bytes);
                        g++;
                    }
                }
            }
        }
    }
    // Always restore tags.
    for (int i = 0; i < n; ++i) {
        if (tags[i]) {
            tags[i]--;
            if (tags[i] == 0) { ss->numActive++; ss->numPassive--; }
        } else if (Lsave) {
            float mag = sqrtf(L[3*i+0]*L[3*i+0] + L[3*i+1]*L[3*i+1] + L[3*i+2]*L[3*i+2]);
            if (mag > 0.0f) mag = 1.0f / mag;
            Lsave[0] = -L[3*i+0] * mag;
            Lsave[1] = -L[3*i+1] * mag;
            Lsave[2] = -L[3*i+2] * mag;
        }
        if (Lsave) Lsave += 3;
    }
}
