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

// LLVM headers generate warnings under our strict flags; suppress them for this block only.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#pragma GCC diagnostic pop

CLLVMJitEngine::CLLVMJitEngine() {
    auto jitOrErr = llvm::orc::LLJITBuilder().create();
    if (!jitOrErr) {
        log_error("Failed to create LLJIT instance: {}", llvm::toString(jitOrErr.takeError()));
        return;
    }
    jit = std::move(*jitOrErr);
}

CLLVMJitEngine::~CLLVMJitEngine() {
    // LLJIT instance will be destroyed automatically
}

CLLVMJitEngine& CLLVMJitEngine::getInstance() {
    static CLLVMJitEngine instance;
    return instance;
}

TShaderJitEntry CLLVMJitEngine::compileShader(const std::string &filename, const std::string &shaderName) {
    if (!jit) return nullptr;

    std::lock_guard<std::mutex> lock(compileMutex_);

    // Return cached entry point if already compiled.
    auto cached = cache_.find(shaderName);
    if (cached != cache_.end())
        return cached->second;

    // Load the bitcode from the .slo file
    auto bufferOrErr = llvm::MemoryBuffer::getFile(filename);
    if (!bufferOrErr) {
        log_error("Failed to open shader file '{}': {}", filename, bufferOrErr.getError().message());
        return nullptr;
    }

    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto moduleOrErr = llvm::parseBitcodeFile(bufferOrErr.get()->getMemBufferRef(), *ctx);
    if (!moduleOrErr) {
        log_error("Failed to parse bitcode in '{}': {}", filename, llvm::toString(moduleOrErr.takeError()));
        return nullptr;
    }

    auto &module = *moduleOrErr;

    // Add the module to the JIT
    if (auto err = jit->addIRModule(llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx)))) {
        log_error("Failed to add IR module to JIT: {}", llvm::toString(std::move(err)));
        return nullptr;
    }

    // Look up the entry point
    auto symOrErr = jit->lookup(shaderName);
    if (!symOrErr) {
        log_error("Failed to find shader entry point '{}' in JIT: {}", shaderName, llvm::toString(symOrErr.takeError()));
        return nullptr;
    }

    TShaderJitEntry entry = reinterpret_cast<TShaderJitEntry>(symOrErr->getValue());
    cache_[shaderName] = entry;
    return entry;
}
