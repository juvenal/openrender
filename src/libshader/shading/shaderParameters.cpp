/**
 * Project: openRender
 *
 * File: shaderParameters.cpp
 *
 * Description:
 *   Shading's implementation of Graphics State's shader-parameter hooks.
 *   Moved verbatim from CAttributes::checkParameters() and
 *   CAttributes::findParameter() (spec 025).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "shaderParameters.h"

#include <string.h>

#include "attributes.h"
#include "common/rendererc.h"
#include "common/variable.h"
#include "shader.h"
#include "shaderRefs.h"

namespace libshader {

    unsigned int usedShaderParameters(const CAttributes &attributes) {
        unsigned int used = 0;

        if (attributes.displacement())
            used |= shaderInstanceOf(attributes.displacement())->requiredParameters();
        if (attributes.surface())
            used |= shaderInstanceOf(attributes.surface())->requiredParameters();
        if (attributes.atmosphere())
            used |= shaderInstanceOf(attributes.atmosphere())->requiredParameters();
        if (attributes.interior())
            used |= shaderInstanceOf(attributes.interior())->requiredParameters();
        if (attributes.exterior())
            used |= shaderInstanceOf(attributes.exterior())->requiredParameters();

        if (!attributes.surface())
            used |= PARAMETER_N;

        for (const CActiveLight *cLight = attributes.lightSources; cLight != NULL; cLight = cLight->next) {
            used |= lightInstanceOf(*cLight)->requiredParameters();
        }

        // The bxdf slot is not part of this answer (spec 025, FR-005): a bxdf is ignored here
        // until spec 035 gives it a place in the shader-parameter model.
        return used;
    }

    CVariable *findShaderParameter(const CAttributes &attributes, const char *name) {
        CVariable *cParameter;

        if (attributes.displacement()) {
            for (cParameter = shaderInstanceOf(attributes.displacement())->parameters; cParameter != NULL; cParameter = cParameter->next) {
                if (strcmp(cParameter->name, name) == 0)
                    return cParameter;
            }
        }

        if (attributes.surface()) {
            for (cParameter = shaderInstanceOf(attributes.surface())->parameters; cParameter != NULL; cParameter = cParameter->next) {
                if (strcmp(cParameter->name, name) == 0)
                    return cParameter;
            }
        }

        if (attributes.atmosphere()) {
            for (cParameter = shaderInstanceOf(attributes.atmosphere())->parameters; cParameter != NULL; cParameter = cParameter->next) {
                if (strcmp(cParameter->name, name) == 0)
                    return cParameter;
            }
        }

        // The bxdf slot is not searched (spec 025, FR-005; spec 035). Lights are never searched.
        return NULL;
    }

    const SShaderParameterHooks &shaderParameterHooks() {
        static const SShaderParameterHooks hooks = {&usedShaderParameters, &findShaderParameter};
        return hooks;
    }

} // namespace libshader
