/**
 * Project: Pixie
 *
 * File: texture.h
 *
 * Description:
 *   This file defines the interface for texture.
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
//  File				:	texture.h
//  Classes				:	CTexture , CEnvironment
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef TEXTURE_H
#define TEXTURE_H

#include "attributes.h"
#include "common/algebra.h"
#include "common/containers.h"
#include "common/global.h"
#include "fileResource.h"
#include "stats.h"

class CShadingContext;
class CVaryingTextureLookup;

///////////////////////////////////////////////////////////////////////
// Class				:	CTextureInfoBase
// Description			:	This class allows textureinfo to get data
// Comments				:
class CTextureInfoBase : public CFileResource {
    public:
        CTextureInfoBase(const char *name) : CFileResource(name) {}
        virtual ~CTextureInfoBase() {}

        virtual void getResolution(float *dest) = 0;
        virtual const char *getTextureType() = 0;
        virtual int getNumChannels() = 0;
        virtual int getViewMatrix(float *dest) = 0;
        virtual int getProjectionMatrix(float *dest) = 0;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTexture
// Description			:	This class the the base of all texture types
// Comments				:
class CTexture : public CTextureInfoBase {
    public:
        CTexture(const char *name) : CTextureInfoBase(name) {
            atomicIncrement(&stats.numTextures);
            if (stats.numTextures > stats.numPeakTextures)
                stats.numPeakTextures = stats.numTextures;
        }

        virtual ~CTexture() {
            atomicDecrement(&stats.numTextures);
        }

        virtual float lookupz(float u, float v, float z, CShadingContext *context) = 0;
        virtual void lookup(float *dest, float u, float v, CShadingContext *context) = 0;
        virtual void lookup4(float *dest, const float *u, const float *v, CShadingContext *context) = 0;

        // textureinfo support
        void getResolution(float *r) {
            r[0] = 0;
            r[1] = 0;
        }
        const char *getTextureType() { return "texture"; }
        int getNumChannels() { return 0; }
        int getViewMatrix(float *) { return FALSE; }
        int getProjectionMatrix(float *) { return FALSE; }
};

///////////////////////////////////////////////////////////////////////
// Class				:	CEnvironment
// Description			:	An environment map (also encapsulates shadow maps)
// Comments				:
class CEnvironment : public CTextureInfoBase {
    public:
        CEnvironment(const char *name) : CTextureInfoBase(name) {
            atomicIncrement(&stats.numEnvironments);
            if (stats.numEnvironments > stats.numPeakEnvironments)
                stats.numPeakEnvironments = stats.numEnvironments;
        }

        virtual ~CEnvironment() {
            atomicDecrement(&stats.numEnvironments);
        }

        virtual void lookup(float *dest, const float *D0, const float *D1, const float *D2, const float *D3, CShadingContext *context) = 0;

        // textureinfo support
        void getResolution(float *r) {
            r[0] = 0;
            r[1] = 0;
        }
        const char *getTextureType() { return "environment"; }
        int getNumChannels() { return 0; }
        int getViewMatrix(float *) { return FALSE; }
        int getProjectionMatrix(float *) { return FALSE; }
};

///////////////////////////////////////////////////////////////////////
// Class				:	CDummyTexture
// Description			:	Encapsulates a texture we use if we could not load it
// Comments				:
class CDummyTexture : public CTexture {
    public:
        CDummyTexture(const char *name) : CTexture(name) {}
        ~CDummyTexture() {}

        float lookupz(float u, float v, float z, CShadingContext *context);
        void lookup(float *dest, float u, float v, CShadingContext *context);
        void lookup4(float *dest, const float *u, const float *v, CShadingContext *context);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CDummyEnvironment
// Description			:	A dummy environment map we create if we can not instantiate
// Comments				:
class CDummyEnvironment : public CEnvironment {
    public:
        CDummyEnvironment(const char *name) : CEnvironment(name) {}
        ~CDummyEnvironment() {}

        void lookup(float *dest, const float *D0, const float *D1, const float *D2, const float *D3, CShadingContext *context);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CDeepShadowHeader
// Description			:	The deep shadow map header (used internally)
// Comments				:
class CDeepShadowHeader {
    public:
        int xres, yres;     // The resolution of the file
        int xTiles, yTiles; // The number of tiles
        int tileSize;       // The tile dimensions
        int tileShift;      // The tile shift
        matrix toNDC;       // Xform to the normalized device coordinates
        matrix toCamera;    // The to camera transform
};

#endif
