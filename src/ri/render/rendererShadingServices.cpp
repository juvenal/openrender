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
#include "channelRegistry.h"
#include "displayChannel.h"
#include "common/rendererc.h"
#include "texture3dChannels.h"

// One instance per process; CRendererServicesImpl is stateless.
static CRendererServicesImpl g_rendererServicesImpl;

// Texturing's view of the display channels (spec 037, FR-025): backed by the Graphics State channel registry,
// which is nullptr outside a session, so a lookup then finds nothing, exactly as with no source installed.
namespace {
    class CRendererChannelSource : public CTexture3dChannelSource {
        public:
            bool find(const char *name, STexture3dChannelInfo &out) const override {
                const CDisplayChannel *channel = channelRegistry() != nullptr ? channelRegistry()->retrieve(name) : nullptr;
                if (channel == nullptr)
                    return false;
                out.name = channel->name;
                out.numSamples = channel->numSamples;
                out.type = channel->variable != nullptr ? (int)channel->variable->type : (int)TYPE_FLOAT;
                out.fill = channel->fill; // the registry's own array, aliased
                return true;
            }
    };

    CRendererChannelSource g_channelSource;
} // namespace

// Called by CRenderer::beginRenderer() to make the renderer services
// available to all CShadingContext instances for the whole session, and to
// install the shader-parameter hooks Graphics State calls.
void rendererShadingServicesInit() {
    CShadingContext::setDefaultServices(&g_rendererServicesImpl);

    // Graphics State asks Shading, through two function pointers, which parameters the bound
    // shaders need and which parameter a name refers to (spec 025). Installed here, not in
    // rendererDeclarations.cpp: ribVector compiles that file and does not link libshader_shading.
    installShaderParameterHooks(&libshader::shaderParameterHooks());

    // Texturing reads the display channels through this (CTexture3d::defineChannels); removed with the services
    setTexture3dChannelSource(&g_channelSource);
}

// Called last by CRenderer::endRenderer(): lookups after RiEnd() get the
// no-services result.
void rendererShadingServicesShutdown() {
    setTexture3dChannelSource(nullptr);
    installShaderParameterHooks(nullptr);
    CShadingContext::setDefaultServices(nullptr);
}
