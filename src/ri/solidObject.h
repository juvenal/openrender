/**
 * Project: openRender
 *
 * File: solidObject.h
 *
 * Description:
 *   This file defines the interface for solidObject.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	solidObject.h
//  Classes				:	CSolidObject
//  Description			:	Resolved Solid Boundary produced by RiSolidEnd
//
////////////////////////////////////////////////////////////////////////
#ifndef SOLIDOBJECT_H
#define SOLIDOBJECT_H

#include "object.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CSolidObject
// Description			:	Holds the flat list of CPolygonMesh Boundary
//							Fragments produced by resolving a SolidBegin/
//							SolidEnd CSG tree (already expressed in the
//							outermost block's local frame). A pure
//							container: REYES dicing and raytrace traversal
//							both use the generic CObject children/cluster()
//							machinery (OBJECT_DUMMY), so no CSG resolution
//							happens in any hider. Mirrors CDelayedInstance's
//							lazy per-instance replay so a solid captured
//							inside RiObjectBegin/RiObjectEnd can be placed
//							by RiObjectInstance more than once.
// Comments				:
class CSolidObject : public CObject {
    public:
        CSolidObject(CAttributes *, CXform *, CObject *fragments);
        virtual ~CSolidObject();

        virtual void intersect(CShadingContext *, CRay *);
        virtual void dice(CReyes *);
        virtual void instantiate(CAttributes *, CXform *, CRiInterface *) const;

        CObject *fragments; // Template Boundary Fragments, chained via CObject::sibling
        int processed;      // TRUE once fragments have been instantiated into children
};

#endif
