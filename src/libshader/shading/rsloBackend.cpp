/**
 * Project: openRender
 *
 * File: rsloBackend.cpp
 *
 * Description:
 *   The interpreter backend (spec 033, contracts/shader-backend.md): key and extension "rslo", priority 0. It loads a .rslo file
 *   with parseShader() (rslo.y), makes the instance (CProgrammableShaderInstance, the instance both RSL backends use), runs the
 *   bind-time init of rsloInit.cpp and executes through CShadingContext::interpret().
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

#include "shader.h"
#include "shading.h"

// This one is defined in rslo.y
CShader *parseShader(const char *, const char *);

namespace {

    class CRsloBackend final : public IShaderBackend {
        public:
            const char *key() const override { return "rslo"; }
            const char *extension() const override { return "rslo"; }

            CShader *load(const char *shaderName, const char *path) const override { return parseShader(shaderName, path); }

            CShaderInstance *instantiate(CShader *desc, CAttributes *attributes, CXform *xform) const override {
                return new CProgrammableShaderInstance(desc, attributes, xform);
            }

            void initialise(CShaderInstance &instance) const override {
                libshader::initialiseRsloInstance(static_cast<CProgrammableShaderInstance &>(instance));
            }

            void execute(CShadingContext &context, CShaderInstance &instance, float **locals) const override {
                context.interpret(static_cast<CProgrammableShaderInstance *>(&instance), locals);
            }
    };

    // Registers the backend when the library loads (a shared library: the registrar always runs)
    struct SRegistrar {
            SRegistrar() { registerShaderBackend(&rsloBackend(), 0); }
    } s_registrar;

} // namespace

const IShaderBackend &rsloBackend() {
    static const CRsloBackend backend;
    return backend;
}
