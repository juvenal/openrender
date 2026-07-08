/**
 * Project: openRender
 *
 * File: rslBuiltins.h
 *
 * Description:
 *   Vectorized C-linkage built-in functions for RSL shaders.
 *   These functions are called by JIT-compiled .slo shaders.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef RI_RSLBUILTINS_H
#define RI_RSLBUILTINS_H

#include "common/global.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Noise Functions (Vectorized) ---

void rsl_noise_f_f(float* out, const float* in, int numVertices, const int* tags);
void rsl_noise_f_ff(float* out, const float* in1, const float* in2, int numVertices, const int* tags);
void rsl_noise_f_p(float* out, const float* in, int numVertices, const int* tags);
void rsl_noise_f_pf(float* out, const float* in1, const float* in2, int numVertices, const int* tags);

void rsl_noise_v_f(float* out, const float* in, int numVertices, const int* tags);
void rsl_noise_v_ff(float* out, const float* in1, const float* in2, int numVertices, const int* tags);
void rsl_noise_v_p(float* out, const float* in, int numVertices, const int* tags);
void rsl_noise_v_pf(float* out, const float* in1, const float* in2, int numVertices, const int* tags);

// --- Cell Noise Functions (Vectorized) ---

void rsl_cellnoise_f_f(float* out, const float* in, int numVertices, const int* tags);
void rsl_cellnoise_f_ff(float* out, const float* in1, const float* in2, int numVertices, const int* tags);
void rsl_cellnoise_f_p(float* out, const float* in, int numVertices, const int* tags);
void rsl_cellnoise_f_pf(float* out, const float* in1, const float* in2, int numVertices, const int* tags);

void rsl_cellnoise_v_f(float* out, const float* in, int numVertices, const int* tags);
void rsl_cellnoise_v_ff(float* out, const float* in1, const float* in2, int numVertices, const int* tags);
void rsl_cellnoise_v_p(float* out, const float* in, int numVertices, const int* tags);
void rsl_cellnoise_v_pf(float* out, const float* in1, const float* in2, int numVertices, const int* tags);

// --- Scalar Helpers for JIT (Per-vertex) ---

float rsl_noise_f_f_scalar(float in);
float rsl_noise_f_p_scalar(const float* in);
void  rsl_noise_v_f_scalar(float* out, float in);
void  rsl_noise_v_p_scalar(float* out, const float* in);

void  rsl_texture_c_scalar(float* out, const char* name, float s, float t);

float rsl_attribute_scalar(const char* name, float* out);
float rsl_option_scalar(const char* name, float* out);
float rsl_rendererinfo_scalar(const char* name, float* out);

float rsl_trace_scalar(const float* P, const float* R);

// --- Lighting (Surface Shaders) ---
void rsl_run_lights(const float* P, const float* N, const float* T, int numVertices, int* tags, int* numActive, int* numPassive, int inShadow, float** varying, void* cInstance);
void rsl_run_category_lights(const float* P, const float* N, const float* T, const char* category, int numVertices, int* tags, int* numActive, int* numPassive, int inShadow, float** varying, void* cInstance);
int  rsl_illuminance_next(float** L_ptr, float** Cl_ptr, int numVertices, int* tags, int* numActive, int* numPassive);
void rsl_illuminance_post(int numVertices, int* tags, int* numActive, int* numPassive);

// --- Lighting Built-ins (Surface Shaders) ---
// Called by JIT-compiled surface/atmosphere shaders for ambient/diffuse shading.
void rsl_ambient(float* result);
void rsl_diffuse(float* result, const float* Nf);

// Per-vertex variants called by the LLVM JIT from inside the vertex loop.
// These call prepareDiffuse/prepareAmbient (lazy, once per batch) then
// accumulate the result for a single vertex index.
void rsl_ambient_1v(float* result_1v, int vtx);
void rsl_diffuse_1v(float* result_1v, const float* Nf_1v, int vtx);

// JIT-callable illuminance helpers.
// rsl_illuminance_setup replaces rsl_run_lights for JIT shaders: converts a scalar
// half-angle to a per-vertex costheta array and runs all lights via the active context.
// rsl_illuminance_next_jit / rsl_illuminance_post_jit are 4/2-arg wrappers that obtain
// numActive and numPassive from the active shading context internally.
void rsl_illuminance_setup(float* P, float* N, float angle, int numVertices, int* tags);
int  rsl_illuminance_next_jit(float** L_ptr, float** Cl_ptr, int numVertices, int* tags);
void rsl_illuminance_post_jit(int numVertices, int* tags);

// Instruction-outer illuminance loop helper (new design).
// Advances to the next light and copies its L/Cl into varying[VARIABLE_L] and
// varying[VARIABLE_CL] for all active vertices — exactly as the interpreter's
// ILLUMINATION2EXPR_PRE does.  Returns 1 if a light was loaded, 0 if done.
// Called once per illuminance loop iteration from JIT-compiled instruction-outer code.
int rsl_illuminance_next_batch(int numVertices, int* tags);

// --- Lighting (Light Shaders) ---
// Returns numActive after updating L and tags (0 = skip body, non-zero = continue).
int  rsl_illuminate_begin_1(const float* Pl);
int  rsl_illuminate_begin_3(const float* Pl, const float* Nf, const float* theta);
void rsl_illuminate_end(void);
int  rsl_solar_begin_1(void);
int  rsl_solar_begin_2(const float* Nf, const float* theta);
void rsl_solar_end(void);

#ifdef __cplusplus
}
#endif

#endif // RI_RSLBUILTINS_H
