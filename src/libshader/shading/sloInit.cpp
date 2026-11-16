/**
 * Project: openRender
 *
 * File: sloInit.cpp
 *
 * Description:
 *   The JIT backend's bind-time init (spec 033): the JIT block of the core's former CRendererContext::init()
 *   (src/ri/render/init.cpp), moved into Shading. It runs a .slo shader's init entry at bind time with the parameter
 *   defaults as its locals. Its named coordinate systems (GitHub #123) are resolved through
 *   libshader::bindCoordinateSystem(), the same bind-time lookup the interpreter's init makes; "shader" keeps using the
 *   instance's own transform through jitSetInitXform(). Folded into sloBackend.cpp when the backend split lands.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "config.h" // OPENRENDER_HAVE_LLVM

#ifdef OPENRENDER_HAVE_LLVM

#include <alloca.h>
#include <cstring>

#include "common/algebra.h"
#include "rslOps.h" // jitSetInitXform, jitSetInitCoordinateResolver
#include "serviceLookup.h"
#include "shader.h"

namespace {

    // The instance being initialised, for the resolver below: the resolver is a plain function pointer and the init runs on
    // the thread that binds the shader
    thread_local const CShaderInstance *t_initInstance = nullptr;

    // The JIT init section's resolver for named spaces (GitHub #123): the same lookup the interpreter's pfrom/vfrom
    // opcodes make in rsloInit.cpp
    bool initCoordinateResolver(const char *name, const float *&from, const float *&to) {
        ECoordinateSystem kind;
        return libshader::bindCoordinateSystem(name, t_initInstance, from, to, kind) != FALSE;
    }

} // namespace

void libshader::initialiseSloInstance(CProgrammableShaderInstance &instance) {
    CProgrammableShaderInstance *currentShaderInstance = &instance;
    CShader *currentShader = currentShaderInstance->parent;

    // .slo-only (JIT) shaders have no interpreter bytecode.
    // Run the JIT init section here, at shader-bind time, so that op_pfrom("shader",...)
    // uses context->getXform() which correctly reflects the shader's bind-time transform.
    if (currentShaderInstance->jitInitEntry != nullptr) {
        const int nVars = currentShader->numVariables;
        // locals: param slots point at persistent defaultValue storage so the
        // init results (e.g. transformed light position) persist across frames.
        // Temporary slots get transient scratch buffers (3 floats each, uniform).
        void **jitLocals = (void **)alloca(nVars * sizeof(void *));
        float *scratch = (float *)alloca(nVars * 3 * sizeof(float));
        memset(scratch, 0, nVars * 3 * sizeof(float));
        for (int vi = 0; vi < nVars; vi++)
            jitLocals[vi] = scratch + vi * 3;
        for (CVariable *v = currentShaderInstance->parameters; v; v = v->next)
            if (v->entry < nVars && v->defaultValue != nullptr)
                jitLocals[v->entry] = v->defaultValue;

        void **stuffInit[3];
        stuffInit[SL_IMMEDIATE_OPERAND] = currentShader->constantEntries;
        stuffInit[SL_GLOBAL_OPERAND] = nullptr; // init section never uses globals
        stuffInit[SL_VARYING_OPERAND] = jitLocals;

        int initTag = 0;
        // Supply the shader-space xform so op_pfrom("shader",...) in the init
        // section can convert space-qualified parameter defaults correctly.
        // activeContext() is null at bind time; jitSetInitXform provides the fallback.
        // Every other named space (GitHub #123) resolves through the bind-time lookup the
        // interpreter's init section uses (libshader::bindCoordinateSystem).
        if (currentShaderInstance->xform) {
            jitSetInitXform(currentShaderInstance->xform->from,
                            currentShaderInstance->xform->to);
        }
        t_initInstance = currentShaderInstance;
        jitSetInitCoordinateResolver(initCoordinateResolver);
        currentShaderInstance->jitInitEntry(1, (void ***)stuffInit, &initTag);
        jitSetInitCoordinateResolver(nullptr);
        jitSetInitXform(nullptr, nullptr);
        t_initInstance = nullptr;
    }
}

#endif // OPENRENDER_HAVE_LLVM
