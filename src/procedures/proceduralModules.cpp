/**
 * Project: openRender
 *
 * File: proceduralModules.cpp
 *
 * Description:
 *   The Procedures module. See proceduralModules.h. This file holds the loaders' osLoadModule call
 *   sites (SC-008: ProceduresAbi_NoLoadOutsideModule).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "proceduralModules.h"

#include <atomic>
#include <cstdlib>
#include <map>
#include <mutex>
#include <vector>

#include "common/diagnostics.h"
#include "common/os.h"
#include "common/pluginAbi.h"

namespace {

    // One lock guards the registry, every reference count and the frame list. Loads happen under it: a
    // concurrent acquire of the same module waits and then finds it (one load, one version check).
    struct SRegistry {
            std::mutex mutex;
            std::map<std::string, CProceduralModule *> modules;
            std::vector<CDynamicLoadModule *> frameModules;
            std::atomic<int> loaded{0};
            std::atomic<int> unloaded{0};
    };

    SRegistry &registry() {
        static SRegistry *r = new SRegistry; // never destroyed: modules are released at frame end
        return *r;
    }

    std::string canonical(const char *path) {
#ifdef _WINDOWS
        return path;
#else
        // realpath allocates the result (a fixed OS_MAX_PATH_LENGTH buffer is smaller than PATH_MAX,
        // which glibc's fortified realpath aborts on).
        char *resolved = realpath(path, nullptr);
        std::string text = resolved != nullptr ? std::string(resolved) : std::string(path);
        free(resolved);
        return text;
#endif
    }

    const uint32_t kDloSupported[] = {DLO_ABI_VERSION};
    const uint32_t kImplicitSupported[] = {IMPLICIT_ABI_VERSION};
    const uint32_t kDynamicLoadSupported[] = {OR_DYNAMICLOAD_ABI_VERSION};
    const SPluginFamily kDloFamily = {"dlo", "orDloAbiVersion", DLO_ABI_VERSION, kDloSupported, 1, true};
    const SPluginFamily kImplicitFamily = {"implicit", "orImplicitAbiVersion", IMPLICIT_ABI_VERSION, kImplicitSupported, 1, true};
    // FR-009: a generator that declares no version is accepted silently.
    const SPluginFamily kDynamicLoadFamily = {"dynamicload", "orDynamicLoadAbiVersion", OR_DYNAMICLOAD_ABI_VERSION, kDynamicLoadSupported, 1, false};

    enum class ELoad {
        Loaded,
        FailedToLoad, // the file did not load: the caller reports it
        Rejected      // the version check refused it and has reported it
    };

    // Load, then check the version; on rejection unload. The caller holds the lock.
    ELoad loadChecked(const SPluginFamily &family, const char *path, void *&handle) {
        handle = osLoadModule(path);
        if (handle == nullptr)
            return ELoad::FailedToLoad;
        registry().loaded++;
        const SPluginVersionVerdict verdict = checkPluginVersion(family, handle, path);
        if (verdict.verdict == EPluginVerdict::Reject) {
            osUnloadModule(handle);
            registry().unloaded++;
            handle = nullptr;
            return ELoad::Rejected;
        }
        return ELoad::Loaded;
    }

    // The module of `key` if it is loaded, with a reference taken. The caller holds the lock.
    CProceduralModule *findAndAttach(const std::string &key) {
        auto found = registry().modules.find(key);
        if (found == registry().modules.end())
            return nullptr;
        atomicIncrement(found->second->refCount);
        return found->second;
    }

} // namespace

CProceduralModule::CProceduralModule(void *moduleHandle, const std::string &key) : handle(moduleHandle), resolvedPath(key.substr(key.find('\n') + 1)), registryKey(key) {
}

CProceduralModule::~CProceduralModule() {
    if (handle != nullptr) {
        osUnloadModule(handle);
        registry().unloaded++;
    }
}

void CProceduralModule::attach() {
    std::lock_guard<std::mutex> lock(registry().mutex);
    atomicIncrement(refCount);
}

void CProceduralModule::detach() {
    bool destroy = false;
    {
        std::lock_guard<std::mutex> lock(registry().mutex);
        destroy = atomicDecrement(refCount) == 0;
        if (destroy)
            registry().modules.erase(registryKey);
    }
    if (destroy)
        delete this; // unloads the module, outside the lock
}

CDloModule *acquireDloModule(const char *locatedPath, const char *displayName) {
    std::lock_guard<std::mutex> lock(registry().mutex);
    const std::string key = "dlo\n" + canonical(locatedPath);
    if (CProceduralModule *found = findAndAttach(key))
        return static_cast<CDloModule *>(found);

    void *handle = nullptr;
    const ELoad loaded = loadChecked(kDloFamily, locatedPath, handle);
    if (loaded != ELoad::Loaded) {
        if (loaded == ELoad::FailedToLoad)
            error(CODE_BADFILE, "Failed to load DLO \"%s\"\n", displayName);
        return nullptr;
    }

    dloInitFunction init = (dloInitFunction)osResolve(handle, "dloInit");
    dloIntersectFunction intersect = (dloIntersectFunction)osResolve(handle, "dloIntersect");
    dloTiniFunction tini = (dloTiniFunction)osResolve(handle, "dloTini");
    if (init == nullptr || intersect == nullptr || tini == nullptr) {
        if (init == nullptr)
            error(CODE_BADFILE, "Missing \"dloInit\" in DLO \"%s\"\n", displayName);
        else if (intersect == nullptr)
            error(CODE_BADFILE, "Missing \"dloIntersect\" in DLO \"%s\"\n", displayName);
        else
            error(CODE_BADFILE, "Missing \"dloTini\" in DLO \"%s\"\n", displayName);
        osUnloadModule(handle);
        registry().unloaded++;
        return nullptr;
    }

    CDloModule *module = new CDloModule(handle, key);
    module->init = init;
    module->intersect = intersect;
    module->tini = tini;
    atomicIncrement(module->refCount);
    registry().modules[key] = module;
    return module;
}

CImplicitModule *acquireImplicitModule(const char *locatedPath, const char *displayName) {
    std::lock_guard<std::mutex> lock(registry().mutex);
    const std::string key = "implicit\n" + canonical(locatedPath);
    if (CProceduralModule *found = findAndAttach(key))
        return static_cast<CImplicitModule *>(found);

    void *handle = nullptr;
    const ELoad loaded = loadChecked(kImplicitFamily, locatedPath, handle);
    if (loaded != ELoad::Loaded) {
        if (loaded == ELoad::FailedToLoad)
            error(CODE_BADFILE, "Failed to load implicit \"%s\": %s\n", displayName, osModuleError());
        return nullptr;
    }

    implicitInitFunction init = (implicitInitFunction)osResolve(handle, "implicitInit");
    implicitEvalFunction eval = (implicitEvalFunction)osResolve(handle, "implicitEval");
    implicitEvalNormalFunction evalNormal = (implicitEvalNormalFunction)osResolve(handle, "implicitEvalNormal");
    implicitTiniFunction tini = (implicitTiniFunction)osResolve(handle, "implicitTini");
    if (init == nullptr || eval == nullptr || tini == nullptr) {
        error(CODE_BADFILE, "Implicit file \"%s\" is missing some functions\n", displayName);
        osUnloadModule(handle);
        registry().unloaded++;
        return nullptr;
    }

    CImplicitModule *module = new CImplicitModule(handle, key);
    module->init = init;
    module->eval = eval;
    module->evalNormal = evalNormal;
    module->tini = tini;
    atomicIncrement(module->refCount);
    registry().modules[key] = module;
    return module;
}

CDynamicLoadModule *acquireDynamicLoadModule(const char *generator) {
    std::lock_guard<std::mutex> lock(registry().mutex);
    // The generator is loaded exactly as written (research Q4): the key canonicalizes it only to tell
    // two spellings of one file apart, the load uses the name as given.
    const std::string key = "dynamicload\n" + canonical(generator);
    auto found = registry().modules.find(key);
    if (found != registry().modules.end())
        return static_cast<CDynamicLoadModule *>(found->second); // already held by this frame's reference

    // A rejection was reported by the version check; a failed load is reported here, naming the generator.
    void *handle = nullptr;
    const ELoad loaded = loadChecked(kDynamicLoadFamily, generator, handle);
    if (loaded != ELoad::Loaded) {
        if (loaded == ELoad::FailedToLoad)
            error(CODE_NOFILE, "The delayed module \"%s\" is not found: %s\n", generator, osModuleError());
        return nullptr;
    }

    CDynamicLoadModule *module = new CDynamicLoadModule(handle, key);
    module->convertParameters = (TConvertParameters)osResolve(handle, "ConvertParameters");
    module->subdivide = (TSubdivide)osResolve(handle, "Subdivide");
    module->free = (TFree)osResolve(handle, "Free");
    atomicIncrement(module->refCount); // the frame reference
    registry().modules[key] = module;
    registry().frameModules.push_back(module);
    return module;
}

void releaseFrameProceduralModules() {
    std::vector<CDynamicLoadModule *> held;
    {
        std::lock_guard<std::mutex> lock(registry().mutex);
        held.swap(registry().frameModules);
    }
    for (CDynamicLoadModule *module : held)
        module->detach();
}

SProceduralModuleStats proceduralModuleStats() {
    return {registry().loaded.load(), registry().unloaded.load()};
}
