/**
 * Project: openRender
 *
 * File: shaderParameters.h
 *
 * Description:
 *   Shading's answers to the two questions Graphics State asks through its
 *   shader-parameter hooks (spec 025, FR-005 to FR-007): which parameters the
 *   shaders bound in an attribute block need, and which shader parameter has a
 *   given name. The renderer core installs shaderParameterHooks() when a
 *   session begins.
 *
 *   The logic moved here, verbatim, from CAttributes::checkParameters() and
 *   CAttributes::findParameter().
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADERPARAMETERS_H
#define SHADERPARAMETERS_H

#include "shaderParameterHooks.h"

namespace libshader {

    // The OR of requiredParameters() over the displacement, surface, atmosphere,
    // interior and exterior shaders and every active light, plus PARAMETER_N when
    // no surface is bound.
    unsigned int usedShaderParameters(const CAttributes &attributes);

    // The first parameter called name in the displacement, then the surface, then
    // the atmosphere shader; nullptr if none declares it. Lights are never searched.
    CVariable *findShaderParameter(const CAttributes &attributes, const char *name);

    // { &usedShaderParameters, &findShaderParameter }
    const SShaderParameterHooks &shaderParameterHooks();

} // namespace libshader

#endif
