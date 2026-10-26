/**
 * Project: openRender
 *
 * File: rendererShadingServices.cpp
 *
 * Description:
 *   Hosts the CRendererServicesImpl singleton and registers it with
 *   CShadingContext::setDefaultServices() at renderer startup. This file
 *   stays in src/ri/ so it can freely include renderer.h and
 *   rendererServicesImpl.h without creating a circular dependency with
 *   src/libshader/shading/.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "rendererServicesImpl.h"
#include "shaderParameterHooks.h"
#include "shaderParameters.h"
#include "shading.h"

// One instance per process; CRendererServicesImpl is stateless.
static CRendererServicesImpl g_rendererServicesImpl;

// Called by CRenderer::beginRenderer() to make the renderer services
// available to all CShadingContext instances for the whole session, and to
// install the shader-parameter hooks Graphics State calls.
void rendererShadingServicesInit() {
    CShadingContext::setDefaultServices(&g_rendererServicesImpl);

    // Graphics State asks Shading, through two function pointers, which parameters the bound
    // shaders need and which parameter a name refers to (spec 025). Installed here, not in
    // rendererDeclarations.cpp: ribVector compiles that file and does not link libshader_shading.
    installShaderParameterHooks(&libshader::shaderParameterHooks());
}

// Called last by CRenderer::endRenderer(): lookups after RiEnd() get the
// no-services result.
void rendererShadingServicesShutdown() {
    installShaderParameterHooks(nullptr);
    CShadingContext::setDefaultServices(nullptr);
}
