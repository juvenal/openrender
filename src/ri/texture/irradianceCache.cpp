/**
 * Project: openRender
 *
 * File: irradianceCache.cpp
 *
 * Description:
 *   This file implements the functionality for the irradiance cache (passive storage, spec 032).
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
//  File				:	irradianceCache.cpp
//  Classes				:	CIrradianceCache
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "common/portable_io.h"
#include "common/diagnostics.h"
#include "irradianceCache.h"
#include "common/containers.h"
#include "common/memory.h"
#include "ri_config.h"
#include "common/stats.h"

const float weightNormalDenominator = (float)(1 / (1 - cos(radians(10))));

// A sample is the record's fields and a list link (it goes on the wire as raw bytes)
static_assert(offsetof(CIrradianceCache::CCacheSample, next) == sizeof(CIrradianceRecord), "CCacheSample is a CIrradianceRecord and its list link");

///////////////////////////////////////////////////////////////////////
// Function				:	transformWorldBound
// Description			:	Transform a bounding box and bound the result (the corner order and arithmetic of the Graphics State's
//							transformBound(), which Texturing does not reach)
// Return Value			:	-
// Comments				:
static void transformWorldBound(float *lbmin, float *lbmax, const float *to, const float *bmin, const float *bmax) {
    vector corners[8];
    int i;
    vector vtmp;

    initv(vtmp, bmin[COMP_X], bmin[COMP_Y], bmin[COMP_Z]);
    mulmp(corners[0], to, vtmp);

    initv(vtmp, bmin[COMP_X], bmin[COMP_Y], bmax[COMP_Z]);
    mulmp(corners[1], to, vtmp);

    initv(vtmp, bmin[COMP_X], bmax[COMP_Y], bmax[COMP_Z]);
    mulmp(corners[2], to, vtmp);

    initv(vtmp, bmin[COMP_X], bmax[COMP_Y], bmin[COMP_Z]);
    mulmp(corners[3], to, vtmp);

    initv(vtmp, bmax[COMP_X], bmin[COMP_Y], bmin[COMP_Z]);
    mulmp(corners[4], to, vtmp);

    initv(vtmp, bmax[COMP_X], bmin[COMP_Y], bmax[COMP_Z]);
    mulmp(corners[5], to, vtmp);

    initv(vtmp, bmax[COMP_X], bmax[COMP_Y], bmax[COMP_Z]);
    mulmp(corners[6], to, vtmp);

    initv(vtmp, bmax[COMP_X], bmax[COMP_Y], bmin[COMP_Z]);
    mulmp(corners[7], to, vtmp);

    movvv(lbmin, corners[0]);
    movvv(lbmax, corners[0]);

    for (i = 1; i < 8; i++) {
        addBox(lbmin, lbmax, corners[i]);
    }
}

///////////////////////////////////////////////////////////////////////
//
//
//		Irradiance Cache Implementation
//
//
///////////////////////////////////////////////////////////////////////

CTexture3d::CChannel CIrradianceCache::cacheChannels[3] = {
    {"irradiance", 3, 0, NULL, TYPE_COLOR},
    {"occlusion", 1, 3, NULL, TYPE_FLOAT},
    {"environmentdir", 3, 4, NULL, TYPE_VECTOR}};

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	CIrradianceCache
// Description			:	Ctor
// Return Value			:
// Comments				:
CIrradianceCache::CIrradianceCache(const char *name, unsigned int f, FILE *in, const float *from, const float *to, const float *tondc, const float *worldBmin, const float *worldBmax) : CIrradianceStore(name, from, to, tondc, 3, cacheChannels) {
    int i;

    assert(dataSize == 7);
    assert(numChannels() == 3);

    memory = new CMemStack; // Where we allocate our memory from
    root = NULL;
    maxDepth = 1;
    flags = f;
    osCreateMutex(mutex);

    // Are we reading from file ?
    if (flags & CACHE_READ) {
        if (in == NULL)
            in = ropen(name, "rb", fileIrradianceCache);

        if (in != NULL) {

            // Read the samples
            if (fread(&maxDepth, sizeof(int), 1, in) != 1) { /* read error */
            }
            root = readNode(in);

            // Close the file
            fclose(in);
        }
    }

    // Are we creating a fresh cache ?
    if (root == NULL) {
        vector center, bmin, bmax;

        // Transform the bounding box to the world
        transformWorldBound(bmin, bmax, to, worldBmin, worldBmax);

        root = (CCacheNode *)memory->alloc(sizeof(CCacheNode));
        for (i = 0; i < 8; i++)
            root->children[i] = NULL;
        addvv(center, bmin, bmax);
        mulvf(center, 0.5f);
        movvv(root->center, center);
        subvv(bmax, bmin);
        float maxBmax01;
        if (bmax[1] > bmax[0]) {
            maxBmax01 = bmax[1];
        }
        else {
            maxBmax01 = bmax[0];
        }
        float maxBmax012;
        if (bmax[2] > maxBmax01) {
            maxBmax012 = bmax[2];
        }
        else {
            maxBmax012 = maxBmax01;
        }
        root->side = maxBmax012;
        root->samples = NULL;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	~CIrradianceCache
// Description			:	Dtor
// Return Value			:
// Comments				:
CIrradianceCache::~CIrradianceCache() {

    osDeleteMutex(mutex);

    // Are we writing the file out ?
    if (flags & CACHE_WRITE) {
        if (name[0] != 0) {
            FILE *out = ropen(name, "wb", fileIrradianceCache);

            if (out != NULL) {

                // Write the samples
                fwrite(&maxDepth, sizeof(int), 1, out);
                writeNode(out, root);

                fclose(out);
            }
        }
    }

    // Delete the memory pool
    delete memory;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	writeNode
// Description			:	Write a node into a file
// Return Value			:
// Comments				:
void CIrradianceCache::writeNode(FILE *out, CCacheNode *cNode) {
    int32_p numSamples;
    int i;
    CCacheSample *cSample;

    // Write node geometry (portable I/O - Phase 2)
    if (!writeVector(out, cNode->center, 3)) {
        error(CODE_SYSTEM, "Failed to write irradiance cache node center\n");
        return;
    }
    if (!writeFloat32(out, static_cast<float32_p>(cNode->side))) {
        error(CODE_SYSTEM, "Failed to write irradiance cache node side\n");
        return;
    }

    // Count samples
    for (cSample = cNode->samples, numSamples = 0; cSample != NULL; cSample = cSample->next, numSamples++)
        ;

    if (!writeInt32(out, numSamples)) {
        error(CODE_SYSTEM, "Failed to write irradiance cache num samples\n");
        return;
    }

    // Write each sample (portable I/O - write fields individually instead of struct)
    for (cSample = cNode->samples; cSample != NULL; cSample = cSample->next) {
        bool sampleWriteSuccess = true;
        sampleWriteSuccess = sampleWriteSuccess && writeVector(out, cSample->P, 3);
        sampleWriteSuccess = sampleWriteSuccess && writeVector(out, cSample->N, 3);
        sampleWriteSuccess = sampleWriteSuccess && writeVector(out, cSample->irradiance, 3);
        sampleWriteSuccess = sampleWriteSuccess && writeFloat32(out, static_cast<float32_p>(cSample->coverage));
        sampleWriteSuccess = sampleWriteSuccess && writeVector(out, cSample->envdir, 3);
        sampleWriteSuccess = sampleWriteSuccess && writeFloat32Array(out, cSample->gP, 21); // Translational gradient
        sampleWriteSuccess = sampleWriteSuccess && writeFloat32Array(out, cSample->gR, 21); // Rotational gradient
        sampleWriteSuccess = sampleWriteSuccess && writeFloat32(out, static_cast<float32_p>(cSample->dP));

        if (!sampleWriteSuccess) {
            error(CODE_SYSTEM, "Failed to write irradiance cache sample data\n");
            return;
        }
    }

    // Write children markers (portable I/O - Phase 2)
    // Instead of writing pointer array, write markers indicating presence
    for (i = 0; i < 8; i++) {
        int32_p hasChild = (cNode->children[i] != NULL) ? 1 : 0;
        if (!writeInt32(out, hasChild)) {
            error(CODE_SYSTEM, "Failed to write irradiance cache child marker\n");
            return;
        }
    }

    // Recursively write child nodes
    for (i = 0; i < 8; i++) {
        if (cNode->children[i] != NULL)
            writeNode(out, cNode->children[i]);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	readNode
// Description			:	Read a node from file
// Return Value			:
// Comments				:
CIrradianceCache::CCacheNode *CIrradianceCache::readNode(FILE *in) {
    int32_p numSamples_i;
    int i;
    CCacheNode *cNode = (CCacheNode *)memory->alloc(sizeof(CCacheNode));

    // Read node geometry (portable I/O - Phase 2)
    if (!readVector(in, cNode->center, 3)) {
        error(CODE_SYSTEM, "Failed to read irradiance cache node center\n");
        return NULL;
    }

    float32_p side_f;
    if (!readFloat32(in, side_f)) {
        error(CODE_SYSTEM, "Failed to read irradiance cache node side\n");
        return NULL;
    }
    cNode->side = side_f;

    if (!readInt32(in, numSamples_i)) {
        error(CODE_SYSTEM, "Failed to read irradiance cache num samples\n");
        return NULL;
    }

    // Read samples (portable I/O - read fields individually instead of struct)
    cNode->samples = NULL;
    for (int numSamples = numSamples_i; numSamples > 0; numSamples--) {
        CCacheSample *cSample = (CCacheSample *)memory->alloc(sizeof(CCacheSample));

        bool sampleReadSuccess = true;
        sampleReadSuccess = sampleReadSuccess && readVector(in, cSample->P, 3);
        sampleReadSuccess = sampleReadSuccess && readVector(in, cSample->N, 3);
        sampleReadSuccess = sampleReadSuccess && readVector(in, cSample->irradiance, 3);

        float32_p coverage_f = 0;
        sampleReadSuccess = sampleReadSuccess && readFloat32(in, coverage_f);
        cSample->coverage = coverage_f;

        sampleReadSuccess = sampleReadSuccess && readVector(in, cSample->envdir, 3);
        sampleReadSuccess = sampleReadSuccess && readFloat32Array(in, cSample->gP, 21); // Translational gradient
        sampleReadSuccess = sampleReadSuccess && readFloat32Array(in, cSample->gR, 21); // Rotational gradient

        float32_p dP_f = 0;
        sampleReadSuccess = sampleReadSuccess && readFloat32(in, dP_f);
        cSample->dP = dP_f;
        cSample->key = 0; // no file carries a key

        if (!sampleReadSuccess) {
            error(CODE_SYSTEM, "Failed to read irradiance cache sample data\n");
            return NULL;
        }

        cSample->next = cNode->samples;
        cNode->samples = cSample;
    }

    // Read children markers (portable I/O - Phase 2)
    // Instead of reading pointer array, read markers and reconstruct
    for (i = 0; i < 8; i++) {
        int32_p hasChild;
        if (!readInt32(in, hasChild)) {
            error(CODE_SYSTEM, "Failed to read irradiance cache child marker\n");
            return NULL;
        }
        cNode->children[i] = hasChild ? (CCacheNode *)1 : NULL; // Temporary marker
    }

    // Recursively read child nodes
    for (i = 0; i < 8; i++) {
        if (cNode->children[i] != NULL)
            cNode->children[i] = readNode(in);
    }

    return cNode;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	lookupCached
// Description			:	Lookup da cache
// Return Value			:	TRUE on a hit (C is written); FALSE on a miss (C is left alone)
// Comments				:	Pc and Nc receive the query point and normal in the cache's space, which a miss hands to the sampler
bool CIrradianceCache::lookupCached(float *C, const float *cP, const float *cN, float maxError, IrradianceDrawFn draw, void *drawState, float *Pc, float *Nc) {
    CCacheSample *cSample;
    CCacheNode *cNode;
    float totalWeight = 0;
    CCacheNode **stackBase = (CCacheNode **)alloca(maxDepth * sizeof(CCacheNode *) * 8);
    CCacheNode **stack;
    int i;
    float coverage;
    vector irradiance, envdir;
    vector P, N;

    // A small value for discard-smoothing of irradiance
    const float smallSampleWeight = (flags & CACHE_SAMPLE) ? 0.1f : 0.0f;

    // Transform the lookup point to the correct coordinate system
    mulmp(P, to, cP);
    mulmn(N, from, cN);
    movvv(Pc, P);
    movvv(Nc, N);

    // Init the result
    coverage = 0;
    initv(irradiance, 0);
    initv(envdir, 0);

    // The weighting algorithm is that described in [Tabellion and Lamorlette 2004]
    // We need to convert the max error as in Wald to Tabellion
    // The default value of maxError is 0.4f
    const float K = 0.4f / maxError;

    // Note, we do not need to lock the data for reading
    // if word-writes are atomic

    // Prepare for the non recursive tree traversal
    stack = stackBase;
    *stack++ = root;
    while (stack > stackBase) {
        cNode = *(--stack);

        // Sum the values in this level
        for (cSample = cNode->samples; cSample != NULL; cSample = cSample->next) {
            vector D;

            // D = vector from sample to query point
            subvv(D, P, cSample->P);

            // Ignore sample in the front
            float a = dotvv(D, cSample->N);
            if ((a * a / (dotvv(D, D) + C_EPSILON)) > 0.1)
                continue;

            // Positional error
            float e1 = sqrtf(dotvv(D, D)) / cSample->dP;

            // Directional error
            float e2 = 1 - dotvv(N, cSample->N);
            if (e2 < 0)
                e2 = 0;
            e2 = sqrtf(e2 * weightNormalDenominator);

            // Compute the weight
            float maxE1E2;
            if (e2 > e1) {
                maxE1E2 = e2;
            }
            else {
                maxE1E2 = e1;
            }
            float w = 1 - K * maxE1E2;
            if (w > draw(drawState) * smallSampleWeight) {
                vector ntmp;

                crossvv(ntmp, cSample->N, N);

                // Sum the sample
                totalWeight += w;
                coverage += w * (cSample->coverage + dotvv(cSample->gP + 0 * 3, D) + dotvv(cSample->gR + 0 * 3, ntmp));
                irradiance[0] += w * (cSample->irradiance[0] + dotvv(cSample->gP + 1 * 3, D) + dotvv(cSample->gR + 1 * 3, ntmp));
                irradiance[1] += w * (cSample->irradiance[1] + dotvv(cSample->gP + 2 * 3, D) + dotvv(cSample->gR + 2 * 3, ntmp));
                irradiance[2] += w * (cSample->irradiance[2] + dotvv(cSample->gP + 3 * 3, D) + dotvv(cSample->gR + 3 * 3, ntmp));
                envdir[0] += w * (cSample->envdir[0] + dotvv(cSample->gP + 4 * 3, D) + dotvv(cSample->gR + 4 * 3, ntmp));
                envdir[1] += w * (cSample->envdir[1] + dotvv(cSample->gP + 5 * 3, D) + dotvv(cSample->gR + 5 * 3, ntmp));
                envdir[2] += w * (cSample->envdir[2] + dotvv(cSample->gP + 6 * 3, D) + dotvv(cSample->gR + 6 * 3, ntmp));
            }
        }

        // Check the children
        for (i = 0; i < 8; i++) {
            CCacheNode *tNode;

            if ((tNode = cNode->children[i]) != NULL) {
                const float tSide = tNode->side;

                if (((tNode->center[0] + tSide) > P[0]) &&
                    ((tNode->center[1] + tSide) > P[1]) &&
                    ((tNode->center[2] + tSide) > P[2]) &&
                    ((tNode->center[0] - tSide) < P[0]) &&
                    ((tNode->center[1] - tSide) < P[1]) &&
                    ((tNode->center[2] - tSide) < P[2])) {
                    *stack++ = tNode;
                }
            }
        }
    }

    // Do we have anything ?
    if (totalWeight > C_EPSILON) {
        double normalizer = 1 / totalWeight;

        normalizevf(envdir);

        C[0] = (float)(irradiance[0] * normalizer);
        C[1] = (float)(irradiance[1] * normalizer);
        C[2] = (float)(irradiance[2] * normalizer);
        C[3] = (float)(coverage * normalizer);
        mulmv(C + 4, from, envdir); // envdir is stored in the target coordinate system
        return true;
    }

    return false;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	insertRecord
// Description			:	Add a record the caller computed
// Return Value			:	-
// Comments				:	Everything that changes the tree happens under the cache's lock
void CIrradianceCache::insertRecord(const CIrradianceRecord &record, float maxError) {
    CCacheNode *cNode;
    CCacheSample *cSample;
    int depth;
    int i, j;

    // We're modifying, lock the thing
    osLock(mutex);

    // Create the sample
    cSample = (CCacheSample *)memory->alloc(sizeof(CCacheSample));

    // Record the data (in the target coordinate system)
    cSample->set(record);

    // Do the neighbour clamping trick
    clamp(cSample);
    float rMean = cSample->dP; // copy dP back so we get the right place in the octree

    // The error multiplier
    const float K = 0.4f / maxError;
    rMean /= K;

    // Insert the new sample into the cache
    const float *P = cSample->P;
    cNode = root;
    depth = 0;
    while (cNode->side > (2 * rMean)) {
        depth++;

        for (j = 0, i = 0; i < 3; i++) {
            if (P[i] > cNode->center[i]) {
                j |= 1 << i;
            }
        }

        if (cNode->children[j] == NULL) {
            CCacheNode *nNode = (CCacheNode *)memory->alloc(sizeof(CCacheNode));

            for (i = 0; i < 3; i++) {
                if (P[i] > cNode->center[i]) {
                    nNode->center[i] = cNode->center[i] + cNode->side * 0.25f;
                }
                else {
                    nNode->center[i] = cNode->center[i] - cNode->side * 0.25f;
                }
            }

            cNode->children[j] = nNode;
            nNode->side = cNode->side * 0.5f;
            nNode->samples = NULL;
            for (i = 0; i < 8; i++)
                nNode->children[i] = NULL;
        }

        cNode = cNode->children[j];
    }

    cSample->next = cNode->samples;
    cNode->samples = cSample;
    if (depth > maxDepth) {
        maxDepth = depth;
    }

    osUnlock(mutex);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	clamp
// Description			:	Clamp the radius
// Return Value			:
// Comments				:
void CIrradianceCache::clamp(CCacheSample *nSample) {
    CCacheSample *cSample;
    CCacheNode *cNode;
    CCacheNode **stackBase = (CCacheNode **)alloca(maxDepth * sizeof(CCacheNode *) * 8);
    CCacheNode **stack = stackBase;
    int i;

    *stack++ = root;
    while (stack > stackBase) {
        cNode = *(--stack);

        // Sum the values in this level
        for (cSample = cNode->samples; cSample != NULL; cSample = cSample->next) {
            vector D;

            subvv(D, cSample->P, nSample->P);
            // const float	l	=	lengthv(D);
            //  Avoid issues with coincident points
            const float l = (dotvv(D, D) > C_EPSILON) ? lengthv(D) : C_EPSILON;

            float newDP = cSample->dP + l;
            if (newDP < nSample->dP) {
                nSample->dP = newDP;
            }
            float newDP2 = nSample->dP + l;
            if (newDP2 < cSample->dP) {
                cSample->dP = newDP2;
            }
        }

        // Check the children
        for (i = 0; i < 8; i++) {
            CCacheNode *tNode;

            if ((tNode = cNode->children[i]) != NULL) {
                const float tSide = tNode->side * 4;

                if (((tNode->center[0] + tSide) > nSample->P[0]) &&
                    ((tNode->center[1] + tSide) > nSample->P[1]) &&
                    ((tNode->center[2] + tSide) > nSample->P[2]) &&
                    ((tNode->center[0] - tSide) < nSample->P[0]) &&
                    ((tNode->center[1] - tSide) < nSample->P[1]) &&
                    ((tNode->center[2] - tSide) < nSample->P[2])) {
                    *stack++ = tNode;
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	insertReceived
// Description			:	Place a record a remote server sent
// Return Value			:	-
// Comments				:	The record is copied into the cache's memory and placed by its own dP, with no clamp(), no
//							K = 0.4 / maxError and no lock: what the remote channel has always done on receipt
void CIrradianceCache::insertReceived(const CCacheSample &record) {
    CCacheSample *cSample = (CCacheSample *)memory->alloc(sizeof(CCacheSample));
    *cSample = record;
    cSample->next = NULL;

    const float rMean = cSample->dP; // error adjustment!!
    vector P;
    movvv(P, cSample->P);

    CCacheNode *cNode = root;
    int depth = 0;
    while (cNode->side > (2 * rMean)) {
        int i, j;
        depth++;

        for (j = 0, i = 0; i < 3; i++) {
            if (P[i] > cNode->center[i]) {
                j |= 1 << i;
            }
        }

        if (cNode->children[j] == NULL) {
            CCacheNode *nNode = (CCacheNode *)memory->alloc(sizeof(CCacheNode));

            for (i = 0; i < 3; i++) {
                if (P[i] > cNode->center[i]) {
                    nNode->center[i] = cNode->center[i] + cNode->side / (float)4;
                }
                else {
                    nNode->center[i] = cNode->center[i] - cNode->side / (float)4;
                }
            }

            cNode->children[j] = nNode;
            nNode->side = cNode->side * 0.5f;
            nNode->samples = NULL;
            for (i = 0; i < 8; i++)
                nNode->children[i] = NULL;
        }

        cNode = cNode->children[j];
    }

    cSample->next = cNode->samples;
    cNode->samples = cSample;
    if (depth > maxDepth) {
        maxDepth = depth;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	bound
// Description			:	Bound the irradiance cache
// Return Value			:
// Comments				:
void CIrradianceCache::bound(float *bmin, float *bmax) {
    assert(root != NULL);

    bmin[0] = root->center[0] - root->side * 0.5f;
    bmin[1] = root->center[1] - root->side * 0.5f;
    bmin[2] = root->center[2] - root->side * 0.5f;

    bmax[0] = root->center[0] + root->side * 0.5f;
    bmax[1] = root->center[1] + root->side * 0.5f;
    bmax[2] = root->center[2] + root->side * 0.5f;
}
