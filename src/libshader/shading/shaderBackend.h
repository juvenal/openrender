/**
 * Project: openRender
 *
 * File: shaderBackend.h
 *
 * Description:
 *   The seam between the shading engine and a shader format (spec 033, FR-002, FR-010, contracts/shader-backend.md). A backend
 *   loads a shader file into a backend-neutral description (CShader) with a payload of its own, makes the instance, runs the
 *   instance's bind-time init and executes it for the context's current batch. The two built-ins are the interpreter's (key
 *   and extension "rslo", priority 0, rsloBackend.cpp) and the JIT's ("slo", priority 10, sloBackend.cpp). No member names a
 *   language, a closure, an LLVM type or a bytecode type: another format registers a backend and nothing else changes.
 *
 *   Phase 4 of the spec adds this interface and the two built-ins; the facade that owns the cache, the format order and the
 *   binding (ShadingSystem) arrives in Phase 5, and reads the registry below.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADERBACKEND_H
#define SHADERBACKEND_H

#include <string>
#include <vector>

#include "config.h" // OPENRENDER_HAVE_LLVM

class CShader;
class CShaderInstance;
class CShadingContext;
class CAttributes;
class CXform;

class IShaderBackend {
    public:
        virtual ~IShaderBackend() = default;
        virtual const char *key() const = 0;       // "rslo", "slo"
        virtual const char *extension() const = 0; // passed to the file resolver
        // A description whose backend is this and whose payload this backend owns, or nullptr with the format's own
        // diagnostics (the file is then left to the next backend in the order)
        virtual CShader *load(const char *shaderName, const char *path) const = 0;
        virtual CShaderInstance *instantiate(CShader *desc, CAttributes *attributes, CXform *xform) const = 0;
        // The bind-time init of the instance's parameter defaults; the caller wraps it in arenaBegin/End(globalArena)
        virtual void initialise(CShaderInstance &instance) const = 0;
        // Run the instance for the context's current batch (varying, tags, lights as today)
        virtual void execute(CShadingContext &context, CShaderInstance &instance, float **locals) const = 0;
};

// The registry. A backend registers itself with its priority (the lower the earlier among the formats the preference does not
// name); a format the build cannot offer registers its warning text instead (it takes the shader name). Tests unregister.
void registerShaderBackend(const IShaderBackend *backend, int priority);
void registerUnavailableShaderFormat(const char *key, const char *warningFormat);
void unregisterShaderBackend(const char *key); // tests only

// One entry of the registry, as a snapshot (the registry is read by the facade of Phase 5)
struct SShaderBackendEntry {
        std::string key;
        const IShaderBackend *backend; // nullptr: an unavailable format
        int priority;
        std::string unavailableWarning; // the warning format of an unavailable format
};
std::vector<SShaderBackendEntry> shaderBackendEntries();

// The built-ins' singletons, which the loaders put in the descriptions they make
const IShaderBackend &rsloBackend();
#ifdef OPENRENDER_HAVE_LLVM
const IShaderBackend &sloBackend();
#endif

#endif
