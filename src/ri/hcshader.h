/**
 * Project: Pixie
 *
 * File: hcshader.h
 *
 * Description:
 *   This file defines the interface for hcshader.
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
//  File				:	hcshader.h
//  Classes				:	-
//  Description			:	This file holds the hardcoded shaders
//
////////////////////////////////////////////////////////////////////////
#ifndef HCSHADER_H
#define HCSHADER_H

#include "common/global.h"
#include "shader.h"

class CActiveLight;

///////////////////////////////////////////////////////////////////////
// Class				:	CQuadLight
// Description			:	This is a quadratic area light source
// Comments				:
class CQuadLight : public CShaderInstance {
    public:
        CQuadLight(CAttributes *, CXform *);
        virtual ~CQuadLight();

        void illuminate(CShadingContext *, float **);
        void setParameters(int, const char **, const void **);
        int getParameter(const char *, void *, CVariable **, int *);
        void execute(CShadingContext *, float **);
        unsigned int requiredParameters();
        void registerDefaults(CAttributes *, CActiveLight *) {}
        const char *getName();
        float **prepare(CMemPage *&, float **, int) { return NULL; }

    private:
        vector corners[4]; // 4 corners of the light
        vector center;     // Center of the light
        float r;           // Radius of the light
        vector lightColor; // The color of the light
        float intensity;   // The intensity of the light
        int numSamples;    // The number of samples to take
        int reverse;       // TRUE if the orientation needs to be swapped
        vector N;          // The normal vector for the plane
};

///////////////////////////////////////////////////////////////////////
// Class				:	CSphereLight
// Description			:	This is a spherical area light source
// Comments				:
class CSphereLight : public CShaderInstance {
    public:
        CSphereLight(CAttributes *, CXform *);
        virtual ~CSphereLight();

        void illuminate(CShadingContext *, float **);
        void setParameters(int, const char **, const void **);
        int getParameter(const char *, void *, CVariable **, int *);
        void execute(CShadingContext *, float **);
        unsigned int requiredParameters();
        void registerDefaults(CAttributes *, CActiveLight *) {}
        const char *getName();
        float **prepare(CMemPage *&, float **, int) { return NULL; }

    private:
        vector from;       // The location of the light
        float radius;      // The radius of the light
        vector lightColor; // The color of the light
        float intensity;   // The intensity of the light
        int numSamples;    // The number of samples to take
};

#endif
