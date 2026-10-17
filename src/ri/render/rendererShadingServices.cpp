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
#include "shading.h"

// One instance per process; CRendererServicesImpl is stateless.
static CRendererServicesImpl g_rendererServicesImpl;

// Called by CRenderer::beginRenderer() to make the renderer services
// available to all CShadingContext instances for the whole session.
void rendererShadingServicesInit() {
    CShadingContext::setDefaultServices(&g_rendererServicesImpl);
}

// Called last by CRenderer::endRenderer(): lookups after RiEnd() get the
// no-services result.
void rendererShadingServicesShutdown() {
    CShadingContext::setDefaultServices(nullptr);
}
