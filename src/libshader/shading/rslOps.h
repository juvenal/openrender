/**
 * Project: openRender
 *
 * File: rslOps.h
 *
 * Description:
 *   Batch (instruction-outer) RSL opcode implementations.
 *   Each function processes ALL n vertices for one RSL instruction, checking
 *   per-vertex activity tags.  This is the single source of truth for shader
 *   arithmetic used by both the LLVM JIT (.slo) and (optionally) the interpreter.
 *
 *   Strides (str_* parameters):
 *     0 = uniform  — the base pointer holds one value shared by all vertices
 *     1 = varying float  — value at vertex i is base[i]
 *     3 = varying vector — value at vertex i is base[3*i .. 3*i+2]
 *    16 = varying matrix — value at vertex i is base[16*i .. 16*i+15]
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef RI_RSLOPS_H
#define RI_RSLOPS_H

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Arithmetic — vector (stride-3)
 * ----------------------------------------------------------------------- */
void op_addvv(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_subvv(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_mulvv(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_divvv(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_negv (float* dst, int sd, const float* a, int sa,                           int n, const int* tags);
void op_addvf(float* dst, int sd, const float* v, int sv, const float* f, int sf, int n, const int* tags);
void op_subvf(float* dst, int sd, const float* v, int sv, const float* f, int sf, int n, const int* tags);
void op_mulvf(float* dst, int sd, const float* v, int sv, const float* f, int sf, int n, const int* tags);
void op_divvf(float* dst, int sd, const float* v, int sv, const float* f, int sf, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Arithmetic — float (stride-1)
 * ----------------------------------------------------------------------- */
void op_addff(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_subff(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_mulff(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_divff(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_negf (float* dst, int sd, const float* a, int sa,                           int n, const int* tags);

/* -----------------------------------------------------------------------
 * Float comparisons → 0.0 or 1.0
 * ----------------------------------------------------------------------- */
void op_flt(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_fle(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_fgt(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_fge(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_feq(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_fne(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Math — scalar float
 * ----------------------------------------------------------------------- */
void op_sqrt       (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_inversesqrt(float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_abs        (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_sign       (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_floor      (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_ceil       (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_exp        (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_log        (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_sin        (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_cos        (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_tan        (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_asin       (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_acos       (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_atan       (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_atan2      (float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_pow        (float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_mod        (float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_clampf     (float* dst, int sd, const float* v, int sv, const float* lo, int sl, const float* hi, int sh, int n, const int* tags);
void op_mixf       (float* dst, int sd, const float* a, int sa, const float* b, int sb, const float* t, int st, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Math — vector
 * ----------------------------------------------------------------------- */
void op_normalize(float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_length   (float* dst, int sd, const float* a, int sa, int n, const int* tags);
void op_dot      (float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_cross    (float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_clampv   (float* dst, int sd, const float* v, int sv, const float* lo, int sl, const float* hi, int sh, int n, const int* tags);
void op_mixv     (float* dst, int sd, const float* a, int sa, const float* b, int sb, const float* t, int st, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Component access
 * ----------------------------------------------------------------------- */
void op_xcomp   (float* dst, int sd, const float* v, int sv, int n, const int* tags);
void op_ycomp   (float* dst, int sd, const float* v, int sv, int n, const int* tags);
void op_zcomp   (float* dst, int sd, const float* v, int sv, int n, const int* tags);
void op_setxcomp(float* v, int sv, const float* f, int sf, int n, const int* tags);
void op_setycomp(float* v, int sv, const float* f, int sf, int n, const int* tags);
void op_setzcomp(float* v, int sv, const float* f, int sf, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Construction
 * ----------------------------------------------------------------------- */
/* vfromf dst src — broadcast float to 3-component vector */
void op_vfromf  (float* dst, int sd, const float* f,  int sf,                                         int n, const int* tags);
/* vfromvff dst vec f1 f2 — vec with components replaced */
void op_vfromvff(float* dst, int sd, const float* v,  int sv, const float* f1, int s1, const float* f2, int s2, int n, const int* tags);
/* vfromfff dst f0 f1 f2 — assemble vector from three floats */
void op_vfromfff(float* dst, int sd, const float* f0, int s0, const float* f1, int s1, const float* f2, int s2, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Move / copy
 * ----------------------------------------------------------------------- */
void op_moveff(float* dst, int sd, const float* src, int ss, int n, const int* tags);
void op_movevv(float* dst, int sd, const float* src, int ss, int n, const int* tags);
/* Uniform-to-uniform copies (no loop, no tags) */
void op_vufloat (float* dst, float val);               /* dst[0] = val */
void op_vuvector(float* dst, const float* src);        /* dst[0..2] = src[0..2] */

/* -----------------------------------------------------------------------
 * Geometry
 * ----------------------------------------------------------------------- */
void rsl_smoothstep(float* dst, int sd,
                    const float* edge0, int s0,
                    const float* edge1, int s1,
                    const float* x, int sx,
                    int n, const int* tags);

void op_faceforward(float* dst, int sd,
                    const float* n_in, int sn,
                    const float* i_in, int si,
                    const float* ng,   int sng,
                    int n, const int* tags);

/* -----------------------------------------------------------------------
 * Conditional tag management
 *   These implement IF2EXPR_PRE / ELSEIFEXPR_PRE / ENDIFEXPR_PRE exactly,
 *   but without the interpreter's numActive jmp() optimisation.
 * ----------------------------------------------------------------------- */
void op_if_update  (const float* cond, int scond, int* tags, int n, int* numActive, int* numPassive);
void op_else_update(                              int* tags, int n, int* numActive, int* numPassive);
void op_endif_update(                             int* tags, int n, int* numActive, int* numPassive);

/* -----------------------------------------------------------------------
 * Lighting (batch, instruction-outer — same semantics as interpreter)
 * ----------------------------------------------------------------------- */
/* Calls ctx->callAmbient(result) — same batch function the interpreter uses */
void op_ambient_batch(float* result, int n, const int* tags);
/* Calls ctx->callDiffuse(result, Nf) — same batch function the interpreter uses */
void op_diffuse_batch(float* result, int sr, const float* Nf, int sn, int n, const int* tags);
/* Calls ctx->callSpecular(result, Nf, V, roughness) — Phong specular highlight */
/* roughness is always a scalar (uniform) — passed as float* and read as roughness[0] */
void op_specular_batch(float* result, const float* Nf, const float* V,
                       const float* roughness, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Shader attribute query (used inside illuminance loops)
 * ----------------------------------------------------------------------- */
void op_lightsource_f(float* result, int sr, const char* attrName, float* outParam, int so, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Coordinate-space transforms
 * ----------------------------------------------------------------------- */
void op_pfrom    (float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags);
void op_ntransform(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags);
void op_vtransform(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags);

#ifdef __cplusplus
}
#endif

#endif /* RI_RSLOPS_H */
