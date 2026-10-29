/**
 * Project: openRender
 *
 * File: textureInternal.h
 *
 * Description:
 *   Texturing-private (spec 027): the tile cache that texture.cpp used to keep in CRenderer statics,
 *   the per-thread data of a lookup, and the loaders the texture system calls. Only src/ri/texture
 *   includes this header.
 *
 *   The cache is behaviour-preserving: the doubly linked list of used blocks, the block-registry and
 *   eviction mutex, the per-thread budgets and reference numbers, and the LRU eviction
 *   (textureMemFlush: evict until a thread's usage is at most half its budget) are what they were.
 *   Every texture layer and deep-shadow tile gets a CTextureCache at construction, from the system
 *   that loads it.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef TEXTUREINTERNAL_H
#define TEXTUREINTERNAL_H

#include "common/memory.h"
#include "common/os.h"

class CTextureBlock; // defined in texture.cpp
class CTexture;
class CEnvironment;
class CTextureFileResolver;

// The data one thread keeps for a lookup (data-model.md section 6). Indexed by the shading context's thread
// until Phase 6 gives the lookups a TexturePerthread to carry.
struct TexturePerthread {
        int thread;         // 0 <= thread < numThreads
        int *refNumber;     // the thread's reference counter, one separate allocation per thread (as before)
        int usedMemory;     // bytes this thread has in memory
        int maxMemory;      // the thread's budget: ceil(total / numThreads)
        CMemPage *scratch;  // scratch memory for eviction (the system's own pages)
};

class CTextureCache {
    public:
        CTextureCache(int numThreads, int maxMemory);
        ~CTextureCache();

        int numThreads;
        CTextureBlock *usedBlocks;    // the list of blocks in the registry
        TMutex mutex;                 // guards the registry during eviction (was CRenderer::textureMutex)
        TexturePerthread *perthread;  // numThreads entries
};

// Loads a 2D texture from `name` (found through `resolver`), registering its blocks with `cache`.
// Returns NULL when the file cannot be found or decoded; the caller substitutes and reports.
CTexture *textureLoadTexture(const char *name, const CTextureFileResolver *resolver, CTextureCache *cache, bool diskCacheEnabled, const char *diskCacheDirectory);

// Loads an environment, shadow or deep shadow map. `toWorld` is the world transformation shadow maps need.
CEnvironment *textureLoadEnvironment(const char *name, const CTextureFileResolver *resolver, CTextureCache *cache, const float *toWorld);

#endif
