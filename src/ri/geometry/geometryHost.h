/**
 * Project: openRender
 *
 * File: geometryHost.h
 *
 * Description:
 *   The services Geometry needs from whoever hosts it (the renderer core),
 *   declared by Geometry and implemented by the host (spec 021;
 *   constitution Principle VIII: an upward capability is inverted through an
 *   interface the lower domain owns).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	geometryHost.h
//  Classes				:	CGeometryHost
//  Description			:	Host services for objects that build their
//							children lazily, at render time
//
////////////////////////////////////////////////////////////////////////
#ifndef GEOMETRYHOST_H
#define GEOMETRYHOST_H

#include "refCounter.h"

class CObject;
class CAttributes;
class CXform;

///////////////////////////////////////////////////////////////////////
// Class				:	CFrameResource
// Description			:	Something that holds objects for the rest of the
//							frame and must let go of them once rendering has
//							finished, before the scene hierarchy is freed
// Comments				:	The host keeps a reference until it has called
//							frameEnded()
class CFrameResource : public CRefCounter {
    public:
        // Rendering is over: release every object reference held
        virtual void frameEnded() = 0;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CGeometryHost
// Description			:	Implemented and registered by the renderer core.
//							Geometry calls it instead of reaching into the
//							renderer (no CRenderer / CRendererContext use).
// Comments				:	Every method may be called from any render thread.
class CGeometryHost {
    public:
        virtual ~CGeometryHost() {}

        // The objects object->instantiate(attributes, xform, ...) creates,
        // returned as a sibling chain (attached for the caller) instead of
        // entering the scene. Serialized by the host.
        virtual CObject *capture(const CObject *object, CAttributes *attributes, CXform *xform) = 0;

        // A procedural's expansion, made under a copy of `flagged` as the
        // current attributes and the procedural's own transform, returned as
        // a sibling chain (attached for the caller) instead of entering the
        // scene. Nested procedurals come back unexpanded. A SolidBegin block
        // emitted during the expansion is reported once and discarded with
        // its contents. Serialized by the host.
        virtual CObject *expand(const CObject *procedural, CAttributes *flagged) = 0;

        // Call resource->frameEnded() when the current frame's rendering is
        // over, before the scene hierarchy is freed; the host holds a
        // reference until then. Thread-safe.
        virtual void releaseAtFrameEnd(CFrameResource *resource) = 0;
};

// Registration hook: the host installs itself (NULL uninstalls). Geometry
// that needs the host when none is installed resolves to nothing.
void setGeometryHost(CGeometryHost *host);
CGeometryHost *geometryHost();

#endif // GEOMETRYHOST_H
