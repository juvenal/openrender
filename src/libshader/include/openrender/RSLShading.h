/**
 * Project: openRender — libshader
 *
 * File: RSLShading.h
 *
 * Description:
 *   Public API for the RSL shader execution library.
 *
 * Phase A scope (current):
 *   - RSLShading::shade()    — execute compiled shader via interpreter
 *   - RSLShading::compile()  — compile .sl source to CShader in memory
 *   The library reaches the renderer only through its renderer services
 *   (CRendererServices, installed with CShadingContext::setDefaultServices);
 *   RSLShadingState callbacks are nullptr stubs.
 *
 * Phase B (future):
 *   - RSLShading::shadeJIT() — LLVM JIT execution path
 *   - Live RSLShadingState callbacks
 */

#pragma once

#include "RSLShadingState.h"

/* Forward-declare renderer types used in Phase A API to avoid pulling in
 * renderer headers from public code.  Callers that already include shader.h
 * will see the full definitions. */
class CShader;
class CProgrammableShaderInstance;

/**
 * RSLShading — static façade for the RSL shader library.
 *
 * All methods are static; there is no per-library instance.
 */
class RSLShading {
    public:
        RSLShading() = delete;

        /**
         * Execute a compiled shader via the interpreter.
         *
         * Phase A: state.lighting / state.texture / state.trace may be nullptr;
         * the interpreter then uses the installed renderer services (same as the
         * direct execute() call it wraps). With no services installed, a lookup
         * of a DSO shadeop, filter, step filter, light category or variable
         * returns "not found" (nullptr, the Gaussian filter, the Gaussian step
         * filter, 0 and nullptr) and reports one warning per kind; a light
         * category that cannot be resolved is skipped.
         *
         * @param shader   A prepared CProgrammableShaderInstance ready to run.
         * @param state    Shading context (varying storage, tags, callbacks).
         */
        static void shade(CProgrammableShaderInstance *shader, RSLShadingState &state);

        /* --- Future Phase B entry points (not yet implemented) ------------- */

        // static void shadeJIT(CProgrammableShaderInstance *shader, RSLShadingState &state);
};
