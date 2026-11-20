/**
 * Project: openRender
 *
 * File: shadingSystem.h
 *
 * Description:
 *   The facade of the shading engine's shaders (spec 033, FR-001, FR-004 to FR-006, FR-008, contracts/shading-system.md). It owns
 *   the cache of loaded shader descriptions, the format precedence over the registered backends (shaderBackend.h) and the whole
 *   binding of one shader layer: find or load, type check, instance, key, light registration, the backend's init. The renderer
 *   core asks for a shader by name and a request; it names no backend.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADINGSYSTEM_H
#define SHADINGSYSTEM_H

#include <mutex>

#include "common/containers.h"
#include "common/fileResource.h"
#include "shaderBackend.h"

class CShader;
class CShaderInstance;
class CAttributes;
class CXform;

namespace openrender::shading {

    class ShadingSystem {
        public:
            ShadingSystem(); // takes the entries of the backend registry
            ~ShadingSystem(); // destroys the cache and every description

            ShadingSystem(const ShadingSystem &) = delete;
            ShadingSystem &operator=(const ShadingSystem &) = delete;

            // Find or load; no instance. nullptr when no registered backend found and loaded the name. A name already cached answers
            // whatever the preference says now (the first format wins, GitHub #167)
            CShader *find(const ShaderRequest &request);

            // The whole binding of one layer (order in contracts/shading-system.md). nullptr, with today's diagnostics, on failure.
            // The RIB's parameters are set by the caller afterwards.
            CShaderInstance *bind(const ShaderRequest &request, CAttributes *attributes, CXform *xform, IShaderBindHost &host);

            // The attribute, else the option, else OPENRENDER_DEFAULT_FORMAT
            static const char *resolvePreference(const char *attributeFormat, const char *optionFormat);

        private:
            std::vector<SShaderBackendEntry> backends; // the registry at construction, by priority
            CTrie<CFileResource *> *cache;             // loaded descriptions, keyed by the description's own name
            std::mutex cacheMutex;                     // held across find, load and insert only
    };

} // namespace openrender::shading

#endif
