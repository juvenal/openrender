/**
 * Project: openRender
 *
 * File: pointCloud.h
 *
 * Description:
 *   This file defines the interface for pointCloud.
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
//  File				:	pointCloud.h
//  Classes				:
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include "common/containers.h"
#include "common/global.h"
#include "map.h"
#include "texture3d.h"
#include "textureResolver.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPointCloudPoint
// Description			:	A point cloud point
// Comments				:
class CPointCloudPoint : public CMapItem {
    public:
        float dP;        // The sample radius
        int entryNumber; // The index to find the associated data in the "data" array
};

///////////////////////////////////////////////////////////////////////
// Class				:	CPointCloud
// Description			:	A collection of points
// Comments				:
class CPointCloud : public CTexture3d, public CMap<CPointCloudPoint> {
    public:
        CPointCloud(const char *name, const float *from, const float *to, const float *toNDC, const char *, int);
        CPointCloud(const char *name, const float *from, const float *to, const float *toNDC, int, char **, char **, int);
        CPointCloud(const char *name, const float *from, const float *to, FILE *);
        ~CPointCloud();

        // Misc interface
        void reset();
        void write();
        void balance();

        // Store/Lookup interface
        void store(const float *, const float *, const float *, float);
        void lookup(float *, const float *, const float *, float);
        void lookup(float *, const float *, const float *, const float *, const float *, CShadingContext *) { assert(FALSE); }

        // The extent and the name of the structure (the data viewer's adapter, CPointCloudView, reports them)
        void bound(float *bmin, float *bmax);
        const char *typeName() const { return "Point Cloud"; }

        // ptcApi interface
        int getNumPoints() const { return numItems; }
        void getPoint(int i, float *C, float *P, float *N, float *dP);

        // Read access to the stored points and their data (the viewer and the remote channel read them)
        const CPointCloudPoint *points() const { return items + 1; }
        const float *pointData() const { return data.array; }

    private:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	CPointLookup
        // Description			:	Extend the CLookup to include extra info
        // Comments				:
        class CPointLookup : public CLookup {
            public:
                int ignoreNormal;
        };

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CMap
        // Method				:	lookup
        // Description			:	Locate the nearest valid points
        // Return Value			:	Internally used
        // Comments				:
        void lookup(CPointLookup *l, int index, const float scale) {
            const CPointCloudPoint *photon = &items[index];
            float d;
            vector D;
            int axis = photon->flags;

            if (index < numItemsh) {
                d = l->P[axis] - photon->P[axis];

                if (d > 0) {
                    lookup(l, 2 * index + 1, scale);

                    if (d * d < l->distances[0]) {
                        lookup(l, 2 * index, scale);
                    }
                }
                else {
                    lookup(l, 2 * index, scale);

                    if (d * d < l->distances[0]) {
                        lookup(l, 2 * index + 1, scale);
                    }
                }
            }

            subvv(D, photon->P, l->P);
            d = dotvv(D, D);
            const float t = dotvv(D, l->N);
            d += t * t * 16;

            // If the item is valid, insert it
            if (d < l->distances[0]) {
                if (d < (photon->dP * photon->dP * scale * scale)) {

                    // Note that we do the opposite to photonmaps
                    // only entries coherent with N contribute
                    // but l.N is reversed...
                    if ((dotvv(photon->N, l->N) < 0) || l->ignoreNormal) {
                        insert(l, d, photon);
                    }
                }
            }
        }

        CArray<float> data; // This is where we actually keep the data
        int flush;          // Should this be written to disk?
        TMutex mutex;       // To synchronize updates
        float maxdP;

        friend void makeBrickMap(int n, const char **, const char *, const CTextureFileResolver *, int, const char **, const void **);
};

#endif // POINTCLOUD_H
