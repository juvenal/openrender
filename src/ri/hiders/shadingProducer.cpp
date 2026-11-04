/**
 * Project: openRender
 *
 * File: shadingProducer.cpp
 *
 * Description:
 *   CHider::shadeSurface (spec 029): the producer behind CShadingContext's Geometry-facing shade(CSurface *, ...). It builds the
 *   point source and the batch description and calls shade(const ShaderGlobals &, IPointSource &) exactly once.
 *   References no CRenderer symbol.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "hider.h"
#include "situation.h"
#include "state/frameParams.h"
#include "surfacePointSource.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CHider
// Method				:	shadeSurface
// Description			:	Describe the batch and shade it
// Return Value			:	-
// Comments				:	The rules of contracts/point-source-and-globals.md, "The producer's rules"
void CHider::shadeSurface(CShadingContext &ctx, CSurface *surface, int uVertices, int vVertices, EShadingDim dim, unsigned int usedParameters, int displaceOnly) {
    using namespace openrender::shading;

    assert(&ctx == &context);
    (void)ctx;

    const CAttributes *attributes = surface->attributes;
    CSurfacePointSource source(surface, usedParameters);

    ShaderGlobals g;
    g.numPoints = uVertices * vVertices;
    g.gridU = uVertices;
    g.gridV = vVertices;
    g.shape = (dim == SHADING_2D_GRID) ? BatchShape::Grid : (dim == SHADING_2D) ? BatchShape::Span : BatchShape::PointSet;
    g.object = ObjectHandle{static_cast<const CObject *>(surface)};
    g.pathDepth = context.rayDepth();
    g.shadingCellFootprint = attributes->shadingRate;

    const openrender::FrameParams &frame = openrender::frameParams();
    g.shutterOpen = frame.shutterOpen;
    g.dtime = frame.shutterTime;

    libshader::ShadingSituation situation;
    situation.dim = (int)dim;
    situation.displaceOnly = displaceOnly;
    situation.inShadow = context.inShadowRay() ? TRUE : FALSE;
    situation.rayDepth = context.rayDepth();
    situation.illuminationHook = context.rendererIlluminationHook();
    situation.rasterHider = rasterizes();
    situation.cameraHitMode = attributes->cameraHitMode;
    situation.specularHitMode = attributes->specularHitMode;
    situation.transmissionHitMode = attributes->transmissionHitMode;
    const libshader::ShadingClass cls = libshader::classifyShading(situation);
    g.rayType = cls.rayType;
    g.hitMode = cls.hitMode;
    g.transport = cls.transport;
    g.pass = cls.pass;

    context.shade(g, source);
}
