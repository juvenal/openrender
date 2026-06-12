/**
 * Project: openRender — libshader
 *
 * File: RSLShadingState.h
 *
 * Description:
 *   Public shading state and callback stubs for the RSL shader library API.
 *
 *   Phase A: callback pointers are defined here but not yet active.
 *   When nullptr, the library falls back to CRenderer::* globals (existing
 *   behaviour).  Phase B will implement live callbacks and break the
 *   CRenderer dependency entirely.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Forward declarations for Phase B callback structs (not yet implemented).
 * Defined as opaque structs so the header is self-contained.
 * ----------------------------------------------------------------------- */

/** Lighting callbacks — drives illuminate/solar/illuminance loops. */
struct RSLLightingCallbacks;

/** Texture callbacks — drives texture() / environment() calls. */
struct RSLTextureCallbacks;

/** Ray-tracing callbacks — drives trace() / transmission() calls. */
struct RSLTraceCallbacks;

/* -----------------------------------------------------------------------
 * RSLShadingState
 *
 * Describes the per-shading-point context passed to RSLShading::shade().
 * All pointer fields mirror the existing CShadingState layout so Phase A/B
 * can delegate directly to the interpreter without copying data.
 * ----------------------------------------------------------------------- */

#ifdef __cplusplus
class CShadingContext; /* forward declaration — C++ only */
#endif

struct RSLShadingState {
    int      numVertices;   /**< Number of shading points in this batch      */
    float  **varying;       /**< Indexed by VARIABLE_* — P, N, Cs, Ci, …    */
    float  **locals;        /**< Shader parameter + temporary variable storage */
    int     *tags;          /**< Per-vertex activity mask (0 = active)        */

#ifdef __cplusplus
    /** Phase B: shading context that owns execute(). Required for shade(). */
    CShadingContext *ctx;   /**< The active shading context                  */
#else
    void *ctx;
#endif

    /**
     * Phase B callbacks — set to nullptr in Phase A; the library then
     * falls back to CRenderer::* globals for lighting, texture, and tracing.
     */
    struct RSLLightingCallbacks *lighting; /**< nullptr until Phase B        */
    struct RSLTextureCallbacks  *texture;  /**< nullptr until Phase B        */
    struct RSLTraceCallbacks    *trace;    /**< nullptr until Phase B        */
};

#ifdef __cplusplus
} /* extern "C" */
#endif
