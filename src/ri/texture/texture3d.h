/**
 * Project: openRender
 *
 * File: texture3d.h
 *
 * Description:
 *   This file defines the interface for texture3d.
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
//  File				:	texture3d.h
//  Classes				:
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef TEXTURE3D_H
#define TEXTURE3D_H

#include "common/algebra.h"
#include "common/global.h"
#include "common/os.h"
#include "common/fileResource.h"
#include "common/rendererc.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CTexture3d
// Description			:	Base class for 3d textures
// Comments				:
class CTexture3d : public CFileResource {
    protected:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	CChannel
        // Description			:	A 3d texture channel
        // Comments				:
        class CChannel {
            public:
                char name[64];      // Name of the channel
                int numSamples;     // The size of channel sample
                int sampleStart;    // Offset of the sample in the stack
                float *fill;        // The sample defaults
                EVariableType type; // The funamental type
        };

    public:
        CTexture3d(const char *name, const float *from, const float *to, const float *tondc = NULL, int channelCount = 0, CChannel *channels = NULL);
        virtual ~CTexture3d();

        // For storing/querying data with radius
        virtual void lookup(float *, const float *, const float *, float) = 0;
        virtual void store(const float *, const float *, const float *, float) = 0;

        // For point-based queries (the point hierarchy): what the lookup in progress asks of it, which the caller reads from
        // its own state and hands in
        struct CPointBasedLookup {
                float maxSolidAngle;
        };
        virtual void lookupPointBased(float *C, const float *P, const float *dPdu, const float *dPdv, const float *N, const CPointBasedLookup &opt) = 0;

        // Resolve the names to channels
        void resolve(int n, const char **names, int *entry, int *size);

        // ptcAPI interface
        void queryChannels(int *, const char **, const char **);
        int getDataSize() { return dataSize; }
        void getFromMatrix(float *m) { movmm(m, from); }
        void getToMatrix(float *m) { movmm(m, to); }
        void getNDCMatrix(float *m) { movmm(m, toNDC); }

        // Storage accessors the viewer and the remote channels read, shared by every CTexture3d-derived
        // structure (CBrickMap, CPointCloud, CIrradianceCache)
        int numChannels() const { return channelCount; }
        const char *channelName(int index) const {
            return (index >= 0 && index < channelCount) ? channels[index].name : NULL;
        }
        int channelSampleStart(int i) const { return channels[i].sampleStart; }
        int channelNumSamples(int i) const { return channels[i].numSamples; }

        // Channel definition from a comma-separated list of names (the remote point-cloud channel's setup)
        void defineChannels(const char *);

        // The transformation of a cloud a remote channel fills (identity, scale 1: the client has no world of its own)
        void setTransform(const float *f, const float *t) {
            movmm(from, f);
            movmm(to, t);
        }
        void setDPScale(float s) { dPscale = s; }

        int dataSize; // The size of each data sample
    protected:
        void defineChannels(int, char **, char **);
        void writeChannels(FILE *);
        void readChannels(FILE *);

        matrix from, to;    // The transformation to the coordinate system
        matrix toNDC;       // The viewing transform
        float dPscale;      // The amount we need to scale dP by
        int channelCount;   // Number of channels
        CChannel *channels; // List of channels
};

///////////////////////////////////////////////////////////////////////
// Function				:	texture3Dflatten
// Description			:	Flatten the channel datas into a vector
// Return Value			:	-
// Comments				:
inline void texture3Dflatten(float *dest, int n, const float **data, int *entry, int *size) {
    int i;

    // For every channel
    for (i = 0; i < n; i++) {
        const float *src = *data++;

        // Unroll for the common cases
        switch (size[i]) {
            case 0:
                // This can happen if the channel was not found
                break;
            case 1:
                dest[entry[i]] = *src;
                break;
            case 2:
                dest[entry[i]] = *src++;
                dest[entry[i] + 1] = *src;
                break;
            case 3:
                movvv(dest + entry[i], src);
                break;
            default:
                for (int j = 0; j < size[i]; j++)
                    dest[entry[i] + j] = *src++;
                break;
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	texture3DflattenInterpolated
// Description			:	Flatten the channel datas into a vector
// Return Value			:	-
// Comments				:
inline void texture3DflattenInterpolated(float *dest, int n, const float **data, int *entry, int *size, int uVertices) {
    int i;

    // For every channel
    for (i = 0; i < n; i++) {
        const float *src = *data++;

        // Unroll for the common cases
        switch (size[i]) {
            case 0:
                // This can happen if the channel was not found
                break;
            case 1:
                dest[entry[i]] = 0.25f * (src[0] + src[1] + src[uVertices] + src[uVertices + 1]);
                break;
            case 2:
                dest[entry[i]] = 0.25f * (src[0] + src[1] + src[uVertices] + src[uVertices + 1]);
                dest[entry[i] + 1] = 0.25f * (src[1] + src[2] + src[uVertices * 2 + 1] + src[uVertices * 2 + 2]);
                break;
            case 3:
                initv(dest + entry[i], 0);
                addvv(dest + entry[i], src);
                addvv(dest + entry[i], src + 3);
                addvv(dest + entry[i], src + uVertices * 3);
                addvv(dest + entry[i], src + uVertices * 3 + 3);
                mulvf(dest + entry[i], 0.25f);
                break;
            default:
                for (int j = 0; j < size[i]; j++)
                    dest[entry[i] + j] = 0;
                for (int j = 0; j < size[i]; j++)
                    dest[entry[i] + j] += src[j];
                for (int j = 0; j < size[i]; j++)
                    dest[entry[i] + j] += src[j + size[i]];
                for (int j = 0; j < size[i]; j++)
                    dest[entry[i] + j] += src[j + uVertices * size[i]];
                for (int j = 0; j < size[i]; j++)
                    dest[entry[i] + j] += src[j + uVertices * size[i] + size[i]];
                for (int j = 0; j < size[i]; j++)
                    dest[entry[i] + j] *= 0.25f;
                break;
        }
    }
}
///////////////////////////////////////////////////////////////////////
// Function				:	texture3Dunpack
// Description			:	Unpack the data from a flat array
// Return Value			:	-
// Comments				:
inline void texture3Dunpack(const float *src, int n, float **data, int *entry, int *size) {
    int i;

    // For every channel
    for (i = 0; i < n; i++) {
        float *dest = *data++;

        // Unroll for the common cases
        switch (size[i]) {
            case 0:
                // This can happen is the channel was not found
                break;
            case 1:
                *dest = src[entry[i]];
                break;
            case 2:
                dest[0] = src[entry[i]];
                dest[1] = src[entry[i] + 1];
                break;
            case 3:
                movvv(dest, src + entry[i]);
                break;
            default:
                for (int j = 0; j < size[i]; j++)
                    dest[j] = src[entry[i] + j];
                break;
        }
    }
}

#endif // TEXTURE3D_H
