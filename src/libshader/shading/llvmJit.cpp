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
#include "error.h"
#include "logging.hpp"

#include <cstdio>

// LLVM headers generate warnings under our strict flags; suppress them for this block only.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Metadata.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#pragma GCC diagnostic pop

// CLLVMJitEngine::extractMetadataFromModule / extractMetadataFromFile now live in
// llvmJitMetadata.cpp, built into the standalone libshader_jitmeta static library.
// That code touches only LLVM bitcode APIs (no ri/CRenderer symbols), so tools like
// sloinfo that only need metadata inspection can link it without pulling in the rest
// of libshader_shading (whose -undefined dynamic_lookup symbols are resolved only
// when ri.dylib loads it).

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
    } else {
        jit->getMainJITDylib().addGenerator(std::move(*dlsgOrErr));
    }
}

CLLVMJitEngine::~CLLVMJitEngine() {}

CLLVMJitEngine& CLLVMJitEngine::getInstance() {
    static CLLVMJitEngine instance;
    return instance;
}

// =========================================================================
// CLLVMJitEngine::getCachedMetadata
// =========================================================================
bool CLLVMJitEngine::getCachedMetadata(const std::string &shaderName, SLOShaderInfo &info) const {
    auto it = metaCache_.find(shaderName);
    if (it == metaCache_.end()) return false;
    info = it->second;
    return true;
}

// =========================================================================
// CLLVMJitEngine::compileShader
// =========================================================================
TShaderJitEntry CLLVMJitEngine::compileShader(const std::string &filename,
                                               const std::string &shaderName) {
    if (!jit) return nullptr;

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

    // Extract named metadata before transferring ownership to the JIT.
    SLOShaderInfo meta;
    bool hasInit = false;
    if (extractMetadataFromModule(*module, meta)) {
        metaCache_[shaderName] = std::move(meta);
        // Check for non-trivial init section.
        const llvm::NamedMDNode *initMD = module->getNamedMetadata("openrender.shader.hasinit");
        hasInit = (initMD && initMD->getNumOperands() > 0);
    }

    // Add the module to the JIT (transfers ownership).
    if (auto err = jit->addIRModule(
            llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx)))) {
        log_error("Failed to add IR module to JIT: {}", llvm::toString(std::move(err)));
        return nullptr;
    }

    // Look up the main entry point.
    auto symOrErr = jit->lookup(shaderName);
    if (!symOrErr) {
        log_error("Failed to find shader entry '{}' in JIT: {}", shaderName,
                  llvm::toString(symOrErr.takeError()));
        return nullptr;
    }

    TShaderJitEntry entry = reinterpret_cast<TShaderJitEntry>(symOrErr->getValue());
    cache_[shaderName] = entry;

    // Cache init entry if present.
    if (hasInit) {
        const std::string initName = shaderName + "_init";
        auto initOrErr = jit->lookup(initName);
        if (initOrErr) {
            initCache_[shaderName] =
                reinterpret_cast<TShaderJitEntry>(initOrErr->getValue());
        } else {
            llvm::consumeError(initOrErr.takeError());
        }
    }

    return entry;
}

// =========================================================================
// CLLVMJitEngine::lookupInitEntry
// =========================================================================
TShaderJitEntry CLLVMJitEngine::lookupInitEntry(const std::string &shaderName) {
    auto it = initCache_.find(shaderName);
    return (it != initCache_.end()) ? it->second : nullptr;
}
