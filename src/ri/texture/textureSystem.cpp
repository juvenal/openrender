/**
 * Project: openRender
 *
 * File: textureSystem.cpp
 *
 * Description:
 *   The texture system (spec 027): CTextureSystemImpl owns the texture-family cache (textures,
 *   environments, shadow and deep shadow maps, texture-info records: one entry per name), the tile
 *   cache and the per-thread data behind it, and finds files through the resolver it was given.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "textureSystem.h"

#include <string.h>
#include <string>

#include "common/containers.h"
#include "common/diagnostics.h"
#include "common/fileResource.h"
#include "texture.h"
#include "textureInternal.h"
#include "textureResolver.h"

namespace {

    // The handle behind a TextureHandle: the cached CTextureInfoBase (the base of CTexture and CEnvironment)
    inline TextureHandle *toHandle(CFileResource *resource) {
        return reinterpret_cast<TextureHandle *>(static_cast<CTextureInfoBase *>(resource));
    }

    inline CTextureInfoBase *fromHandle(TextureHandle *handle) {
        return reinterpret_cast<CTextureInfoBase *>(handle);
    }

    // Holds a mutex for the length of a scope
    class CScopedLock {
        public:
            explicit CScopedLock(TMutex &m) : mutex(m) { osLock(mutex); }
            ~CScopedLock() { osUnlock(mutex); }

        private:
            TMutex &mutex;
    };

    ///////////////////////////////////////////////////////////////////////
    // Class				:	CTextureSystemImpl
    // Description			:	The texture system
    // Comments				:
    class CTextureSystemImpl : public CTextureSystem {
        public:
            explicit CTextureSystemImpl(const CTextureSystemSettings &s) : settings(s), cache(s.numThreads, s.maxMemory) {
                if (s.diskCacheDirectory != NULL) {
                    cacheDirectory = s.diskCacheDirectory;
                    settings.diskCacheDirectory = cacheDirectory.c_str();
                }
                family = new CTrie<CFileResource *>;

                // The load lock: the creator's when it supplies one, else our own
                osCreateMutex(ownMutex);
                loadMutex = (s.loadMutex != NULL) ? s.loadMutex : &ownMutex;
            }

            ~CTextureSystemImpl() override {
                // Delete the textures first (they unregister their blocks), then the cache goes
                family->destroy();
                osDeleteMutex(ownMutex);
            }

            TextureHandle *handle(const char *name, ETextureKind kind) override {
                CFileResource *tex;

                // One lock across find -> load -> insert, so a name is located and loaded once and a missing
                // name is reported once, however many threads ask for it first (FR-016, GitHub #20)
                CScopedLock lock(*loadMutex);

                if (family->find(name, tex) == FALSE) {
                    switch (kind) {
                        case ETextureKind::Texture:
                            tex = textureLoadTexture(name, settings.resolver, &cache, settings.diskCacheEnabled, settings.diskCacheDirectory);

                            if (tex == NULL) {
                                // Not found, substitude with a dummy one
                                if (*name != 0)
                                    error(CODE_NOFILE, "Failed open texture \"%s\"\n", name);
                                tex = new CDummyTexture(name);
                            }

                            family->insert(tex->name, tex);
                            break;

                        case ETextureKind::Environment:
                            tex = textureLoadEnvironment(name, settings.resolver, &cache, settings.toWorld);

                            if (tex == NULL) {
                                // Not found, substitude with a dummy one
                                error(CODE_NOFILE, "Failed open environment \"%s\"\n", name);
                                tex = new CDummyEnvironment(name);
                            }

                            family->insert(tex->name, tex);
                            break;

                        case ETextureKind::Info:
                            // try environments first
                            tex = textureLoadEnvironment(name, settings.resolver, &cache, settings.toWorld);

                            if (tex == NULL) {
                                // else try as textures
                                tex = textureLoadTexture(name, settings.resolver, &cache, settings.diskCacheEnabled, settings.diskCacheDirectory);
                            }

                            if (tex != NULL) {
                                // only store the result if found
                                family->insert(tex->name, tex);
                            }
                            break;
                    }
                }

                return toHandle(tex);
            }

            TexturePerthread *perthread(int thread) override {
                return &cache.perthread[thread];
            }

            void describe(TextureHandle *handle, TextureInfo &out) override {
                memset(&out, 0, sizeof(out));

                CTextureInfoBase *info = fromHandle(handle);
                if (info == NULL)
                    return;

                info->getResolution(out.resolution);
                out.type = info->getTextureType();
                out.numChannels = info->getNumChannels();
                out.hasViewMatrix = info->getViewMatrix(out.viewMatrix);
                out.hasProjectionMatrix = info->getProjectionMatrix(out.projectionMatrix);
            }

        private:
            CTextureSystemSettings settings;   // a copy; the directory points into cacheDirectory
            std::string cacheDirectory;        // the disk-cache directory, copied
            CTextureCache cache;               // the tile cache and the per-thread data
            CTrie<CFileResource *> *family;    // the texture family, one entry per name
            TMutex ownMutex;                   // the load lock when the creator supplies none
            TMutex *loadMutex;                 // the lock handle() holds: the creator's or ownMutex
    };

} // namespace

///////////////////////////////////////////////////////////////////////
// Function				:	createTextureSystem
// Description			:	Create the frame's texture system
// Return Value			:	The system, or NULL on invalid settings
// Comments				:
CTextureSystem *createTextureSystem(const CTextureSystemSettings &settings) {
    if (settings.numThreads < 1) {
        error(CODE_BUG, "createTextureSystem: invalid thread count %d\n", settings.numThreads);
        return NULL;
    }

    if (settings.resolver == NULL) {
        error(CODE_BUG, "createTextureSystem: no file resolver\n");
        return NULL;
    }

    return new CTextureSystemImpl(settings);
}
