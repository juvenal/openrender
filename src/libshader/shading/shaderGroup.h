/**
 * Project: openRender
 *
 * File: shaderGroup.h
 *
 * Description:
 *   The group-shaped API of the shading engine's shaders (spec 033, FR-011, FR-012, contracts/shader-group.md). Every binding the
 *   renderer makes is a group of one layer, built through ShaderGroupBuilder, and shade(), the volume step and the imager run a
 *   group through groupPrepare()/groupExecute(). A group of one layer IS its layer's instance (P-Q5): the group handle the slots
 *   hold, the light handle and shaderInstanceOf() are unchanged. Spec 034 replaces "the instance is the group" with a group
 *   object of several layers behind this API, and gives connect() a body; no caller changes when it does.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADERGROUP_H
#define SHADERGROUP_H

#include "common/memory.h"
#include "shadingSystem.h"

class CShaderGroupRefTarget;
class CShadingContext;

namespace openrender::shading {

    class ShaderGroupBuilder {
        public:
            explicit ShaderGroupBuilder(ShadingSystem &system) : system(system), instance(nullptr) {}

            // Start a group (a layer an earlier use never ended is dropped, not released)
            void begin();

            // Bind one layer: ShadingSystem::bind() then setParameters(np, tokens, values). false when the layer was not bound (its
            // diagnostics were given) or was refused (a second layer: CODE_INCAPABLE, nothing loaded, instantiated or keyed)
            bool shader(const ShaderRequest &request, CAttributes *attributes, CXform *xform, IShaderBindHost &host, int np, const char **tokens,
                        const void **values);

            // Always refused before spec 034: CODE_INCAPABLE, returns false, changes nothing
            bool connect(const char *fromLayer, const char *fromParameter, const char *toLayer, const char *toParameter);

            // The bound group: its single layer's instance with one reference (the caller owns it), or nullptr if no layer was bound
            CShaderInstance *end();

        private:
            ShadingSystem &system;
            CShaderInstance *instance; // the one layer, held until end()
    };

} // namespace openrender::shading

namespace libshader {

    // Allocate the group's locals for numVertices points from memory and copy the parameter defaults
    float **groupPrepare(const CShaderGroupRefTarget *group, CMemPage *&memory, float **varying, int numVertices);

    // Run the group for the context's current batch
    void groupExecute(CShadingContext &context, const CShaderGroupRefTarget *group, float **locals);

} // namespace libshader

#endif
