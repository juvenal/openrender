/**
 * Project: Pixie
 *
 * File: occlusion.h
 *
 * Description:
 *   This file defines the interface for occlusion.
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
//  File				:	occlusion.h
//  Classes				:	COcclusionCuller
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef OCCLUSION_H
#define OCCLUSION_H

#include "common/global.h"
#include "common/os.h"

///////////////////////////////////////////////////////////////////////
// Class				:	COcclusionCuller
// Description			:	The occlusion culler class
// Comments				:
class COcclusionCuller {
    public:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	COcclusionCuller
        // Description			:	The occlusion culler class
        // Comments				:
        class COcclusionNode {
            public:
                COcclusionNode *parent;      // The parent pointer
                COcclusionNode *children[4]; // The
                float zmax;                  // The depth range
                int width;
        };

        COcclusionCuller();
        virtual ~COcclusionCuller();

    protected:
        void initCuller(int, float *);
        int probeRect(int *xbound, int *ybound, int bl, int bt, int br, int bb, float zmin);

        void resetHierarchy(COcclusionNode *cNode = NULL);
        void initToZero();

        void touchNode(COcclusionNode *cNode, float z) {
            COcclusionNode *pNode;

            while (TRUE) {

                if ((pNode = cNode->parent) == NULL) {
                    *maxOpaqueDepth = cNode->zmax = z;
                    return;
                } else {

                    if (cNode->zmax == pNode->zmax) {
                        cNode->zmax = z;

                        z = max(max(pNode->children[0]->zmax, pNode->children[1]->zmax),
                                max(pNode->children[2]->zmax, pNode->children[3]->zmax));

                        if (z < pNode->zmax) {
                            cNode = pNode;
                        } else {
                            return;
                        }
                    } else {
                        cNode->zmax = z;
                        return;
                    }
                }
            }
        }

        COcclusionNode *getNode(int x, int y) {
            return nodes[(y << depth) + x];
        }

    private:
        COcclusionNode *root;
        int depth;
        int width;
        COcclusionNode **nodes;
        float *maxOpaqueDepth;

        COcclusionNode *newNode(COcclusionNode *, int w, int d, int nodeOffset, int x, int y);
};

#endif
