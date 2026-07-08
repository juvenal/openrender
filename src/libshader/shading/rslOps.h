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
 * for / while loop (JIT path) — mirrors FOR3EXPR_PRE / FOREND3EXPR_PRE
 * ----------------------------------------------------------------------- */
/* op_for_check: test condition for each active vertex; marks those with cond==0
 * as loop-passive (tags[i] = execCount).  Increments *execCount. */
void op_for_check(const float* cond, int sc,
                  int* execCount, int* tags, int n,
                  int* numActive, int* numPassive);
/* op_forend: restore tags to pre-loop state; mirrors FOREND3EXPR_PRE. */
void op_forend(const int* execCount, int* tags, int n,
               int* numActive, int* numPassive);
/* op_for_break: mark all currently-active vertices as loop-exited. */
void op_for_break(const int* execCount, int* tags, int n,
                  int* numActive, int* numPassive);

/* -----------------------------------------------------------------------
 * illuminance loop (surface shader construct, JIT path)
 * ----------------------------------------------------------------------- */
/* op_illuminance_begin: run all lights for (P,N,angle), enter first light's
 * conditional (load L/Cl, apply lightTags).  Returns 1 if iterating, 0 if done. */
int op_illuminance_begin(const float* P, int sp, const float* N, int sn,
                         const float* angle, int sa,
                         int* tags, int n, int* numActive, int* numPassive);
/* op_illuminance_next: exit current light, advance to next, enter it.
 * Returns 1 if more iterations needed, 0 if done. */
int op_illuminance_next(int* tags, int n, int* numActive, int* numPassive);

/* -----------------------------------------------------------------------
 * illuminate / endilluminate (light shader construct, JIT path)
 * ----------------------------------------------------------------------- */
/* op_illuminate_begin: mirror of ILLUMINATE1EXPR_PRE for LLVM .slo light shaders.
 * Computes L = Ps - from (with stride sf), gates vertices outside the cone. */
void op_illuminate_begin(const float* from, int sf,
                         int* tags, int n, int* numActive, int* numPassive);
/* op_illuminate3_begin: mirror of ILLUMINATE3EXPR_PRE — spotlight/cone gate.
 * Computes L = Ps - from; gates vertex if outside cone (dot(axis,L) < cos(angle)*|L|)
 * or if back-facing (dot(Ns,L) > -costheta*|L|). */
void op_illuminate3_begin(const float* from, int sf,
                          const float* axis, int sa,
                          const float* angle, int st,
                          int* tags, int n, int* numActive, int* numPassive);
/* op_illuminate_end: mirror of ILLUMINATEEND_PRE for LLVM .slo light shaders.
 * Saves (L,Cl) into CShadedLight, restores tags. */
void op_illuminate_end(int* tags, int n, int* numActive, int* numPassive);

/* op_solar_begin: mirror of SOLAR2EXPR_PRE for LLVM .slo directional light shaders.
 * Sets L = Nf * worldRadius, gates back-facing vertices. */
void op_solar_begin(const float* Nf, int sf, const float* thetaf, int st,
                    int* tags, int n, int* numActive, int* numPassive);
/* op_solar_end: mirror of SOLAREND_PRE — saves (-normalize(L), Cl), restores tags. */
void op_solar_end(int* tags, int n, int* numActive, int* numPassive);

/* -----------------------------------------------------------------------
 * Coordinate-space transforms
 * ----------------------------------------------------------------------- */
void op_pfrom      (float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags);
void op_ptransform (float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags);
void op_ntransform (float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags);
void op_vtransform (float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Layer G — additional math / comparison ops
 * ----------------------------------------------------------------------- */
/* max / logical */
void op_maxf(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_andf(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
void op_orf (float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags);
/* degrees → radians */
void op_radians(float* dst, int sd, const float* a, int sa, int n, const int* tags);
/* string equality */
void op_seql (float* dst, int sd, const char* const* a, const char* const* b, int n, const int* tags);
void op_sneql(float* dst, int sd, const char* const* a, const char* const* b, int n, const int* tags);
/* filterstep (simplified: no antialiasing) */
void op_filterstep(float* dst, int sd, const float* edge, int se, const float* x, int sx, int n, const int* tags);
/* reflect / fresnel */
void op_reflect(float* dst, int sd, const float* I, int si, const float* N, int sn, int n, const int* tags);
void op_fresnel(const float* I, int si, const float* N, int sn, const float* eta, int se,
                float* Kr, int skr, float* Kt, int skt,
                float* R, int sr, float* T, int st,
                int n, const int* tags);
/* noise */
void op_noise_ff(float* dst, int sd, const float* x, int sx, int n, const int* tags);
void op_noise_fp(float* dst, int sd, const float* p, int sp, int n, const int* tags);
void op_noise_vf(float* dst, int sd, const float* x, int sx, int n, const int* tags);
void op_noise_vp(float* dst, int sd, const float* p, int sp, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Layer G — context-dependent geometric built-ins (call via activeContext())
 * ----------------------------------------------------------------------- */
void op_Du_ff(float* dst, int sd, const float* src, int ss, int n, const int* tags);
void op_Dv_ff(float* dst, int sd, const float* src, int ss, int n, const int* tags);
void op_Du_vv(float* dst, int sd, const float* src, int ss, int n, const int* tags);
void op_Dv_vv(float* dst, int sd, const float* src, int ss, int n, const int* tags);
void op_area          (float* dst, int sd, const float* P,  int sp, int n, const int* tags);
void op_calculatenormal(float* dst, int sd, const float* P, int sp, int n, const int* tags);
void op_depth         (float* dst, int sd, const float* P,  int sp, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Layer G — texture / environment / shadow (call via activeContext())
 * The "name" is a C string; channel is 0=R 1=G 2=B 3=A.
 * ----------------------------------------------------------------------- */
void op_texture_f(float* dst, int sd, const char* const* namepp, const float* chan,
                  const float* s, int ss, const float* t, int st, int n, const int* tags);
void op_texture_c(float* dst, int sd, const char* const* namepp,
                  const float* s, int ss, const float* t, int st, int n, const int* tags);
void op_environment_f(float* dst, int sd, const char* const* namepp, const float* chan,
                      const float* D, int sD, int n, const int* tags);
void op_environment_c(float* dst, int sd, const char* const* namepp,
                      const float* D, int sD, int n, const int* tags);
void op_shadow_f(float* dst, int sd, const char* const* namepp,
                 const float* Ps, int sPs, int n, const int* tags);

/* -----------------------------------------------------------------------
 * Spline interpolation (Catmull-Rom, default basis, no basis-string variant)
 * knots[] is an array of numKnots float* pointers; all stride 3 for color,
 * stride 1 for float.
 * ----------------------------------------------------------------------- */
void op_spline_c(float* dst, int sd, const float* t, int st,
                 int numKnots, float** knots,
                 int n, const int* tags);
void op_spline_f(float* dst, int sd, const float* t, int st,
                 int numKnots, float** knots,
                 int n, const int* tags);

#ifdef __cplusplus
}
#endif

#endif /* RI_RSLOPS_H */
