/**
 * Project: openRender
 *
 * File: shadingSystem.cpp
 *
 * Description:
 *   The facade of the shading engine's shaders (spec 033, contracts/shading-system.md): the cache, the format precedence and the
 *   binding, moved out of the renderer core (CRenderer::getShader and CRendererContext::getShader).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "shadingSystem.h"

#include <algorithm>
#include <cassert>
#include <string.h>

#include "common/diagnostics.h"
#include "common/memory.h"
#include "common/os.h"
#include "config.h" // OPENRENDER_DEFAULT_FORMAT
#include "include/logging.hpp"
#include "shader.h"

namespace openrender::shading {

    ShadingSystem::ShadingSystem() : backends(shaderBackendEntries()), cache(new CTrie<CFileResource *>) {
        // The order the formats are tried in once the preferred one is out: by priority (a stable sort keeps registration order for equals)
        std::stable_sort(backends.begin(), backends.end(), [](const SShaderBackendEntry &a, const SShaderBackendEntry &b) { return a.priority < b.priority; });
    }

    ShadingSystem::~ShadingSystem() {
        // Destroys every cached description with the cache, as the renderer's global file cache did
        cache->destroy();
    }

    const char *ShadingSystem::resolvePreference(const char *attributeFormat, const char *optionFormat) {
        return attributeFormat != nullptr ? attributeFormat : (optionFormat != nullptr ? optionFormat : OPENRENDER_DEFAULT_FORMAT);
    }

    CShader *ShadingSystem::find(const ShaderRequest &request) {
        assert(request.name != nullptr);

        std::lock_guard<std::mutex> guard(cacheMutex);

        // Check if we already loaded this shader before ... whatever the preference says now: the first format wins (#167)
        CFileResource *file = nullptr;
        if (cache->find(request.name, file))
            return static_cast<CShader *>(file);

        const char *preference = resolvePreference(request.attributeFormat, request.optionFormat);

        // The order: the entry whose key is the preference, then the others by priority
        std::vector<const SShaderBackendEntry *> order;
        for (const SShaderBackendEntry &e : backends)
            if (e.key == preference)
                order.push_back(&e);
        for (const SShaderBackendEntry &e : backends)
            if (e.key != preference)
                order.push_back(&e);

        for (const SShaderBackendEntry *entry : order) {
            if (entry->backend == nullptr) {
                // A format this build cannot offer. The scene asked for it only when it is the preferred one: say so, once per
                // uncached request; the next format loads the shader (a build with a JIT that simply has no .slo on disk falls back silently)
                if (entry->key == preference)
                    warning(CODE_INCAPABLE, entry->unavailableWarning.c_str(), request.name);
                continue;
            }

            char shaderLocation[OS_MAX_PATH_LENGTH];
            if (request.resolver->locate(shaderLocation, request.name, entry->backend->extension()) != TRUE)
                continue;

            CShader *desc = entry->backend->load(request.name, shaderLocation);
            if (desc == nullptr)
                continue; // refused by its loader (the loader gave its diagnostics): the next format

            log_debug("getShader: loaded shader '{}' from '{}'", request.name, shaderLocation);
            // Cache under desc->name (a stable, owned copy from CFileResource's ctor), not the caller's `name`: the RIB parser's
            // per-statement memory arena is rewound (memRestore) before the next statement is lexed, so `name` can be silently
            // overwritten once a later statement's tokens reuse the same address, aliasing the trie's cached key.
            cache->insert(desc->name, desc);
            return desc;
        }

        return nullptr;
    }

    CShaderInstance *ShadingSystem::bind(const ShaderRequest &request, CAttributes *attributes, CXform *xform, IShaderBindHost &host) {
        // 1. find or load; the cache lock is released before the instance is made
        CShader *desc = find(request);
        if (desc == nullptr) {
            error(CODE_NOSHADER, "Failed to find shader \"%s\"\n", request.name);
            return nullptr;
        }

        // 2. the type
        if (request.type != kAnyShaderType && desc->type != request.type) {
            error(CODE_NOSHADER, "Shader \"%s\" is not of the expected type\n", request.name);
            return nullptr;
        }

        // 3.
        log_info("getShader: loading shader '{}', type '{}', in format '{}'", request.name, desc->type, resolvePreference(request.attributeFormat, request.optionFormat));

        // 4. the instance, with one reference
        CShaderInstance *instance = desc->backend->instantiate(desc, attributes, xform);

        // 5. the key: only now, after a successful load and type check
        instance->instanceKey = host.takeInstanceKey();

        // 6. a light: the categories (from the default __category: the RIB's parameters are not set yet), then the frame's light list
        if (desc->type == SL_LIGHTSOURCE) {
            instance->createCategories();
            host.registerLight(instance);
        }

        // 7. the bind-time init, inside a region of the global arena
        arenaBegin(globalArena);
        desc->backend->initialise(*instance);
        arenaEnd(globalArena);

        // 8.
        return instance;
    }

} // namespace openrender::shading
