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
#include "libshader/closures/bxdfInstance.h"
#include "libshader/closures/closureInterim.h"
#include "common/rendererc.h"
#include "common/variable.h"
#include "shader.h"
#include "shaderRefs.h"
#include "shading.h"

namespace libshader {

    unsigned int usedShaderParameters(const CAttributes &attributes) {
        unsigned int used = 0;

        // Spec 035: when a Bxdf governs (the last-bound record says so and the slot holds a Bxdf instance), the surface slot is replaced by the
        // Bxdf's substitute, interimSurfaceFor(): the captured defaultsurface, or no shader (PARAMETER_N below). A consumer installed under a
        // Bxdf adds Cs and Os, which its trees need. A foreign object in the bxdf slot is not a Bxdf instance and changes nothing.
        const openrender::shading::CBxdfInstance *bxdf = attributes.governingMaterial() == EMaterialBinding::Bxdf ? openrender::shading::bxdfInstanceOf(attributes.bxdf()) : nullptr;
        CShaderInstance *surface = bxdf != nullptr ? openrender::shading::interimSurfaceFor(*bxdf) : shaderInstanceOf(attributes.surface());

        if (attributes.displacement())
            used |= shaderInstanceOf(attributes.displacement())->requiredParameters();
        if (surface != nullptr)
            used |= surface->requiredParameters();
        if (bxdf != nullptr && CShadingContext::defaultClosureConsumer() != nullptr)
            used |= PARAMETER_CS | PARAMETER_OS;
        if (attributes.atmosphere())
            used |= shaderInstanceOf(attributes.atmosphere())->requiredParameters();
        if (attributes.interior())
            used |= shaderInstanceOf(attributes.interior())->requiredParameters();
        if (attributes.exterior())
            used |= shaderInstanceOf(attributes.exterior())->requiredParameters();

        if (surface == nullptr)
            used |= PARAMETER_N;

        for (const CActiveLight *cLight = attributes.lightSources; cLight != NULL; cLight = cLight->next) {
            used |= lightInstanceOf(*cLight)->requiredParameters();
        }

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
