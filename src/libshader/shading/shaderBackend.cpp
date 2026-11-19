/**
 * Project: openRender
 *
 * File: shaderBackend.cpp
 *
 * Description:
 *   The registry of shader backends (spec 033, contracts/shader-backend.md): what the two built-ins and a test register, and
 *   what the facade reads. A function-local static, so a registrar in another translation unit can run at load in any order.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "shaderBackend.h"

#include <algorithm>
#include <mutex>

namespace {

    struct SRegistry {
            std::mutex lock;
            std::vector<SShaderBackendEntry> entries;
    };

    SRegistry &registry() {
        static SRegistry r;
        return r;
    }

    void removeKey(std::vector<SShaderBackendEntry> &entries, const std::string &key) {
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const SShaderBackendEntry &e) { return e.key == key; }), entries.end());
    }

} // namespace

void registerShaderBackend(const IShaderBackend *backend, int priority) {
    SRegistry &r = registry();
    std::lock_guard<std::mutex> guard(r.lock);
    removeKey(r.entries, backend->key()); // a second registration of a key replaces the first
    r.entries.push_back({backend->key(), backend, priority, std::string()});
}

void registerUnavailableShaderFormat(const char *key, const char *warningFormat) {
    SRegistry &r = registry();
    std::lock_guard<std::mutex> guard(r.lock);
    removeKey(r.entries, key);
    r.entries.push_back({key, nullptr, 0, warningFormat});
}

void unregisterShaderBackend(const char *key) {
    SRegistry &r = registry();
    std::lock_guard<std::mutex> guard(r.lock);
    removeKey(r.entries, key);
}

std::vector<SShaderBackendEntry> shaderBackendEntries() {
    SRegistry &r = registry();
    std::lock_guard<std::mutex> guard(r.lock);
    return r.entries;
}
