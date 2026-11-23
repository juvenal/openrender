/**
 * Project: openRender
 *
 * File: shaderGroup.cpp
 *
 * Description:
 *   ShaderGroupBuilder and the group run helpers (spec 033, contracts/shader-group.md). With one layer, which is its own group
 *   (P-Q5), the run helpers make the calls the sites made before: prepare() and execute() of the layer's instance.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "shaderGroup.h"

#include "common/diagnostics.h"
#include "shaderRefs.h"
#include "shading.h"

namespace openrender::shading {

    void ShaderGroupBuilder::begin() {
        // A layer an earlier group bound and never ended is dropped, not detached: a light is already in the frame's light list
        // (no extra reference), so releasing it here would leave that entry dangling. The renderer always ends what it begins.
        instance = nullptr;
    }

    bool ShaderGroupBuilder::shader(const ShaderRequest &request, CAttributes *attributes, CXform *xform, IShaderBindHost &host, int np,
                                    const char **tokens, const void **values) {
        if (instance != nullptr) {
            // A second layer: refused before anything is located, loaded, instantiated or keyed
            error(CODE_INCAPABLE, "Shader group connections and multi-layer groups are not supported before spec 034\n");
            return false;
        }

        CShaderInstance *layer = system.bind(request, attributes, xform, host);
        if (layer == nullptr)
            return false; // the shading system gave the diagnostic

        layer->setParameters(np, tokens, values);
        // bind() built a light's categories from the shader's default __category; the RIB's own __category is set only now (GitHub #199)
        if (request.type == SL_LIGHTSOURCE)
            layer->createCategories();
        instance = layer;
        return true;
    }

    bool ShaderGroupBuilder::connect(const char *, const char *, const char *, const char *) {
        error(CODE_INCAPABLE, "Shader group connections and multi-layer groups are not supported before spec 034\n");
        return false;
    }

    CShaderInstance *ShaderGroupBuilder::end() {
        CShaderInstance *group = instance;
        instance = nullptr; // handed to the caller with its one reference
        return group;
    }

} // namespace openrender::shading

namespace libshader {

    float **groupPrepare(const CShaderGroupRefTarget *group, CMemPage *&memory, float **varying, int numVertices) {
        return shaderInstanceOf(group)->prepare(memory, varying, numVertices);
    }

    void groupExecute(CShadingContext &context, const CShaderGroupRefTarget *group, float **locals) {
        shaderInstanceOf(group)->execute(&context, locals);
    }

} // namespace libshader
