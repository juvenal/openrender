/**
 * Project: openRender
 *
 * File: shadeopModule.cpp
 *
 * Description:
 *   The Shader DSO Operands module. See shadeopModule.h. This file holds the shadeop osLoadModule call
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

#include "shadeopModule.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "common/diagnostics.h"
#include "common/os.h"
#include "common/pluginAbi.h"

namespace {

    const uint32_t kSupported[] = {SHADEOP_ABI_VERSION};
    const SPluginFamily kFamily = {"shadeop", "orShadeopAbiVersion", SHADEOP_ABI_VERSION, kSupported, 1, true};

    const char *text(const char *s) {
        return s != nullptr ? s : "";
    }

    // The module files of one search-path directory, in lexicographic order of their names (not the order
    // the file system returns them in: FR-013).
    int collectModule(const char *file, void *userData) {
        static_cast<std::vector<std::string> *>(userData)->push_back(file);
        return 1;
    }

    std::vector<std::string> moduleFiles(const char *directory) {
        std::string pattern(directory);
        if (pattern.empty() || (pattern.back() != OS_DIR_SEPERATOR && pattern.back() != '/'))
            pattern += OS_DIR_SEPERATOR;
        pattern += "*.";
        pattern += osModuleExtension;

        std::vector<std::string> files;
        osEnumerate(pattern.c_str(), collectModule, &files);
        std::sort(files.begin(), files.end());
        return files;
    }

    // A module being scanned: loaded and version-checked, its table resolved.
    struct SScannedModule {
            void *handle = nullptr;
            const SHADEOP_SPEC *rows = nullptr;
            const SHADEOP_THREAD_SPEC *threadRows = nullptr; // interface 1.1 and newer, if the module exports it
    };

    // Loads `file`; returns false (nothing loaded) when it does not load, is rejected, or has no <name>_shadeops.
    bool openModule(const char *file, const std::string &name, bool reportLoadFailure, SScannedModule &out) {
        void *handle = osLoadModule(file);
        if (handle == nullptr) {
            if (reportLoadFailure)
                error(CODE_SYSTEM, "Failed to load DSO \"%s\": %s\n", file, osModuleError());
            return false;
        }

        const SPluginVersionVerdict verdict = checkPluginVersion(kFamily, handle, file);
        if (verdict.verdict == EPluginVerdict::Reject) {
            osUnloadModule(handle);
            return false;
        }

        const SHADEOP_SPEC *rows = static_cast<const SHADEOP_SPEC *>(osResolve(handle, (name + "_shadeops").c_str()));
        if (rows == nullptr) {
            osUnloadModule(handle);
            return false;
        }

        out.handle = handle;
        out.rows = rows;
        out.threadRows = nullptr;
        if (OR_ABI_MAJOR(verdict.pluginVersion) == 1 && OR_ABI_MINOR(verdict.pluginVersion) >= 1)
            out.threadRows = static_cast<const SHADEOP_THREAD_SPEC *>(osResolve(handle, (name + "_shadeop_threads").c_str()));
        return true;
    }

    // "shadeop <module>: <symbol> not found", once per module and symbol.
    void reportUnresolvedExec(const char *file, const std::string &symbol) {
        static std::mutex mutex;
        static std::set<std::string> *reported = new std::set<std::string>;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!reported->insert(std::string(file) + "\n" + symbol).second)
                return;
        }
        warning(CODE_BADFILE, "shadeop %s: %s not found, the table entry is skipped\n", file, symbol.c_str());
    }

    struct SSlot {
            std::atomic<int> state{0}; // 0 not made, 1 being made, 2 made
            void *data = nullptr;
    };

} // namespace

class CShadeopOperand {
    public:
        std::string name;
        std::string prototype;
        std::string moduleFile;
        void *module = nullptr;
        dsoInitFunction init = nullptr;
        dsoExecFunction exec = nullptr;
        dsoCleanupFunction cleanup = nullptr;
        dsoThreadInitFunction threadInit = nullptr;
        dsoThreadCleanupFunction threadCleanup = nullptr;
        void *initData = nullptr;

        std::unique_ptr<SSlot[]> slots; // one per thread index, only when threadInit is set
        int numSlots = 0;
        std::atomic<bool> reportedOutside{false};

        void *hostObject = nullptr;
        void (*destroyHostObject)(void *) = nullptr;

        // Called with no shading thread running (beginFrame) or before the first use: existing data is kept.
        void growSlots(int count) {
            if (threadInit == nullptr || count <= numSlots)
                return;
            std::unique_ptr<SSlot[]> grown(new SSlot[count]);
            for (int i = 0; i < numSlots; ++i) {
                grown[i].state.store(slots[i].state.load());
                grown[i].data = slots[i].data;
            }
            slots = std::move(grown);
            numSlots = count;
        }

        // threadCleanup for every made slot, then cleanup, then the host object, then the unload.
        void release() {
            for (int i = 0; i < numSlots; ++i)
                if (slots[i].state.load() == 2 && threadCleanup != nullptr)
                    threadCleanup(initData, slots[i].data);
            if (cleanup != nullptr)
                cleanup(initData);
            if (hostObject != nullptr && destroyHostObject != nullptr)
                destroyHostObject(hostObject);
            if (module != nullptr)
                osUnloadModule(module);
        }
};

void *shadeopOperandData(CShadeopOperand *operand, int thread) {
    if (operand->threadInit == nullptr)
        return operand->initData;

    if (thread < 0 || thread >= operand->numSlots) {
        if (!operand->reportedOutside.exchange(true))
            warning(CODE_RANGE, "shadeop \"%s\": thread index %d is outside the %d per-thread slots; the process-wide data is used\n", operand->name.c_str(), thread, operand->numSlots);
        return operand->initData;
    }

    SSlot &slot = operand->slots[thread];
    if (slot.state.load(std::memory_order_acquire) == 2)
        return slot.data;

    int expected = 0;
    if (slot.state.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) {
        slot.data = operand->threadInit(operand->initData, thread);
        slot.state.store(2, std::memory_order_release);
        return slot.data;
    }
    while (slot.state.load(std::memory_order_acquire) != 2)
        std::this_thread::yield();
    return slot.data;
}

dsoExecFunction shadeopOperandExec(const CShadeopOperand *operand) {
    return operand->exec;
}

void *shadeopOperandHostObject(const CShadeopOperand *operand) {
    return operand->hostObject;
}

void setShadeopOperandHostObject(CShadeopOperand *operand, void *object, void (*destroy)(void *)) {
    operand->hostObject = object;
    operand->destroyHostObject = destroy;
}

void enumerateShadeopDeclarations(const SShadeopSearchPath &path, const char *name, const std::function<void(const SShadeopDeclaration &)> &visit) {
    for (int d = 0; d < path.count; ++d) {
        for (const std::string &file : moduleFiles(path.directories[d])) {
            SScannedModule module;
            if (!openModule(file.c_str(), name, false, module))
                continue;
            for (int i = 0;; ++i) {
                const char *definition = text(module.rows[i].definition);
                if (definition[0] == '\0')
                    break;
                SShadeopDeclaration declaration;
                if (parseShadeopDeclaration(definition, declaration))
                    visit(declaration);
            }
            osUnloadModule(module.handle);
        }
    }
}

struct CShadeopRegistry::SImpl {
        std::mutex mutex;
        std::map<std::string, CShadeopOperand *> byPrototype; // "name\nprototype"
        std::vector<CShadeopOperand *> operands;              // in acquisition order
        int numThreads = 0;
};

CShadeopRegistry::CShadeopRegistry() : impl(new SImpl) {
}

CShadeopRegistry::~CShadeopRegistry() {
    for (size_t i = impl->operands.size(); i-- > 0;) {
        impl->operands[i]->release();
        delete impl->operands[i];
    }
    delete impl;
}

void CShadeopRegistry::beginFrame(int numThreads) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (numThreads > impl->numThreads)
        impl->numThreads = numThreads;
    for (CShadeopOperand *operand : impl->operands)
        operand->growSlots(impl->numThreads);
}

CShadeopOperand *CShadeopRegistry::acquire(const SShadeopSearchPath &path, const char *name, const char *prototype) {
    std::lock_guard<std::mutex> lock(impl->mutex);

    const std::string key = std::string(name) + "\n" + prototype;
    auto cached = impl->byPrototype.find(key);
    if (cached != impl->byPrototype.end())
        return cached->second;

    for (int d = 0; d < path.count; ++d) {
        for (const std::string &file : moduleFiles(path.directories[d])) {
            SScannedModule module;
            if (!openModule(file.c_str(), name, true, module))
                continue;

            for (int i = 0;; ++i) {
                const SHADEOP_SPEC &row = module.rows[i];
                const char *definition = text(row.definition);
                if (definition[0] == '\0')
                    break;

                SShadeopDeclaration declaration;
                if (!parseShadeopDeclaration(definition, declaration) || declaration.prototype != prototype)
                    continue;

                // FR-033: the test is on the value resolved into this row's own variable.
                dsoExecFunction exec = reinterpret_cast<dsoExecFunction>(osResolve(module.handle, declaration.name.c_str()));
                if (exec == nullptr) {
                    reportUnresolvedExec(file.c_str(), declaration.name);
                    continue;
                }

                CShadeopOperand *operand = new CShadeopOperand;
                operand->name = name;
                operand->prototype = prototype;
                operand->moduleFile = file;
                operand->module = module.handle;
                operand->exec = exec;
                if (text(row.init)[0] != '\0')
                    operand->init = reinterpret_cast<dsoInitFunction>(osResolve(module.handle, row.init));
                if (text(row.cleanup)[0] != '\0')
                    operand->cleanup = reinterpret_cast<dsoCleanupFunction>(osResolve(module.handle, row.cleanup));
                if (module.threadRows != nullptr) {
                    if (text(module.threadRows[i].threadInit)[0] != '\0')
                        operand->threadInit = reinterpret_cast<dsoThreadInitFunction>(osResolve(module.handle, module.threadRows[i].threadInit));
                    if (text(module.threadRows[i].threadCleanup)[0] != '\0')
                        operand->threadCleanup = reinterpret_cast<dsoThreadCleanupFunction>(osResolve(module.handle, module.threadRows[i].threadCleanup));
                }

                // The one process-wide init, at acquisition (as getDSO always did).
                operand->initData = operand->init != nullptr ? operand->init(0, nullptr) : nullptr;
                operand->growSlots(std::max(impl->numThreads, 1)); // slot 0 also serves a call made before the first frame (shader init blocks)

                impl->operands.push_back(operand);
                impl->byPrototype[key] = operand;
                return operand;
            }

            osUnloadModule(module.handle); // supplies nothing
        }
    }
    return nullptr;
}
