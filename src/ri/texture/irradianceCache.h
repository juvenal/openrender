/**
 * Project: openRender
 *
 * File: irradianceCache.h
 *
 * Description:
 *   This file defines the interface for the irradiance cache: passive storage (spec 032). It holds records, answers a
 *   lookup from them and takes a record the caller computed; the sampler that computes one lives in Hiders.
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
//  File				:	irradianceCache.h
//  Classes				:	CIrradianceCache
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef IRRADIANCECACHE_H
#define IRRADIANCECACHE_H

#include <vector>

#include "common/algebra.h"
#include "common/global.h"
#include "common/os.h"
#include "irradianceStore.h"
#include "texture3d.h"

// Forward declarations
class CMemStack;

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Description			:	Encapsulates an irradiance cache
// Comments				:
class CIrradianceCache : public CIrradianceStore {
    public:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	CIrradiance
        // Description			:	Holds irradiance information on a surface
        // Comments				:
        class CCacheSample {
            public:
                vector P;          // Point
                vector N;          // Normal
                vector irradiance; // Irradiance
                float coverage;    // Coverage
                vector envdir;     // The unoccluded direction

                float gP[7 * 3]; // The translational gradient
                float gR[7 * 3]; // The rotational gradient

                float dP;           // Mean radius
                CCacheSample *next; // Next irradiance in the list

                // The record this sample holds (a copy), and the other way. A sample is the record's fields in the same
                // order and a list link, so that it goes on the wire as the raw bytes it always has (remoteChannel.cpp)
                void get(CIrradianceRecord &record) const {
                    movvv(record.P, P);
                    movvv(record.N, N);
                    movvv(record.irradiance, irradiance);
                    record.coverage = coverage;
                    movvv(record.envdir, envdir);
                    for (int i = 0; i < 7 * 3; i++) {
                        record.gP[i] = gP[i];
                        record.gR[i] = gR[i];
                    }
                    record.dP = dP;
                }
                void set(const CIrradianceRecord &record) {
                    movvv(P, record.P);
                    movvv(N, record.N);
                    movvv(irradiance, record.irradiance);
                    coverage = record.coverage;
                    movvv(envdir, record.envdir);
                    for (int i = 0; i < 7 * 3; i++) {
                        gP[i] = record.gP[i];
                        gR[i] = record.gR[i];
                    }
                    dP = record.dP;
                }
        };

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CIrradianceNode
        // Description			:	Holds information about incident illumination
        // Comments				:
        class CCacheNode {
            public:
                CCacheNode(const float *);
                CCacheNode(FILE *);
                ~CCacheNode();

                void write(FILE *);

                CCacheSample *samples;   // Linked list of samples
                CCacheNode *children[8]; // Pointers to the children
                vector center;           // The center of the node
                float side;              // The side length of the node
        };

    public:
        // worldBmin/worldBmax: the world's bound, from which a fresh cache sizes its root (the caller reads it from the renderer)
        CIrradianceCache(const char *name, unsigned int flags, FILE *in, const float *from, const float *to, const float *tondc, const float *worldBmin, const float *worldBmax);
        ~CIrradianceCache();

        void lookup(float *, const float *, const float *, float) { assert(FALSE); }
        void store(const float *, const float *, const float *, float) { assert(FALSE); }
        void lookupPointBased(float *, const float *, const float *, const float *, const float *, const CPointBasedLookup &) { assert(FALSE); }

        // The CIrradianceStore interface
        bool lookupCached(float *C, const float *P, const float *N, float maxError, IrradianceDrawFn draw, void *drawState, float *Pc, float *Nc);
        void insertRecord(const CIrradianceRecord &record, float maxError);
        bool samplesOnMiss() const { return (flags & CACHE_SAMPLE) != 0; }

        const char *typeName() const { return "Irradiance Cache"; }

        void bound(float *bmin, float *bmax);

        // The read interface of the data viewer and the remote channel
        ///////////////////////////////////////////////////////////////////////
        // Method				:	forEachNode
        // Description			:	Visit the sample list of every node, in the tree's stack order
        // Comments				:	The root first; the children of a node are pushed 0 to 7 and popped last in, first out
        //							(the order the remote channel sends them). visit(const CCacheSample *head): the head of the
        //							node's list, which may be NULL.
        template <class F>
        void forEachNode(F &&visit) const {
            std::vector<const CCacheNode *> stack;
            stack.push_back(root);
            while (!stack.empty()) {
                const CCacheNode *cNode = stack.back();
                stack.pop_back();

                visit((const CCacheSample *)cNode->samples);

                for (int i = 0; i < 8; i++) {
                    const CCacheNode *tNode;
                    if ((tNode = cNode->children[i]) != NULL) {
                        stack.push_back(tNode);
                    }
                }
            }
        }

        // Visit every record: forEachNode's order, each node's list from its head. visit(const CIrradianceRecord &): a copy
        // of the record, valid for the call.
        template <class F>
        void forEachRecord(F &&visit) const {
            forEachNode([&](const CCacheSample *head) {
                for (const CCacheSample *cSample = head; cSample != NULL; cSample = cSample->next) {
                    CIrradianceRecord record;
                    cSample->get(record);
                    visit(static_cast<const CIrradianceRecord &>(record));
                }
            });
        }

        const float *rootCenter() const { return root->center; }
        float rootSide() const { return root->side; }
        void setRoot(const float *center, float side) {
            movvv(root->center, center);
            root->side = side;
        }
        int treeDepth() const { return maxDepth; } // the depth of the deepest node (the member of that name is private)

        // Place a record a remote server sent (a copy; placed by its own dP, with no clamp, no K and no lock)
        void insertReceived(const CCacheSample &record);

    private:
        void writeNode(FILE *, CCacheNode *);
        CCacheNode *readNode(FILE *);

        void clamp(CCacheSample *);

        CMemStack *memory;

        CCacheNode *root;
        int maxDepth;
        int flags;

        TMutex mutex;

        static CChannel cacheChannels[3];
};

#endif
