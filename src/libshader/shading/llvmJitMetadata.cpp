/**
 * Project: openRender
 *
 * File: llvmJitMetadata.cpp
 *
 * Description:
 *   CLLVMJitEngine::extractMetadataFromFile / extractMetadataFromModule: read-only
 *   .slo bitcode metadata probe. Deliberately self-contained (LLVM bitcode APIs
 *   only, no ri/CRenderer symbols) so it can be built into the standalone
 *   libshader_jitmeta static library and linked by tools such as sloinfo that
 *   don't otherwise link ri or libshader_shading.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "common/sloFormat.h"
#include "llvmJit.h"

// LLVM headers generate warnings under our strict flags; suppress them for this block only.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#pragma GCC diagnostic pop

// -------------------------------------------------------------------------
// Internal helper: extract one MDString field from a metadata node operand.
// Returns empty string if the operand is not an MDString.
// -------------------------------------------------------------------------
static std::string mdStr(const llvm::MDNode *node, unsigned idx) {
    if (!node || idx >= node->getNumOperands())
        return {};
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
    p.name = mdStr(node, 0);
    p.typeName = mdStr(node, 1);
    p.storage = mdStr(node, 2);
    p.writable = (mdStr(node, 3) == "true");
    p.arraySize = 1;
    p.defaultStr = mdStr(node, 5);
    const std::string szStr = mdStr(node, 4);
    if (!szStr.empty())
        p.arraySize = std::stoi(szStr);
    return p;
}

// =========================================================================
// CLLVMJitEngine::readFormat  (static, private)
// =========================================================================
bool CLLVMJitEngine::readFormat(const llvm::Module &mod, SFormatVersion &version) {
    const llvm::NamedMDNode *nmd = mod.getNamedMetadata(SLO_FORMAT_KEY);
    if (!nmd || nmd->getNumOperands() != 1)
        return false;
    const llvm::MDNode *node = nmd->getOperand(0);
    if (node->getNumOperands() != 3)
        return false;
    int part[3];
    for (unsigned i = 0; i < 3; ++i) {
        const auto *value = llvm::mdconst::dyn_extract<llvm::ConstantInt>(node->getOperand(i));
        if (!value || value->getSExtValue() < 0 || value->getSExtValue() > 1000000)
            return false;
        part[i] = static_cast<int>(value->getSExtValue());
    }
    version.major = part[0];
    version.minor = part[1];
    version.patch = part[2];
    return true;
}

// =========================================================================
// CLLVMJitEngine::extractMetadataFromModule  (static, private)
// =========================================================================
bool CLLVMJitEngine::extractMetadataFromModule(const llvm::Module &mod, SLOShaderInfo &info) {
    auto getFirst = [&](const char *key) -> const llvm::MDNode * {
        const llvm::NamedMDNode *nmd = mod.getNamedMetadata(key);
        if (!nmd || nmd->getNumOperands() == 0)
            return nullptr;
        return nmd->getOperand(0);
    };

    // Shader name
    const llvm::MDNode *nameNode = getFirst("openrender.shader.name");
    if (!nameNode)
        return false;
    info.name = mdStr(nameNode, 0);
    if (info.name.empty())
        return false;

    // Shader type
    const llvm::MDNode *typeNode = getFirst("openrender.shader.type");
    info.typeName = typeNode ? mdStr(typeNode, 0) : "surface";

    // Version
    const llvm::MDNode *verNode = getFirst("openrender.shader.version");
    // The record holds a "major.minor.patch" string; the field keeps the major number
    // (1 when there is no record or it does not parse).
    info.version = 1;
    if (verNode) {
        SFormatVersion parsed;
        if (parseFormatVersion(mdStr(verNode, 0).c_str(), parsed))
            info.version = parsed.major;
    }

    // Parameters
    const llvm::NamedMDNode *params = mod.getNamedMetadata("openrender.shader.params");
    if (params) {
        for (unsigned i = 0; i < params->getNumOperands(); ++i) {
            SLOParamInfo p = parseParamNode(params->getOperand(i));
            if (!p.name.empty())
                info.params.push_back(std::move(p));
        }
    }

    // Local variables
    const llvm::NamedMDNode *vars = mod.getNamedMetadata("openrender.shader.vars");
    if (vars) {
        for (unsigned i = 0; i < vars->getNumOperands(); ++i) {
            SLOParamInfo v = parseParamNode(vars->getOperand(i));
            if (!v.name.empty())
                info.vars.push_back(std::move(v));
        }
    }

    // usedParameters bitmask (decimal string; 0 for pre-C3 .slo files)
    const llvm::MDNode *upNode = getFirst("openrender.shader.usedparameters");
    if (upNode) {
        const std::string us = mdStr(upNode, 0);
        if (!us.empty()) {
            try {
                info.usedParameters = static_cast<unsigned>(std::stoul(us));
            }
            catch (...) {
                info.usedParameters = 0;
            }
        }
    }

    // Confirm this is really a JIT-callable shader: a defined (not merely
    // declared) function named `info.name` with the entry signature
    // void(i32 numVertices, ptr stuff, ptr tags) — the exact lookup the
    // JIT performs at bind time (see CLLVMJitEngine::compileShader).
    if (const llvm::Function *fn = mod.getFunction(info.name)) {
        if (!fn->isDeclaration() && fn->arg_size() == 3 &&
            fn->getReturnType()->isVoidTy() &&
            fn->getFunctionType()->getParamType(0)->isIntegerTy(32) &&
            fn->getFunctionType()->getParamType(1)->isPointerTy() &&
            fn->getFunctionType()->getParamType(2)->isPointerTy()) {
            info.hasJitEntry = true;
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
    if (!bufferOrErr)
        return false;

    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto moduleOrErr = llvm::parseBitcodeFile(bufferOrErr.get()->getMemBufferRef(), *ctx);
    if (!moduleOrErr) {
        llvm::consumeError(moduleOrErr.takeError());
        return false;
    }

    return extractMetadataFromModule(**moduleOrErr, info);
}
