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

#include <algorithm>
#include <unordered_map>

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
    fixedIndexValid = false;
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

    // The frame's records join the tree before the file is written (spec 040)
    if (flags & CACHE_WRITE) {
        commitPending();
    }

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

    // Read samples (portable I/O - read fields individually instead of struct). The list keeps the order of the file (spec 040: it used to
    // be reversed on every read)
    cNode->samples = NULL;
    CCacheSample *tail = NULL;
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

        cSample->next = NULL;
        if (tail == NULL) {
            cNode->samples = cSample;
        }
        else {
            tail->next = cSample;
        }
        tail = cSample;
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
// Comments				:	Pc and Nc receive the query point and normal in the cache's space, which a miss hands to the sampler.
//						The accept value of a record is accept(acceptState, identity), a function of the call and the record, never of the walk
bool CIrradianceCache::lookupCached(float *C, const float *cP, const float *cN, float maxError, IrradianceAcceptFn accept, const void *acceptState, const CIrradianceBatchView &batch, float *Pc, float *Nc) {
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

    // One record's share of the blend: a cache sample or a record of the caller's batch (both have the fields of a CIrradianceRecord)
    auto consider = [&](const auto &s) {
        vector D;

        // D = vector from sample to query point
        subvv(D, P, s.P);

        // Ignore sample in the front
        float a = dotvv(D, s.N);
        if ((a * a / (dotvv(D, D) + C_EPSILON)) > 0.1)
            return;

        // Positional error
        float e1 = sqrtf(dotvv(D, D)) / s.dP;

        // Directional error
        float e2 = 1 - dotvv(N, s.N);
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
        if (w > accept(acceptState, openrender::sampling::giRecordIdentity(s.key, s.P, s.N)) * smallSampleWeight) {
            vector ntmp;

            crossvv(ntmp, s.N, N);

            // Sum the sample
            totalWeight += w;
            coverage += w * (s.coverage + dotvv(s.gP + 0 * 3, D) + dotvv(s.gR + 0 * 3, ntmp));
            irradiance[0] += w * (s.irradiance[0] + dotvv(s.gP + 1 * 3, D) + dotvv(s.gR + 1 * 3, ntmp));
            irradiance[1] += w * (s.irradiance[1] + dotvv(s.gP + 2 * 3, D) + dotvv(s.gR + 2 * 3, ntmp));
            irradiance[2] += w * (s.irradiance[2] + dotvv(s.gP + 3 * 3, D) + dotvv(s.gR + 3 * 3, ntmp));
            envdir[0] += w * (s.envdir[0] + dotvv(s.gP + 4 * 3, D) + dotvv(s.gR + 4 * 3, ntmp));
            envdir[1] += w * (s.envdir[1] + dotvv(s.gP + 5 * 3, D) + dotvv(s.gR + 5 * 3, ntmp));
            envdir[2] += w * (s.envdir[2] + dotvv(s.gP + 6 * 3, D) + dotvv(s.gR + 6 * 3, ntmp));
        }
    };

    // Prepare for the non recursive tree traversal
    stack = stackBase;
    *stack++ = root;
    while (stack > stackBase) {
        cNode = *(--stack);

        // Sum the values in this level
        for (cSample = cNode->samples; cSample != NULL; cSample = cSample->next) {
            consider(*cSample);
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

    // The records of the caller's batch, after the fixed part
    for (int b = 0; b < batch.count; b++) {
        consider(batch.records[b]);
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
// Class				:	CIrradianceCache::CPointIndex
// Description			:	A read-only kd-tree over points that carry a radius, answering "the smallest radius + distance"
// Comments				:	Used by the commit and by clampAgainstFixed(). The answer is an exact minimum of float expressions that are symmetric in
//							the two points, so it does not depend on the order the points were given in (data-model.md section 5). A node is pruned only
//							when it cannot lower the best value found.
class CIrradianceCache::CPointIndex {
    public:
        struct Item {
                float P[3];
                float value; // the radius the point carries
                unsigned int id;
        };

        void build(std::vector<Item> &&source) {
            items = std::move(source);
            nodes.clear();
            if (!items.empty()) {
                nodes.reserve(items.size() / 4 + 2);
                buildNode(0, (int)items.size());
            }
        }

        // min(best, min over the items j (j.id != skipId) of j.value + |P - j.P|), |.| = C_EPSILON for coincident points
        float query(const float *P, float best, unsigned int skipId = ~0u) const {
            if (!nodes.empty())
                visit(0, P, best, skipId);
            return best;
        }

        static float distance(const float *a, const float *b) {
            const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
            const float l = sqrtf(dx * dx + dy * dy + dz * dz);
            return (l > 0) ? l : (float)C_EPSILON; // coincident points: C_EPSILON, as clamp() did
        }

    private:
        struct Node {
                float bmin[3], bmax[3];
                float minValue;
                int left, right;   // children (-1: a leaf)
                int first, count;  // the items of a leaf
        };

        std::vector<Item> items;
        std::vector<Node> nodes;

        int buildNode(int first, int count) {
            const int self = (int)nodes.size();
            nodes.push_back(Node());
            {
                Node &n = nodes[(size_t)self];
                n.minValue = items[(size_t)first].value;
                for (int k = 0; k < 3; k++) {
                    n.bmin[k] = n.bmax[k] = items[(size_t)first].P[k];
                }
                for (int i = first; i < first + count; i++) {
                    const Item &it = items[(size_t)i];
                    n.minValue = std::min(n.minValue, it.value);
                    for (int k = 0; k < 3; k++) {
                        n.bmin[k] = std::min(n.bmin[k], it.P[k]);
                        n.bmax[k] = std::max(n.bmax[k], it.P[k]);
                    }
                }
                n.left = n.right = -1;
                n.first = first;
                n.count = count;
            }
            if (count > 8) {
                int axis = 0;
                const Node &n = nodes[(size_t)self];
                for (int k = 1; k < 3; k++) {
                    if (n.bmax[k] - n.bmin[k] > n.bmax[axis] - n.bmin[axis])
                        axis = k;
                }
                const int half = count / 2;
                std::nth_element(items.begin() + first, items.begin() + first + half, items.begin() + first + count, [axis](const Item &a, const Item &b) { return a.P[axis] < b.P[axis]; });
                const int l = buildNode(first, half);
                const int r = buildNode(first + half, count - half);
                nodes[(size_t)self].left = l;
                nodes[(size_t)self].right = r;
            }
            return self;
        }

        void visit(int n, const float *P, float &best, unsigned int skipId) const {
            const Node &node = nodes[(size_t)n];
            float d2 = 0;
            for (int k = 0; k < 3; k++) {
                float d = 0;
                if (P[k] < node.bmin[k])
                    d = node.bmin[k] - P[k];
                else if (P[k] > node.bmax[k])
                    d = P[k] - node.bmax[k];
                d2 += d * d;
            }
            // the lower bound of any value in this node (a hair below the true one, so that rounding never prunes a node that could win)
            if (node.minValue + sqrtf(d2) * 0.99999f >= best)
                return;
            if (node.left < 0) {
                for (int i = node.first; i < node.first + node.count; i++) {
                    const Item &it = items[(size_t)i];
                    if (it.id == skipId)
                        continue;
                    const float v = it.value + distance(P, it.P);
                    if (v < best)
                        best = v;
                }
                return;
            }
            visit(node.left, P, best, skipId);
            visit(node.right, P, best, skipId);
        }
};

// Every sample of the tree, in the traversal order of forEachNode, without committing (the commit and the index use it)
template <class F>
static void forEachTreeSample(CIrradianceCache::CCacheNode *root, F &&visit) {
    std::vector<CIrradianceCache::CCacheNode *> stack;
    stack.push_back(root);
    while (!stack.empty()) {
        CIrradianceCache::CCacheNode *cNode = stack.back();
        stack.pop_back();
        for (CIrradianceCache::CCacheSample *cSample = cNode->samples; cSample != NULL; cSample = cSample->next) {
            visit(cSample);
        }
        for (int i = 0; i < 8; i++) {
            if (cNode->children[i] != NULL) {
                stack.push_back(cNode->children[i]);
            }
        }
    }
}

// The bytes of a record the commit sorts by: the fields a file stores, in the order it stores them
static void recordBytes(const CIrradianceRecord &r, float *f) {
    memcpy(f, r.P, 3 * sizeof(float));
    memcpy(f + 3, r.N, 3 * sizeof(float));
    memcpy(f + 6, r.irradiance, 3 * sizeof(float));
    f[9] = r.coverage;
    memcpy(f + 10, r.envdir, 3 * sizeof(float));
    memcpy(f + 13, r.gP, 21 * sizeof(float));
    memcpy(f + 34, r.gR, 21 * sizeof(float));
    f[55] = r.dP;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	buildFixedIndex
// Description			:	Build the index of the fixed part
// Return Value			:	-
// Comments				:	Under the mutex, or single-threaded
void CIrradianceCache::buildFixedIndex() const {
    std::vector<CPointIndex::Item> items;
    unsigned int id = 0;
    forEachTreeSample(root, [&](CCacheSample *c) {
        CPointIndex::Item it;
        memcpy(it.P, c->P, sizeof(it.P));
        it.value = c->dP;
        it.id = id++;
        items.push_back(it);
    });
    std::shared_ptr<CPointIndex> index = std::make_shared<CPointIndex>();
    index->build(std::move(items));
    fixedIndex = index;
    fixedIndexValid = true;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	clampAgainstFixed
// Description			:	The radius of a record against the fixed part
// Return Value			:	min(raw dP, min over F (dP + l))
// Comments				:	F is read-only during a frame; the index is built once (here, under the lock, if nothing has built it yet)
float CIrradianceCache::clampAgainstFixed(const CIrradianceRecord &record) const {
    if (!fixedIndexValid) {
        osLock(const_cast<TMutex &>(mutex));
        if (!fixedIndexValid) {
            buildFixedIndex();
        }
        osUnlock(const_cast<TMutex &>(mutex));
    }
    return fixedIndex->query(record.P, record.dP);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	addPending
// Description			:	Add a record made this frame to the pending set
// Return Value			:	-
// Comments				:	The tree is not touched: the record joins it at the commit
void CIrradianceCache::addPending(const CIrradianceRecord &record, float maxError) {
    if (flags & CACHE_RDONLY) {
        return;
    }
    CPending p;
    p.record = record;
    p.maxError = maxError;

    osLock(mutex);
    pending.push_back(p);
    osUnlock(mutex);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	sortedPending
// Description			:	The pending set as the commit takes it
// Return Value			:	The records deduplicated and sorted by (key, bytes, maxError)
// Comments				:	A record with a key is one record per key (the smallest under the byte order stays); a record with key 0
//							(one a file would hold) is never merged with another: every one is kept
std::vector<CIrradianceCache::CPending> CIrradianceCache::sortedPending() const {
    std::vector<CPending> set = pending;
    std::sort(set.begin(), set.end(), [](const CPending &a, const CPending &b) {
        if (a.record.key != b.record.key)
            return a.record.key < b.record.key;
        float fa[56], fb[56];
        recordBytes(a.record, fa);
        recordBytes(b.record, fb);
        const int c = memcmp(fa, fb, sizeof(fa));
        if (c != 0)
            return c < 0;
        return memcmp(&a.maxError, &b.maxError, sizeof(float)) < 0;
    });

    std::vector<CPending> out;
    out.reserve(set.size());
    for (const CPending &p : set) {
        if (!out.empty() && p.record.key != 0 && out.back().record.key == p.record.key) {
            continue; // the same key: the first is the smallest. Key 0 is not a key: every key-0 record is kept
        }
        out.push_back(p);
    }
    return out;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	commitPending
// Description			:	The end-of-frame commit
// Return Value			:	-
// Comments				:	data-model.md section 5: dedupe, sort, clamp from the raw and stored radii only, place in sorted order after the fixed
//							records of each node. The result is a function of F and the pending set as sets
void CIrradianceCache::commitPending() {
    osLock(mutex);
    if (pending.empty()) {
        osUnlock(mutex);
        return;
    }

    const std::vector<CPending> set = sortedPending();
    pending.clear();
    const size_t n = set.size();

    if (!fixedIndexValid) {
        buildFixedIndex();
    }

    // The pending records, indexed by position, with their raw radii
    CPointIndex pendIndex;
    {
        std::vector<CPointIndex::Item> items;
        items.reserve(n);
        for (size_t i = 0; i < n; i++) {
            CPointIndex::Item it;
            memcpy(it.P, set[i].record.P, sizeof(it.P));
            it.value = set[i].record.dP;
            it.id = (unsigned int)i;
            items.push_back(it);
        }
        pendIndex.build(std::move(items));
    }

    // New record i: min(raw_i, min over F (dP_j + l), min over the other pending (raw_j + l)); every term is a raw or stored radius
    std::vector<float> radius(n);
    for (size_t i = 0; i < n; i++) {
        float r = fixedIndex->query(set[i].record.P, set[i].record.dP);
        r = pendIndex.query(set[i].record.P, r, (unsigned int)i);
        radius[i] = r;
    }

    // Fixed record k: min(dP_k, min over the pending (raw_j + l)); read from the stored radii, applied once all are read
    {
        std::vector<std::pair<CCacheSample *, float>> lowered;
        forEachTreeSample(root, [&](CCacheSample *c) {
            const float r = pendIndex.query(c->P, c->dP);
            if (r < c->dP) {
                lowered.push_back(std::make_pair(c, r));
            }
        });
        for (const auto &l : lowered) {
            l.first->dP = l.second;
        }
    }

    // Place the new records in sorted order, at the tail of their node's list (after the fixed records)
    std::unordered_map<CCacheNode *, CCacheSample *> tails;
    for (size_t i = 0; i < n; i++) {
        CCacheSample *cSample = (CCacheSample *)memory->alloc(sizeof(CCacheSample));
        cSample->set(set[i].record);
        cSample->dP = radius[i];
        cSample->key = 0; // a committed record is what a file holds: no key, so that the tree answers as the file read back does (SC-005, FR-014)
        cSample->next = NULL;

        const float K = 0.4f / set[i].maxError;
        const float rMean = cSample->dP / K;
        const float *P = cSample->P;
        CCacheNode *cNode = root;
        int depth = 0;
        while (cNode->side > (2 * rMean)) {
            int j = 0;
            depth++;

            for (int k = 0; k < 3; k++) {
                if (P[k] > cNode->center[k]) {
                    j |= 1 << k;
                }
            }

            if (cNode->children[j] == NULL) {
                CCacheNode *nNode = (CCacheNode *)memory->alloc(sizeof(CCacheNode));

                for (int k = 0; k < 3; k++) {
                    if (P[k] > cNode->center[k]) {
                        nNode->center[k] = cNode->center[k] + cNode->side * 0.25f;
                    }
                    else {
                        nNode->center[k] = cNode->center[k] - cNode->side * 0.25f;
                    }
                }

                cNode->children[j] = nNode;
                nNode->side = cNode->side * 0.5f;
                nNode->samples = NULL;
                for (int k = 0; k < 8; k++)
                    nNode->children[k] = NULL;
            }

            cNode = cNode->children[j];
        }

        auto t = tails.find(cNode);
        if (t == tails.end()) {
            CCacheSample *last = cNode->samples;
            while (last != NULL && last->next != NULL) {
                last = last->next;
            }
            t = tails.insert(std::make_pair(cNode, last)).first;
        }
        if (t->second == NULL) {
            cNode->samples = cSample;
        }
        else {
            t->second->next = cSample;
        }
        t->second = cSample;

        if (depth > maxDepth) {
            maxDepth = depth;
        }
    }

    invalidateFixedIndex();
    osUnlock(mutex);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Method				:	receivePending
// Description			:	Add a record a remote server made to the pending set
// Return Value			:	-
// Comments				:	The same as addPending(): the client commits the servers' records with its own fixed part and each record's own K
void CIrradianceCache::receivePending(const CCacheSample &sample, float maxError) {
    CIrradianceRecord record;
    sample.get(record);
    addPending(record, maxError);
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
