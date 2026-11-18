/**
 * Project: openRender
 *
 * File: sloBackend.cpp
 *
 * Description:
 *   The JIT backend (spec 033, contracts/shader-backend.md): key and extension "slo", priority 10. It loads a .slo file with
 *   parseSloShader() (sloLoader.cpp), makes the same instance the interpreter does, runs the .slo shader's init entry at bind time
 *   (the JIT block of the core's former CRendererContext::init(), moved here with sloInit.cpp) and executes the compiled entry
 *   (the JIT block of CShadingContext::execute(), moved here verbatim).
 *
 *   In a build without the JIT the format is registered as unavailable, with the text the renderer gave before: a request that
 *   prefers "slo" warns once per uncached shader and the .rslo is used.
 *
 *   The dispatch invariant: jitEntry gets a non-null value only in parseSloShader() (sloLoader.cpp); the rest of the library only
 *   nulls or copies it. So "jitEntry != nullptr" means the shader was loaded by parseSloShader() and compiled, and a null jitEntry
 *   in a CSloPayload means it was loaded by parseSloShader() and the JIT compile failed (no bytecode to fall back to).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "shaderBackend.h"

#include "config.h" // OPENRENDER_HAVE_LLVM

#ifdef OPENRENDER_HAVE_LLVM

#include <alloca.h>
#include <cassert>
#include <cstring>

#include "activeContext.h"
#include "common/algebra.h"
#include "common/diagnostics.h"
#include "include/logging.hpp"
#include "rslo_code.h"
#include "rslOps.h" // jitSetInitXform, jitSetInitCoordinateResolver
#include "serviceLookup.h"
#include "shader.h"
#include "shaderPayload.h"
#include "shading.h"
#include "sloLoader.h"

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

    class CSloBackend final : public IShaderBackend {
        public:
            const char *key() const override { return "slo"; }
            const char *extension() const override { return "slo"; }

            CShader *load(const char *shaderName, const char *path) const override { return parseSloShader(shaderName, path); }

            CShaderInstance *instantiate(CShader *desc, CAttributes *attributes, CXform *xform) const override {
                return new CProgrammableShaderInstance(desc, attributes, xform);
            }

            // The bind-time init: the JIT block of the core's former CRendererContext::init()
            void initialise(CShaderInstance &base) const override {
                CProgrammableShaderInstance *currentShaderInstance = static_cast<CProgrammableShaderInstance *>(&base);
                CShader *currentShader = currentShaderInstance->parent;
                CSloPayload *payload = sloPayloadOf(*currentShader);

                // .slo-only (JIT) shaders have no interpreter bytecode.
                // Run the JIT init section here, at shader-bind time, so that op_pfrom("shader",...)
                // uses context->getXform() which correctly reflects the shader's bind-time transform.
                if (payload->jitInitEntry != nullptr) {
                    const CRslPayload *frame = payload;
                    const int nVars = frame->numVariables;
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
                    stuffInit[SL_IMMEDIATE_OPERAND] = nullptr; // a .slo has no constant table
                    stuffInit[SL_GLOBAL_OPERAND] = nullptr;    // init section never uses globals
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
                    payload->jitInitEntry(1, (void ***)stuffInit, &initTag);
                    jitSetInitCoordinateResolver(nullptr);
                    jitSetInitXform(nullptr, nullptr);
                    t_initInstance = nullptr;
                }
            }

            // The JIT block of CShadingContext::execute(), moved here verbatim
            void execute(CShadingContext &context, CShaderInstance &base, float **locals) const override {
                CProgrammableShaderInstance *cInstance = static_cast<CProgrammableShaderInstance *>(&base);
                CShadingState *currentShadingState = context.currentShadingState;

                // This is the current shader we're executing
                CShader *currentShader = cInstance->parent;
                CSloPayload *payload = sloPayloadOf(*currentShader);

                assert((currentShadingState->numActive + currentShadingState->numPassive) == currentShadingState->numVertices);

                currentShadingState->currentShaderInstance = cInstance;
                int *tagStart = currentShadingState->tags;

                // Save this stuff for fast access
                int numVertices = currentShadingState->numVertices;
                float **varying = currentShadingState->varying;
                CShadedLight **alights = &currentShadingState->alights;

                // Set the access arrays
                void **stuff[3];
                stuff[SL_IMMEDIATE_OPERAND] = nullptr;       // a .slo has no constant table
                stuff[SL_GLOBAL_OPERAND] = (void **)varying; // Global variables
                stuff[SL_VARYING_OPERAND] = (void **)locals; // Local variables

                // JIT dispatch: if a compiled entry point exists for this shader, call it
                // instead of the interpreter and return immediately.
                if (payload->jitEntry != nullptr) {
                    log_debug("[JIT-PROBE] execute: shader={} jitEntry={}", cInstance->getName(), (void *)payload->jitEntry);
                    // Save outer context: a surface shader's JIT may nest into a light shader's
                    // JIT via callDiffuse/callAmbient → light->illuminate() → execute().
                    // Without save/restore the outer context is cleared on return, breaking
                    // all subsequent op_* calls in the outer JIT function.
                    CShadingContext *savedCtx = libshader::activeContext();
                    libshader::setActiveContext(&context);
                    payload->jitEntry(numVertices, stuff, tagStart);
                    libshader::setActiveContext(savedCtx);
                    if (currentShader->type == SL_SURFACE) {
                        const float *CI = varying[VARIABLE_CI];
                        log_debug("[JIT-DBG] post-JIT CI[0]=[{:.3f},{:.3f},{:.3f}]", CI[0], CI[1], CI[2]);
                    }
                    // Mirror interpreter execEnd: accumulate ambient Cl for lightsource shaders.
                    if (currentShader->type == SL_LIGHTSOURCE &&
                        !(currentShader->usedParameters & PARAMETER_NONAMBIENT) &&
                        *alights != nullptr) {
                        const float *Cl = varying[VARIABLE_CL];
                        float *Clsave = (*alights)->savedState[1];
                        int *tags = tagStart;
                        for (int i = numVertices; i > 0; --i, Cl += 3, Clsave += 3, ++tags) {
                            if (*tags == 0)
                                addvv(Clsave, Cl);
                        }
                    }
                    return;
                }

                // .slo-only shader: codeArea is null and JIT entry is also null (JIT failed).
                warning(CODE_SYSTEM, "Shader \"%s\" has no bytecode and no JIT entry — skipping",
                        currentShader->name);
            }
    };

    struct SRegistrar {
            SRegistrar() { registerShaderBackend(&sloBackend(), 10); }
    } s_registrar;

} // namespace

const IShaderBackend &sloBackend() {
    static const CSloBackend backend;
    return backend;
}

#else // OPENRENDER_HAVE_LLVM

namespace {

    // The build has no JIT: "slo" is a format that cannot be offered. The text is the one the renderer gave before (the shader name
    // fills the %s).
    struct SRegistrar {
            SRegistrar() {
                registerUnavailableShaderFormat("slo",
                                                "Shader \"%s\": shaderformat \"slo\" requested, but this build has no "
                                                "LLVM JIT support; using the .rslo interpreter instead\n");
            }
    } s_registrar;

} // namespace

#endif // OPENRENDER_HAVE_LLVM
