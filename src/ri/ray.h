/**
 * Project: Pixie
 *
 * File: ray.h
 *
 * Description:
 *   This file defines the interface for ray.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	ray.h
//  Classes				:	CRay
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef RAY_H
#define RAY_H

#include "common/algebra.h"
#include "common/global.h"

class CSurface;

////////////////////////////////////////////////////////////////////////////////////
// Ray class ---
// This class is the heart of the message system
// It holds data about the execution environment in the form of void pointers
// Every class that touches the ray class sets the corresponding pointers
class CRay {
    public:
        // ------------------> I N P U T
        vector from, dir; // The ray in camera coordinate system
        float time;       // The time of the ray (0 <= time <= 1)
        int flags;        // The intersection flags
        float t;          // The maximum intersection (if intersection, this is overwritten)
        float tmin;       // The intersection bias
        float da, db;     // The linear ray differential (r = da*t + db)

        // ------------------> O U T P U T
        CSurface *object; // The intersection object (NULL if no intersection)
        float u, v;       // The parametric intersection coordinates on the object
        vector N;         // The normal vector at the intersection

        // ------------------> I N T E R M E D I A T E
        float jimp;     // The jittered importance (sampled automatically before tracing)
        dvector invDir; // 1/dir
        CRay *child;    // For keeping track of the opacity
};

#endif
