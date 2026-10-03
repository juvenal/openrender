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

class CObject;
class CAttributes;
class CXform;

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
};

// Registration hook: the host installs itself (NULL uninstalls). Geometry
// that needs the host when none is installed resolves to nothing.
void setGeometryHost(CGeometryHost *host);
CGeometryHost *geometryHost();

#endif // GEOMETRYHOST_H
