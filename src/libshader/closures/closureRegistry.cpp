/**
 * Project: openRender
 *
 * File: closureRegistry.cpp
 *
 * Description:
 *   The closure registry (spec 035, closureRegistry.h). The table is an immutable snapshot published through an atomic pointer: a lookup
 *   reads the pointer and the snapshot and takes no lock; a registration takes the mutex, copies the snapshot, adds the entry and
 *   publishes the copy. The entries themselves live in stable storage for the life of the process, so a pointer a lookup returned stays
 *   valid, and so does every superseded snapshot (a reader may still hold one): registrations are few, the copies are small.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "closureRegistry.h"

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "closureBuiltins.h"
#include "common/diagnostics.h"

namespace openrender::shading {

    namespace {

        // One registered closure: the entry (whose name and parameters point into this object) and the storage they point to
        struct Stored {
                OrClosureEntry entry;
                std::string name;
                std::vector<OrClosureParam> params;
                std::vector<std::string> paramNames;
        };

        // The published table: by id and by name (aliases included)
        struct Snapshot {
                std::vector<const Stored *> byId;
                std::unordered_map<std::string, const Stored *> byName;
                std::unordered_map<std::string, const Stored *> aliases;
        };

        struct Registry {
                std::mutex mutex;                                 // registration only
                std::atomic<uint64_t> lockCount{0};               // the times `mutex` was taken (the test hook)
                std::deque<Stored> storage;                       // stable addresses
                std::vector<std::unique_ptr<Snapshot>> snapshots; // every published snapshot, kept alive (a reader may hold an old one)
                std::atomic<const Snapshot *> current{nullptr};
                std::once_flag builtinsOnce;
                std::atomic<bool> ready{false}; // the built-ins are all in (set after the fill)

                std::mutex internMutex;
                std::unordered_set<std::string> interned; // node-based: the c_str() pointers are stable
        };

        Registry &registry() {
            static Registry r;
            return r;
        }

        uint32_t paramSize(OrClosureParamType type) {
            switch (type) {
                case OR_CPARAM_FLOAT:
                case OR_CPARAM_INT:
                    return 4;
                case OR_CPARAM_COLOR:
                case OR_CPARAM_NORMAL:
                case OR_CPARAM_VECTOR:
                    return 12;
                case OR_CPARAM_STRING:
                    return (uint32_t)sizeof(const char *);
            }
            return 0;
        }

        // The checks of an entry; returns nullptr when it is acceptable, else the reason (a static string)
        const char *refusal(const OrClosureEntry &e, bool builtin) {
            if (e.category < OR_CLOSURE_BSDF || e.category > OR_CLOSURE_SIGNAL)
                return "the category is out of range";
            if (e.category == OR_CLOSURE_BSDF && e.ops == nullptr && !builtin)
                return "a BSDF closure needs component operations (ops)";
            if (e.numParams < 0 || (e.numParams > 0 && e.params == nullptr))
                return "the parameter list is malformed";
            if (e.blockSize > 0 && (e.blockAlign == 0 || (e.blockAlign & (e.blockAlign - 1)) != 0 || e.blockAlign > 16))
                return "blockAlign must be a power of two, at most 16";
            for (int i = 0; i < e.numParams; ++i) {
                const OrClosureParam &p = e.params[i];
                if (p.name == nullptr || p.name[0] == '\0')
                    return "a parameter has no name";
                if ((uint64_t)p.offset + paramSize(p.type) > e.blockSize)
                    return "a parameter overruns the parameter block";
            }
            return nullptr;
        }

        // Adds an entry under the mutex; returns the id or -1
        int registerLocked(Registry &r, const OrClosureEntry &entry, bool builtin) {
            std::lock_guard<std::mutex> lock(r.mutex);
            r.lockCount.fetch_add(1, std::memory_order_relaxed);

            if (entry.structSize < sizeof(OrClosureEntry)) {
                error(CODE_RANGE, "Closure registration refused: the entry's structSize %u is smaller than %u", (unsigned)entry.structSize, (unsigned)sizeof(OrClosureEntry));
                return -1;
            }
            if (entry.name == nullptr || entry.name[0] == '\0') {
                error(CODE_RANGE, "Closure registration refused: the entry has no name");
                return -1;
            }
            const Snapshot *old = r.current.load(std::memory_order_acquire);
            const Snapshot empty{};
            if (old == nullptr)
                old = &empty;
            const std::string name(entry.name);
            if (old->byName.count(name) != 0 || old->aliases.count(name) != 0) {
                error(CODE_RANGE, "Closure registration refused: a closure named \"%s\" is already registered", entry.name);
                return -1;
            }
            if (const char *why = refusal(entry, builtin)) {
                error(CODE_RANGE, "Closure registration of \"%s\" refused: %s", entry.name, why);
                return -1;
            }

            // The registry's own copy
            r.storage.emplace_back();
            Stored &s = r.storage.back();
            s.name = name;
            s.entry = entry;
            s.entry.id = (int32_t)old->byId.size();
            s.entry.name = s.name.c_str();
            s.paramNames.reserve((size_t)entry.numParams);
            s.params.reserve((size_t)entry.numParams);
            for (int i = 0; i < entry.numParams; ++i) {
                s.paramNames.emplace_back(entry.params[i].name);
                OrClosureParam p = entry.params[i];
                p.name = s.paramNames.back().c_str();
                s.params.push_back(p);
            }
            s.entry.params = s.params.empty() ? nullptr : s.params.data();

            auto next = std::make_unique<Snapshot>(*old);
            next->byId.push_back(&s);
            next->byName[name] = &s;
            const int id = s.entry.id;
            r.current.store(next.get(), std::memory_order_release);
            r.snapshots.push_back(std::move(next));
            return id;
        }

        // The eight built-ins, in id order, from the block structs of closureBuiltins.h
        void fillBuiltins(Registry &r) {
            const auto add = [&r](const char *name, OrClosureCategory category, const OrClosureParam *params, int numParams, uint32_t size, uint32_t align, int expectedId) {
                OrClosureEntry e = {};
                e.structSize = sizeof(OrClosureEntry);
                e.name = name;
                e.category = category;
                e.params = params;
                e.numParams = numParams;
                e.blockSize = size;
                e.blockAlign = align;
                e.ops = nullptr; // Phase 2 fills the BSDF components' operations; the first-use fill is exempt from "a BSDF needs ops"
                const int id = registerLocked(r, e, /*builtin*/ true);
                if (id != expectedId)
                    error(CODE_BUG, "The built-in closure \"%s\" got id %d, expected %d", name, id, expectedId);
            };
            const auto off = [](size_t o) { return (uint32_t)o; };

            const OrClosureParam diffuse[] = {{"N", OR_CPARAM_NORMAL, off(offsetof(ClosureDiffuseBlock, N))}};
            add("diffuse", OR_CLOSURE_BSDF, diffuse, 1, sizeof(ClosureDiffuseBlock), alignof(ClosureDiffuseBlock), kClosureDiffuse);

            const OrClosureParam orenNayar[] = {{"N", OR_CPARAM_NORMAL, off(offsetof(ClosureOrenNayarBlock, N))}, {"sigma", OR_CPARAM_FLOAT, off(offsetof(ClosureOrenNayarBlock, sigma))}};
            add("oren_nayar", OR_CLOSURE_BSDF, orenNayar, 2, sizeof(ClosureOrenNayarBlock), alignof(ClosureOrenNayarBlock), kClosureOrenNayar);

            const OrClosureParam microfacet[] = {{"distribution", OR_CPARAM_STRING, off(offsetof(ClosureMicrofacetBlock, distribution))},
                                                 {"N", OR_CPARAM_NORMAL, off(offsetof(ClosureMicrofacetBlock, N))},
                                                 {"alpha", OR_CPARAM_FLOAT, off(offsetof(ClosureMicrofacetBlock, alpha))},
                                                 {"eta", OR_CPARAM_FLOAT, off(offsetof(ClosureMicrofacetBlock, eta))},
                                                 {"refract", OR_CPARAM_INT, off(offsetof(ClosureMicrofacetBlock, refract))}};
            add("microfacet", OR_CLOSURE_BSDF, microfacet, 5, sizeof(ClosureMicrofacetBlock), alignof(ClosureMicrofacetBlock), kClosureMicrofacet);

            const OrClosureParam reflection[] = {{"N", OR_CPARAM_NORMAL, off(offsetof(ClosureReflectionBlock, N))}, {"eta", OR_CPARAM_FLOAT, off(offsetof(ClosureReflectionBlock, eta))}};
            add("reflection", OR_CLOSURE_BSDF, reflection, 2, sizeof(ClosureReflectionBlock), alignof(ClosureReflectionBlock), kClosureReflection);

            const OrClosureParam refraction[] = {{"N", OR_CPARAM_NORMAL, off(offsetof(ClosureRefractionBlock, N))}, {"eta", OR_CPARAM_FLOAT, off(offsetof(ClosureRefractionBlock, eta))}};
            add("refraction", OR_CLOSURE_BSDF, refraction, 2, sizeof(ClosureRefractionBlock), alignof(ClosureRefractionBlock), kClosureRefraction);

            add("transparent", OR_CLOSURE_SIGNAL, nullptr, 0, 0, 0, kClosureTransparent);
            add("holdout", OR_CLOSURE_SIGNAL, nullptr, 0, 0, 0, kClosureHoldout);

            const OrClosureParam edf[] = {{"emittance", OR_CPARAM_COLOR, off(offsetof(ClosureUniformEdfBlock, emittance))}};
            add("uniform_edf", OR_CLOSURE_EDF, edf, 1, sizeof(ClosureUniformEdfBlock), alignof(ClosureUniformEdfBlock), kClosureUniformEdf);

            // The aliases: `emission` is the old name of uniform_edf (deprecated)
            std::lock_guard<std::mutex> lock(r.mutex);
            r.lockCount.fetch_add(1, std::memory_order_relaxed);
            const Snapshot *old = r.current.load(std::memory_order_acquire);
            auto next = std::make_unique<Snapshot>(*old);
            next->aliases["emission"] = next->byId[kClosureUniformEdf];
            r.current.store(next.get(), std::memory_order_release);
            r.snapshots.push_back(std::move(next));
        }

        // The published table; the first use fills the built-ins (lock-free once published). `ready` is set only after the whole fill: the
        // table is non-null as soon as the first built-in is added, so testing it would let a second thread in before the eight have their ids.
        const Snapshot *published() {
            Registry &r = registry();
            if (!r.ready.load(std::memory_order_acquire)) {
                std::call_once(r.builtinsOnce, [&r]() {
                    fillBuiltins(r);
                    r.ready.store(true, std::memory_order_release);
                });
            }
            return r.current.load(std::memory_order_acquire);
        }

    } // namespace

    int registerClosure(const OrClosureEntry &entry) {
        (void)published(); // the built-ins take ids 0-7 whatever registers first
        return registerLocked(registry(), entry, /*builtin*/ false);
    }

    const OrClosureEntry *findClosure(const char *name) {
        if (name == nullptr || name[0] == '\0')
            return nullptr;
        const Snapshot *s = published();
        const std::string key(name);
        auto it = s->byName.find(key);
        if (it != s->byName.end())
            return &it->second->entry;
        auto alias = s->aliases.find(key);
        if (alias != s->aliases.end()) {
            static std::once_flag deprecated;
            std::call_once(deprecated, [&]() { warning(CODE_BADTOKEN, "The closure name \"%s\" is deprecated: use \"%s\"", name, alias->second->entry.name); });
            return &alias->second->entry;
        }
        error(CODE_BADTOKEN, "Unknown closure \"%s\": it is not registered", name);
        return nullptr;
    }

    const OrClosureEntry *closureEntry(int32_t id) {
        const Snapshot *s = published();
        if (id < 0 || (size_t)id >= s->byId.size())
            return nullptr;
        return &s->byId[(size_t)id]->entry;
    }

    const char *internClosureString(const char *text) {
        if (text == nullptr)
            return nullptr;
        Registry &r = registry();
        std::lock_guard<std::mutex> lock(r.internMutex);
        return r.interned.emplace(text).first->c_str();
    }

    uint64_t closureRegistryLockCount() { return registry().lockCount.load(std::memory_order_relaxed); }

} // namespace openrender::shading
