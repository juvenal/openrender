/**
 * Project: openRender
 *
 * File: textureSystem.h
 *
 * Description:
 *   The texture system (spec 027, contracts/texture-system.md): the per-frame owner of the texture
 *   cache. It hands out handles to textures, environments, shadow maps and texture-info records, owns
 *   the tile cache and the per-thread budgets behind them, and finds files through the resolver it
 *   was given. The renderer core constructs one per frame (CRenderer::textureSystem) and Shading
 *   reaches textures only through the renderer services' textureSystem().
 *
 *   Texturing depends on infrastructure alone: this header includes common/ headers and nothing else,
 *   and its aggregates are header-only, so Shading imports no Texturing symbol.
 *
 *   Phase 5 of spec 027 declares the handles, the settings and the system's identity (handle(),
 *   perthread(), describe()); the lookups join the interface in Phase 6.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef TEXTURESYSTEM_H
#define TEXTURESYSTEM_H

#include <stdint.h>

#include "common/algebra.h"      // matrix
#include "common/global.h"       // TRUE/FALSE
#include "common/os.h"           // TMutex, osLock/osUnlock
#include "common/riVocabulary.h" // RtFilterFunc
#include "common/sampling.h"     // Backend, SubstreamKey, get1D

class CTextureFileResolver;

struct TextureHandle;    // opaque: never defined outside Texturing
struct TexturePerthread; // opaque: never defined outside Texturing

enum class ETextureKind {
    Texture,     // a 2D texture (texture())
    Environment, // an environment, shadow or deep shadow map (environment(), shadow())
    Info         // texture-info only (textureinfo()): a miss is not an error and is not cached
};

// A position in a counter-based sub-stream (spec 039). Each draw takes the next dimension.
struct TextureSampleStream {
        openrender::sampling::Backend backend;
        openrender::sampling::SubstreamKey key;
        uint32_t dim;

        float next() { return openrender::sampling::get1D(backend, key, dim++, 0); }
        void next2(float *r) {
            r[0] = next();
            r[1] = next();
        }
        void next4(float *r) {
            for (int k = 0; k < 4; k++)
                r[k] = next();
        }
};

// The per-call options of a lookup (FR-003). Built by the caller, passed by reference; the
// lookup advances `stream`, and the caller reads the advanced cursor back (FR-005).
struct TextureOpt {
        RtFilterFunc filter;
        float blur;
        float width, swidth, twidth; // the caller's footprint; no lookup reads them
        float fill;
        float samples;      // 2D filter samples ("samples" of texture())
        float traceSamples; // "samples" of environment()/shadow() (CTraceLookup's block)
        float bias;         // "bias" of shadow()
        TextureSampleStream stream;
};

// What textureinfo() asks of a handle
struct TextureInfo {
        float resolution[2];
        const char *type;
        int numChannels;
        int hasViewMatrix;
        float viewMatrix[16];
        int hasProjectionMatrix;
        float projectionMatrix[16];
};

struct CTextureSystemSettings {
        int numThreads;
        int maxMemory;                        // bytes, split evenly per thread
        const CTextureFileResolver *resolver; // not owned, outlives the system
        bool diskCacheEnabled;
        const char *diskCacheDirectory;       // copied; nullptr = the resolved default
        matrix toWorld;                       // copied; for shadow and deep-shadow maps
        TMutex *loadMutex;                    // not owned; nullptr = the system's own
};

class CTextureSystem {
    public:
        virtual ~CTextureSystem() = default;

        // Handles (FR-002, FR-016). Thread-safe: one name is located, loaded and reported once.
        // Texture and Environment never return nullptr (a missing file yields a substitute after one
        // CODE_NOFILE error); Info returns nullptr for a missing file and caches nothing.
        virtual TextureHandle *handle(const char *name, ETextureKind kind) = 0;

        // Per-thread data (FR-004), 0 <= thread < numThreads.
        virtual TexturePerthread *perthread(int thread) = 0;

        // textureinfo(): zeroes `out`, then fills it from what the handle describes
        virtual void describe(TextureHandle *handle, TextureInfo &out) = 0;
};

// The system the renderer core constructs once per frame. nullptr (and CODE_BUG) on invalid settings.
CTextureSystem *createTextureSystem(const CTextureSystemSettings &settings);

#endif
