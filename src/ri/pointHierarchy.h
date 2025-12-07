/**
 * Project: Pixie
 *
 * File: pointHierarchy.h
 *
 * Description:
 *   This file defines the interface for pointHierarchy.
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
//  File				:	pointHierarchy.h
//  Classes				:
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef POINTHIERARCHY_H
#define POINTHIERARCHY_H

#include "common/containers.h"
#include "common/global.h"
#include "map.h"
#include "options.h"
#include "pointCloud.h"
#include "texture3d.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPointHierarchy
// Description			:	A hierarchy of points
// Comments				:
class CPointHierarchy : public CTexture3d, public CMap<CPointCloudPoint> {

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CMapNode
        // Description			:	Encapsulates a node in the hierarchy
        // Comments				:
        class CMapNode {
            public:
                vector P, N;        // The center of the node
                vector radiosity;   // The radiosity of the node
                float dP, dN;       // The average radius and maximum normal deviation (cosine)
                int child0, child1; // The children indices (>=0 if internal nodes < if leaf)
        };

    public:
        CPointHierarchy(const char *, const float *from, const float *to, FILE *);
        ~CPointHierarchy();

        void lookup(float *, const float *, const float *, const float *, const float *, CShadingContext *);
        void store(const float *, const float *, const float *, float) { assert(FALSE); }
        void lookup(float *, const float *, const float *, float) { assert(FALSE); }

    protected:
        CArray<CMapNode> nodes; // This is where we keep the internal nodes
        CArray<float> data;     // This is where we actually keep the data
        int areaIndex;          // Index of the area variable
        int radiosityIndex;     // Index of the radiosity variable

        // Functions used to construct the hierarchy
        void computeHierarchy();
        int average(int numItems, int *indices);
        int cluster(int numItems, int *indices);

        // CView interface
        void draw() {}
        void bound(float *bmin, float *bmax) {}
};

#endif
