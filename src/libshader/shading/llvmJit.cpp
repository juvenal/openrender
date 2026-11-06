/**
 * Project: openRender
 *
 * File: llvmJit.cpp
 *
 * Description:
 *   CLLVMJitEngine implementation: Manages ORC JIT v2 for on-the-fly shader compilation.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "llvmJit.h"
#include "common/diagnostics.h"
#include "common/sloFormat.h"
#include "logging.hpp"

#include <cstdio>

// LLVM headers generate warnings under our strict flags; suppress them for this block only.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#pragma GCC diagnostic pop

// CLLVMJitEngine::extractMetadataFromModule / extractMetadataFromFile now live in
// llvmJitMetadata.cpp, built into the standalone libshader_jitmeta static library.
// That code touches only LLVM bitcode APIs (no ri/CRenderer symbols), so tools like
// sloinfo that only need metadata inspection can link it without pulling in the rest
// of libshader_shading (sloinfo needs neither the JIT nor a renderer).

// =========================================================================
// CLLVMJitEngine constructor / destructor / getInstance
// =========================================================================
void CLLVMJitEngine::addProcessSymbol(const char *name, void *addr) {
    llvm::sys::DynamicLibrary::AddSymbol(name, addr);
}

CLLVMJitEngine::CLLVMJitEngine() {
    // LLVM requires native target initialization before any JIT compilation.
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto jitOrErr = llvm::orc::LLJITBuilder()
                        .create();
    if (!jitOrErr) {
        log_error("Failed to create LLJIT instance: {}", llvm::toString(jitOrErr.takeError()));
        return;
    }
    jit = std::move(*jitOrErr);

    // Keep the default reporting (stderr) and also remember the text, so a failed bind
    // can name the unresolved symbols in the diagnostic.
    jit->getExecutionSession().setErrorReporter([this](llvm::Error err) {
        const std::string text = llvm::toString(std::move(err));
        llvm::errs() << "JIT session error: " << text << "\n";
        std::lock_guard<std::mutex> lock(sessionErrorMutex_);
        if (!sessionErrors_.empty())
            sessionErrors_ += " ";
        sessionErrors_ += text;
    });

    // IR dump transform — intercept the IR right before code generation.
    // Active only when OPENRENDER_DUMP_JIT_IR is set. Used to verify no
    // optimization pass silently eliminates op_clampf calls.
    if (getenv("OPENRENDER_DUMP_JIT_IR")) {
        jit->getIRTransformLayer().setTransform(
            [](llvm::orc::ThreadSafeModule TSM,
               const llvm::orc::MaterializationResponsibility &) {
                TSM.withModuleDo([](llvm::Module &M) {
                    fprintf(stderr, "[JIT-IR-DUMP] module: %s\n",
                            M.getName().str().c_str());
                    M.print(llvm::errs(), nullptr);
                });
                return TSM;
            });
    }

    // Expose process-level symbols (op_*, etc.) to JIT-compiled shaders.
    // This allows the JIT to resolve calls to runtime functions that are
    // already loaded in the orender process (from libshader_shading / ri.dylib).
    char globalPrefix = jit->getDataLayout().getGlobalPrefix();
    auto dlsgOrErr = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(globalPrefix);
    if (!dlsgOrErr) {
        log_error("Failed to create DynamicLibrarySearchGenerator: {}",
                  llvm::toString(dlsgOrErr.takeError()));
    }
    else {
        jit->getMainJITDylib().addGenerator(std::move(*dlsgOrErr));
    }
}

CLLVMJitEngine::~CLLVMJitEngine() {}

CLLVMJitEngine &CLLVMJitEngine::getInstance() {
    static CLLVMJitEngine instance;
    return instance;
}

// =========================================================================
// CLLVMJitEngine::getCachedMetadata
// =========================================================================
bool CLLVMJitEngine::getCachedMetadata(const std::string &shaderName, SLOShaderInfo &info) const {
    auto it = metaCache_.find(shaderName);
    if (it == metaCache_.end())
        return false;
    info = it->second;
    return true;
}

// =========================================================================
// CLLVMJitEngine::compileShader
// =========================================================================
TShaderJitEntry CLLVMJitEngine::compileShader(const std::string &filename,
                                              const std::string &shaderName) {
    if (!jit)
        return nullptr;

    std::lock_guard<std::mutex> lock(compileMutex_);

    // Return cached entry point if already compiled.
    auto cached = cache_.find(shaderName);
    if (cached != cache_.end())
        return cached->second;

    // Load the bitcode from the .slo file.
    auto bufferOrErr = llvm::MemoryBuffer::getFile(filename);
    if (!bufferOrErr) {
        log_error("Failed to open shader file '{}': {}", filename,
                  bufferOrErr.getError().message());
        return nullptr;
    }

    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto moduleOrErr = llvm::parseBitcodeFile(bufferOrErr.get()->getMemBufferRef(), *ctx);
    if (!moduleOrErr) {
        log_error("Failed to parse bitcode in '{}': {}", filename,
                  llvm::toString(moduleOrErr.takeError()));
        return nullptr;
    }

    auto &module = *moduleOrErr;

    // Check the format before anything is added or linked: a module of another
    // major, or newer than this loader, may call entries that do not exist or mean
    // something else. A module without a record predates the versioning: 1.0.0.
    {
        SFormatVersion format{1, 0, 0};
        const bool recorded = readFormat(*module, format);
        if (!formatVersionReadable(format.major, format.minor, SLO_FORMAT_MAJOR, SLO_FORMAT_MINOR)) {
            error(CODE_BADFILE,
                  "Cannot load JIT shader \"%s\" (%s): its .slo format is %d.%d.%d%s, this renderer reads %d.%d.%d "
                  "(same major, minor up to %d). Recompile it with `oshader --jit`.\n",
                  shaderName.c_str(), filename.c_str(), format.major, format.minor, format.patch,
                  recorded ? "" : " (it has no readable format record, which counts as 1.0.0)", SLO_FORMAT_MAJOR,
                  SLO_FORMAT_MINOR, SLO_FORMAT_PATCH, SLO_FORMAT_MINOR);
            return nullptr;
        }
    }

    // Extract named metadata before transferring ownership to the JIT.
    SLOShaderInfo meta;
    bool hasInit = false;
    if (extractMetadataFromModule(*module, meta)) {
        metaCache_[shaderName] = std::move(meta);
        // Check for non-trivial init section.
        const llvm::NamedMDNode *initMD = module->getNamedMetadata("openrender.shader.hasinit");
        hasInit = (initMD && initMD->getNumOperands() > 0);
    }

    // The DSO operands the module calls (spec 038): the slot list, in slot order.
    std::vector<std::pair<std::string, std::string>> shadeops;
    if (const llvm::NamedMDNode *slots = module->getNamedMetadata("openrender.shader.shadeops")) {
        for (unsigned i = 0; i < slots->getNumOperands(); ++i) {
            const llvm::MDNode *entry = slots->getOperand(i);
            const auto *name = entry->getNumOperands() >= 2 ? llvm::dyn_cast<llvm::MDString>(entry->getOperand(0)) : nullptr;
            const auto *proto = entry->getNumOperands() >= 2 ? llvm::dyn_cast<llvm::MDString>(entry->getOperand(1)) : nullptr;
            if (name == nullptr || proto == nullptr) {
                error(CODE_BADFILE, "Cannot load JIT shader \"%s\" (%s): its DSO operand list is malformed. Recompile it with `oshader --jit`.\n",
                      shaderName.c_str(), filename.c_str());
                return nullptr;
            }
            shadeops.emplace_back(name->getString().str(), proto->getString().str());
        }
    }

    {
        std::lock_guard<std::mutex> errLock(sessionErrorMutex_);
        sessionErrors_.clear();
    }

    // Add the module to the JIT (transfers ownership).
    if (auto err = jit->addIRModule(
            llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx)))) {
        const std::string text = llvm::toString(std::move(err));
        log_error("Failed to add IR module to JIT: {}", text);
        error(CODE_BADFILE, "Cannot load JIT shader \"%s\" (%s): %s\n", shaderName.c_str(), filename.c_str(), text.c_str());
        return nullptr;
    }

    // Look up the main entry point.
    auto symOrErr = jit->lookup(shaderName);
    if (!symOrErr) {
        std::string text = llvm::toString(symOrErr.takeError());
        {
            std::lock_guard<std::mutex> errLock(sessionErrorMutex_);
            if (!sessionErrors_.empty())
                text += " (" + sessionErrors_ + ")";
        }
        log_error("Failed to find shader entry '{}' in JIT: {}", shaderName, text);
        // Binding fails when the module calls a runtime entry that no longer exists
        // (an older ABI), so say what to do about it, in the diagnostic sink the
        // .rslo loader reports through.
        error(CODE_BADFILE,
              "Cannot bind JIT shader \"%s\" (%s): the .slo is stale or incompatible with this renderer and must be recompiled with `oshader --jit`. %s\n",
              shaderName.c_str(), filename.c_str(), text.c_str());
        return nullptr;
    }

    TShaderJitEntry entry = reinterpret_cast<TShaderJitEntry>(symOrErr->getValue());

    // The module's slot array, written by bindShadeops() once the loader has resolved the operands.
    if (!shadeops.empty()) {
        auto arrayOrErr = jit->lookup(shaderName + ".shadeops");
        if (!arrayOrErr) {
            const std::string text = llvm::toString(arrayOrErr.takeError());
            error(CODE_BADFILE, "Cannot bind JIT shader \"%s\" (%s): its DSO slot array is missing (%s). Recompile it with `oshader --jit`.\n",
                  shaderName.c_str(), filename.c_str(), text.c_str());
            return nullptr;
        }
        shadeopSlots_[shaderName] = std::move(shadeops);
        shadeopArrays_[shaderName] = reinterpret_cast<void **>(arrayOrErr->getValue());
    }

    cache_[shaderName] = entry;

    // Cache init entry if present.
    if (hasInit) {
        const std::string initName = shaderName + "_init";
        auto initOrErr = jit->lookup(initName);
        if (initOrErr) {
            initCache_[shaderName] =
                reinterpret_cast<TShaderJitEntry>(initOrErr->getValue());
        }
        else {
            llvm::consumeError(initOrErr.takeError());
        }
    }

    return entry;
}

// =========================================================================
// CLLVMJitEngine::bindShadeops
// =========================================================================
int CLLVMJitEngine::bindShadeops(const std::string &shaderName,
                                 const std::function<void *(const std::string &, const std::string &)> &resolve,
                                 std::string *failedName, std::string *failedPrototype) {
    std::lock_guard<std::mutex> lock(compileMutex_);
    auto slots = shadeopSlots_.find(shaderName);
    if (slots == shadeopSlots_.end())
        return -1;
    void **array = shadeopArrays_[shaderName];
    for (size_t k = 0; k < slots->second.size(); ++k) {
        void *entry = resolve(slots->second[k].first, slots->second[k].second);
        if (entry == nullptr) {
            if (failedName)
                *failedName = slots->second[k].first;
            if (failedPrototype)
                *failedPrototype = slots->second[k].second;
            return static_cast<int>(k);
        }
        array[k] = entry;
    }
    return -1;
}

// =========================================================================
// CLLVMJitEngine::lookupInitEntry
// =========================================================================
TShaderJitEntry CLLVMJitEngine::lookupInitEntry(const std::string &shaderName) {
    auto it = initCache_.find(shaderName);
    return (it != initCache_.end()) ? it->second : nullptr;
}
