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
#include <llvm/IR/Metadata.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#pragma GCC diagnostic pop

// -------------------------------------------------------------------------
// Internal helper: extract one MDString field from a metadata node operand.
// Returns empty string if the operand is not an MDString.
// -------------------------------------------------------------------------
static std::string mdStr(const llvm::MDNode *node, unsigned idx) {
    if (!node || idx >= node->getNumOperands()) return {};
    if (auto *s = llvm::dyn_cast<llvm::MDString>(node->getOperand(idx)))
        return s->getString().str();
    return {};
}

// -------------------------------------------------------------------------
// Internal helper: parse the per-parameter/variable metadata node.
// Node operand layout: [name, typeName, storage, writable, arraySize, default]
// -------------------------------------------------------------------------
static SLOParamInfo parseParamNode(const llvm::MDNode *node) {
    SLOParamInfo p;
    p.name       = mdStr(node, 0);
    p.typeName   = mdStr(node, 1);
    p.storage    = mdStr(node, 2);
    p.writable   = (mdStr(node, 3) == "true");
    p.arraySize  = 1;
    p.defaultStr = mdStr(node, 5);
    const std::string szStr = mdStr(node, 4);
    if (!szStr.empty()) p.arraySize = std::stoi(szStr);
    return p;
}

// =========================================================================
// CLLVMJitEngine::extractMetadataFromModule  (static, private)
// =========================================================================
bool CLLVMJitEngine::extractMetadataFromModule(const llvm::Module &mod, SLOShaderInfo &info) {
    auto getFirst = [&](const char *key) -> const llvm::MDNode * {
        const llvm::NamedMDNode *nmd = mod.getNamedMetadata(key);
        if (!nmd || nmd->getNumOperands() == 0) return nullptr;
        return nmd->getOperand(0);
    };

    // Shader name
    const llvm::MDNode *nameNode = getFirst("openrender.shader.name");
    if (!nameNode) return false;
    info.name = mdStr(nameNode, 0);
    if (info.name.empty()) return false;

    // Shader type
    const llvm::MDNode *typeNode = getFirst("openrender.shader.type");
    info.typeName = typeNode ? mdStr(typeNode, 0) : "surface";

    // Version
    const llvm::MDNode *verNode = getFirst("openrender.shader.version");
    info.version = 1;
    if (verNode) {
        const std::string vs = mdStr(verNode, 0);
        if (!vs.empty()) info.version = std::stoi(vs);
    }

    // Parameters
    const llvm::NamedMDNode *params = mod.getNamedMetadata("openrender.shader.params");
    if (params) {
        for (unsigned i = 0; i < params->getNumOperands(); ++i) {
            SLOParamInfo p = parseParamNode(params->getOperand(i));
            if (!p.name.empty()) info.params.push_back(std::move(p));
        }
    }

    // Local variables
    const llvm::NamedMDNode *vars = mod.getNamedMetadata("openrender.shader.vars");
    if (vars) {
        for (unsigned i = 0; i < vars->getNumOperands(); ++i) {
            SLOParamInfo v = parseParamNode(vars->getOperand(i));
            if (!v.name.empty()) info.vars.push_back(std::move(v));
        }
    }

    return true;
}

// =========================================================================
// CLLVMJitEngine::extractMetadataFromFile  (static, public)
// Read-only probe — no JIT engine needed.
// =========================================================================
bool CLLVMJitEngine::extractMetadataFromFile(const std::string &filename, SLOShaderInfo &info) {
    auto bufferOrErr = llvm::MemoryBuffer::getFile(filename);
    if (!bufferOrErr) return false;

    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto moduleOrErr = llvm::parseBitcodeFile(bufferOrErr.get()->getMemBufferRef(), *ctx);
    if (!moduleOrErr) {
        llvm::consumeError(moduleOrErr.takeError());
        return false;
    }

    return extractMetadataFromModule(**moduleOrErr, info);
}

// =========================================================================
// CLLVMJitEngine constructor / destructor / getInstance
// =========================================================================
CLLVMJitEngine::CLLVMJitEngine() {
    auto jitOrErr = llvm::orc::LLJITBuilder().create();
    if (!jitOrErr) {
        log_error("Failed to create LLJIT instance: {}", llvm::toString(jitOrErr.takeError()));
        return;
    }
    jit = std::move(*jitOrErr);
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
    if (extractMetadataFromModule(*module, meta))
        metaCache_[shaderName] = std::move(meta);

    // Add the module to the JIT (transfers ownership).
    if (auto err = jit->addIRModule(
            llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx)))) {
        log_error("Failed to add IR module to JIT: {}", llvm::toString(std::move(err)));
        return nullptr;
    }

    // Look up the entry point.
    auto symOrErr = jit->lookup(shaderName);
    if (!symOrErr) {
        log_error("Failed to find shader entry '{}' in JIT: {}", shaderName,
                  llvm::toString(symOrErr.takeError()));
        return nullptr;
    }

    TShaderJitEntry entry = reinterpret_cast<TShaderJitEntry>(symOrErr->getValue());
    cache_[shaderName] = entry;
    return entry;
}
