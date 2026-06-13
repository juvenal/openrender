/**
 * File: llvmEmitter.cpp
 * Description: CLLVMEmitter — RSL IR → LLVM bitcode (.slo) compiler backend.
 *
 * Entry signature emitted:
 *   void @shadername(i32 numVertices, ptr stuff, ptr tags)
 *   stuff[0] = void** constantEntries (SL_IMMEDIATE_OPERAND)
 *   stuff[1] = float** varying        (SL_GLOBAL_OPERAND   — RSL globals P,N,Ci…)
 *   stuff[2] = float** locals         (SL_VARYING_OPERAND  — shader params + temps)
 *
 * Strides passed to op_* functions:
 *   0  = uniform (one value, no per-vertex advance)
 *   1  = varying float
 *   3  = varying vector
 *  16  = varying matrix
 */

#include "llvmEmitter.h"
#include "ir.h"
#include "rslo.h"   // SLC_* bit constants (compiler-side)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Metadata.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#pragma GCC diagnostic pop

#include <unordered_map>
#include <string>
#include <vector>
#include <cstdio>

// =========================================================================
// RSL global variable name → VARIABLE_* index (slot 1 / SL_GLOBAL_OPERAND)
// =========================================================================
static const std::unordered_map<std::string, int> s_rslGlobals = {
    {"P", 0}, {"Ps", 1}, {"N", 2}, {"Ng", 3}, {"dPdu", 4}, {"dPdv", 5},
    {"L", 6}, {"Cs", 7}, {"Os", 8}, {"Cl", 9}, {"Ol", 10},
    {"Ci", 11}, {"Oi", 12},
    {"s", 13}, {"t", 14}, {"du", 15}, {"dv", 16}, {"u", 17}, {"v", 18},
    {"I", 19}, {"E", 20}, {"alpha", 21}, {"time", 22}, {"Pw", 23},
    {"ncomps", 24}, {"dtime", 25}, {"dPdtime", 26},
    {"width", 27}, {"constantwidth", 28},
};

// RSL global default strides (0=uniform-scalar, 1=varying-float, 3=varying-vector, 16=matrix)
static const std::unordered_map<std::string, int> s_rslGlobalStrides = {
    {"P", 3}, {"Ps", 3}, {"N", 3}, {"Ng", 3}, {"dPdu", 3}, {"dPdv", 3},
    {"L", 3}, {"Cs", 3}, {"Os", 3}, {"Cl", 3}, {"Ol", 3},
    {"Ci", 3}, {"Oi", 3},
    {"s", 1}, {"t", 1}, {"du", 1}, {"dv", 1}, {"u", 1}, {"v", 1},
    {"I", 3}, {"E", 3}, {"alpha", 1}, {"time", 0}, {"Pw", 3},
    {"ncomps", 0}, {"dtime", 0}, {"dPdtime", 3},
    {"width", 1}, {"constantwidth", 0},
};

// =========================================================================
// VarDesc — describes where to find a variable in the stuff[][] arrays.
// =========================================================================
struct VarDesc {
    int slot;    // 0=constants, 1=globals/varying, 2=locals
    int idx;     // index within the slot array
    int stride;  // 0=uniform, 1=float, 3=vector, 16=matrix
};

// =========================================================================
// Build variable table from IRModule
// =========================================================================
static std::unordered_map<std::string, VarDesc>
buildVarTable(const IRModule &mod) {
    std::unordered_map<std::string, VarDesc> tbl;

    // Walk IR vars: parameters come first, then local temporaries.
    // Both go into slot 2 (SL_VARYING_OPERAND = locals[]).
    int slot2Idx = 0;
    for (const IRVarInfo &v : mod.vars) {
        if (v.slcType & SLC_GLOBAL) {
            // RSL global — handled separately via s_rslGlobals
            continue;
        }
        // Compute stride from type bits.
        int stride = 0;
        if (v.slcType & SLC_UNIFORM) {
            stride = 0;
        } else if (v.slcType & SLC_MATRIX) {
            stride = 16;
        } else if (v.slcType & SLC_VECTOR) {
            stride = 3;
        } else {
            stride = 1; // default: varying float
        }

        tbl[v.cName]     = {2, slot2Idx, stride};
        tbl[v.symbolName]= {2, slot2Idx, stride};
        ++slot2Idx;
    }

    // RSL globals live in slot 1; add them from the static table.
    for (const auto &[name, idx] : s_rslGlobals) {
        auto strideIt = s_rslGlobalStrides.find(name);
        int  stride   = (strideIt != s_rslGlobalStrides.end()) ? strideIt->second : 3;
        tbl[name] = {1, idx, stride};
    }

    return tbl;
}

// =========================================================================
// Embed C3 named metadata in the LLVM module.
// =========================================================================
static void embedMetadata(llvm::Module &mod,
                          const std::string &shaderName,
                          const IRModule &ir,
                          llvm::LLVMContext &ctx) {
    auto mkStr = [&](const std::string &s) -> llvm::Metadata * {
        return llvm::MDString::get(ctx, s);
    };
    auto mkMD  = [&](llvm::Metadata *m) { return llvm::MDNode::get(ctx, m); };

    mod.getOrInsertNamedMetadata("openrender.shader.name")
        ->addOperand(mkMD(mkStr(shaderName)));
    mod.getOrInsertNamedMetadata("openrender.shader.type")
        ->addOperand(mkMD(mkStr(ir.shaderType)));
    mod.getOrInsertNamedMetadata("openrender.shader.version")
        ->addOperand(mkMD(mkStr(ir.version)));

    auto embedVars = [&](const char *key, int slcFilter) {
        llvm::NamedMDNode *nmd = mod.getOrInsertNamedMetadata(key);
        for (const IRVarInfo &v : ir.vars) {
            if ((v.slcType & SLC_PARAMETER) == 0 && slcFilter == SLC_PARAMETER) continue;
            if ((v.slcType & SLC_PARAMETER) != 0 && slcFilter == 0) continue;
            if (v.slcType & SLC_GLOBAL) continue;

            const std::string typeStr = (v.slcType & SLC_FLOAT) ? "float" :
                                        (v.slcType & SLC_MATRIX) ? "matrix" :
                                        (v.slcType & SLC_STRING) ? "string" :
                                        (v.slcType & SLC_VPOINT)  ? "point" :
                                        (v.slcType & SLC_VNORMAL) ? "normal" :
                                        (v.slcType & SLC_VCOLOR)  ? "color" :
                                        (v.slcType & SLC_VECTOR)  ? "vector" : "float";
            const std::string storage = (v.slcType & SLC_UNIFORM) ? "uniform" : "varying";
            const std::string writable = (v.slcType & SLC_OUTPUT) ? "true" : "false";
            const std::string szStr = std::to_string(v.numItems);

            llvm::Metadata *fields[] = {
                mkStr(v.symbolName), mkStr(typeStr), mkStr(storage),
                mkStr(writable), mkStr(szStr), mkStr(v.defaultValue)
            };
            nmd->addOperand(llvm::MDNode::get(ctx, fields));
        }
    };

    embedVars("openrender.shader.params", SLC_PARAMETER);
    embedVars("openrender.shader.vars", 0);
}

// =========================================================================
// External function declaration helper
// =========================================================================
static llvm::Function *declareOp(llvm::Module &mod, const std::string &name,
                                  llvm::FunctionType *ty) {
    if (auto *existing = mod.getFunction(name)) return existing;
    return llvm::Function::Create(ty, llvm::Function::ExternalLinkage, name, &mod);
}

// =========================================================================
// Generate LLVM IR for one IRFunction (init or code section).
// =========================================================================
static void emitFunction(const IRFunction &irFn,
                         llvm::BasicBlock *entryBB,
                         llvm::Value *numVerts,
                         llvm::Value *slot1,  // RSL globals
                         llvm::Value *slot2,  // shader locals
                         llvm::Value *tags,
                         const std::unordered_map<std::string, VarDesc> &varTbl,
                         llvm::LLVMContext &ctx,
                         llvm::Module &mod) {
    llvm::IRBuilder<> B(entryBB);
    auto *i32Ty = llvm::Type::getInt32Ty(ctx);
    auto *ptrTy = llvm::PointerType::getUnqual(ctx);

    // Helper: given a VarDesc, emit GEP+load to get the float* pointer.
    auto loadVarPtr = [&](const VarDesc &d) -> llvm::Value * {
        llvm::Value *slot = (d.slot == 1) ? slot1 : slot2;
        auto *gep  = B.CreateGEP(ptrTy, slot, B.getInt32(d.idx));
        return B.CreateLoad(ptrTy, gep);
    };

    // Helper: look up a variable by name; returns nullptr for literals.
    auto resolveVar = [&](const std::string &tok, VarDesc &out) -> bool {
        auto it = varTbl.find(tok);
        if (it == varTbl.end()) return false;
        out = it->second;
        return true;
    };

    // Pre-declare common function types.
    auto *voidTy = llvm::Type::getVoidTy(ctx);

    // Type for binary vector/float op: (ptr,i32, ptr,i32, ptr,i32, i32, ptr)
    auto *binOpTy = llvm::FunctionType::get(voidTy,
        {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
    // Type for unary op: (ptr,i32, ptr,i32, i32, ptr)
    auto *unOpTy = llvm::FunctionType::get(voidTy,
        {ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
    // Ternary op (clamp/mix): (ptr,i32, ptr,i32, ptr,i32, ptr,i32, i32, ptr)
    auto *ternOpTy = llvm::FunctionType::get(voidTy,
        {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);

    for (const IRBlock &blk : irFn.blocks) {
        for (const IRInstr &ins : blk.instrs) {
            const std::string &op = ins.opcode;

            // --- return ---
            if (op == "return") { B.CreateRetVoid(); return; }

            // Skip labels and flow control for now (no-op).
            if (op == "if" || op == "else" || op == "endif" ||
                op == "for" || op == "forbegin" || op == "endfor" ||
                op == "while" || op == "whilebegin" || op == "endwhile" ||
                op == "break" || op == "continue" || op == "jmp") {
                continue;
            }

            // Resolve operand variables.
            auto getVar = [&](int i) -> std::pair<llvm::Value*, int> {
                if (i >= (int)ins.operands.size()) return {nullptr, 0};
                const std::string &tok = ins.operands[i].token;
                VarDesc d; bool ok = resolveVar(tok, d);
                if (!ok) return {nullptr, 0};
                return {loadVarPtr(d), d.stride};
            };

            // Destination (result variable).
            VarDesc dstDesc;
            bool hasDst = !ins.result.empty() && resolveVar(ins.result, dstDesc);
            llvm::Value *dst = hasDst ? loadVarPtr(dstDesc) : nullptr;
            llvm::Value *dstStride = B.getInt32(hasDst ? dstDesc.stride : 3);

            // --- Binary vector/float ops ---
            auto emitBin = [&](const char *name) {
                auto [a, sa] = getVar(0);
                auto [b, sb] = getVar(1);
                if (!dst || !a || !b) return;
                auto *fn = declareOp(mod, name, binOpTy);
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa), b, B.getInt32(sb), numVerts, tags});
            };
            auto emitUn = [&](const char *name) {
                auto [a, sa] = getVar(0);
                if (!dst || !a) return;
                auto *fn = declareOp(mod, name, unOpTy);
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa), numVerts, tags});
            };
            auto emitTern = [&](const char *name) {
                auto [a, sa] = getVar(0);
                auto [b, sb] = getVar(1);
                auto [c, sc] = getVar(2);
                if (!dst || !a || !b || !c) return;
                auto *fn = declareOp(mod, name, ternOpTy);
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa), b, B.getInt32(sb), c, B.getInt32(sc), numVerts, tags});
            };

            if      (op == "addvv")      emitBin("op_addvv");
            else if (op == "subvv")      emitBin("op_subvv");
            else if (op == "mulvv")      emitBin("op_mulvv");
            else if (op == "divvv")      emitBin("op_divvv");
            else if (op == "addff")      emitBin("op_addff");
            else if (op == "subff")      emitBin("op_subff");
            else if (op == "mulff")      emitBin("op_mulff");
            else if (op == "divff")      emitBin("op_divff");
            else if (op == "addvf" || op == "addvf2") emitBin("op_addvf");
            else if (op == "subvf")      emitBin("op_subvf");
            else if (op == "mulvf" || op == "mulvf2") emitBin("op_mulvf");
            else if (op == "divvf")      emitBin("op_divvf");
            else if (op == "dot")        emitBin("op_dot");
            else if (op == "cross")      emitBin("op_cross");
            else if (op == "pow")        emitBin("op_pow");
            else if (op == "mod")        emitBin("op_mod");
            else if (op == "atan2")      emitBin("op_atan2");
            else if (op == "flt")        emitBin("op_flt");
            else if (op == "fle")        emitBin("op_fle");
            else if (op == "fgt")        emitBin("op_fgt");
            else if (op == "fge")        emitBin("op_fge");
            else if (op == "feq")        emitBin("op_feq");
            else if (op == "fne")        emitBin("op_fne");

            else if (op == "movevv")     emitUn("op_movevv");
            else if (op == "moveff")     emitUn("op_moveff");
            else if (op == "negv")       emitUn("op_negv");
            else if (op == "negf")       emitUn("op_negf");
            else if (op == "normalize")  emitUn("op_normalize");
            else if (op == "length")     emitUn("op_length");
            else if (op == "sqrt")       emitUn("op_sqrt");
            else if (op == "inversesqrt")emitUn("op_inversesqrt");
            else if (op == "abs")        emitUn("op_abs");
            else if (op == "sign")       emitUn("op_sign");
            else if (op == "floor")      emitUn("op_floor");
            else if (op == "ceil")       emitUn("op_ceil");
            else if (op == "exp")        emitUn("op_exp");
            else if (op == "log")        emitUn("op_log");
            else if (op == "sin")        emitUn("op_sin");
            else if (op == "cos")        emitUn("op_cos");
            else if (op == "tan")        emitUn("op_tan");
            else if (op == "asin")       emitUn("op_asin");
            else if (op == "acos")       emitUn("op_acos");
            else if (op == "atan")       emitUn("op_atan");
            else if (op == "xcomp")      emitUn("op_xcomp");
            else if (op == "ycomp")      emitUn("op_ycomp");
            else if (op == "zcomp")      emitUn("op_zcomp");
            else if (op == "vfromf")     emitUn("op_vfromf");

            else if (op == "clampf")     emitTern("op_clampf");
            else if (op == "clampv")     emitTern("op_clampv");
            else if (op == "mixf")       emitTern("op_mixf");
            else if (op == "mixv")       emitTern("op_mixv");

            else if (op == "faceforward") {
                auto [nIn, sn] = getVar(0);
                auto [iIn, si] = getVar(1);
                auto [ngIn, sng] = (ins.operands.size() > 2) ? getVar(2) : std::make_pair((llvm::Value*)nullptr, 0);
                if (!dst || !nIn || !iIn) continue;
                // op_faceforward(dst,sd, n,sn, i,si, ng,sng, n_verts, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_faceforward", ty);
                if (!ngIn) ngIn = nIn, sng = sn;  // use N as Ng fallback
                B.CreateCall(fn, {dst, dstStride, nIn, B.getInt32(sn),
                                  iIn, B.getInt32(si), ngIn, B.getInt32(sng),
                                  numVerts, tags});
            }
            else if (op == "vfromvff") {
                // op_vfromvff(dst,sd, v,sv, f1,s1, f2,s2, n, tags)
                auto [v, sv] = getVar(0);
                auto [f1,s1] = getVar(1);
                auto [f2,s2] = getVar(2);
                if (!dst || !v || !f1 || !f2) continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_vfromvff", ty);
                B.CreateCall(fn, {dst, dstStride, v, B.getInt32(sv),
                                  f1, B.getInt32(s1), f2, B.getInt32(s2),
                                  numVerts, tags});
            }
            else if (op == "vfromfff") {
                auto [f0,s0] = getVar(0); auto [f1,s1] = getVar(1); auto [f2,s2] = getVar(2);
                if (!dst || !f0 || !f1 || !f2) continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_vfromfff", ty);
                B.CreateCall(fn, {dst, dstStride, f0, B.getInt32(s0),
                                  f1, B.getInt32(s1), f2, B.getInt32(s2),
                                  numVerts, tags});
            }
            else if (op == "setxcomp" || op == "setycomp" || op == "setzcomp") {
                auto [a, sa] = getVar(0);
                if (!dst || !a) continue;
                // op_setxcomp(v,sv, f,sf, n, tags) — note: dst is first (v), src is second (f)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                const char *fnName = (op == "setxcomp") ? "op_setxcomp" :
                                     (op == "setycomp") ? "op_setycomp" : "op_setzcomp";
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa), numVerts, tags});
            }
            else if (op == "vuvector") {
                // op_vuvector(dst, src) — uniform vector copy (no loop)
                auto [a, sa] = getVar(0);
                if (!dst || !a) continue;
                auto *ty = llvm::FunctionType::get(voidTy, {ptrTy, ptrTy}, false);
                auto *fn = declareOp(mod, "op_vuvector", ty);
                B.CreateCall(fn, {dst, a});
            }
            else if (op == "vufloat") {
                // op_vufloat(dst, val) — treat as uniform moveff
                auto [a, sa] = getVar(0);
                if (!dst || !a) continue;
                auto *fn = declareOp(mod, "op_moveff", unOpTy);
                B.CreateCall(fn, {dst, B.getInt32(0), a, B.getInt32(0), numVerts, tags});
            }

            // --- Lighting ---
            else if (op == "ambient") {
                if (!dst) continue;
                // op_ambient_batch(result, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy, {ptrTy, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_ambient_batch", ty);
                B.CreateCall(fn, {dst, numVerts, tags});
            }
            else if (op == "diffuse") {
                auto [nf, sn] = getVar(0);
                if (!dst || !nf) continue;
                // op_diffuse_batch(result, sr, Nf, sn, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_diffuse_batch", ty);
                B.CreateCall(fn, {dst, dstStride, nf, B.getInt32(sn), numVerts, tags});
            }
            else if (op == "specular") {
                auto [nf, sn] = getVar(0);
                auto [v,  sv] = getVar(1);
                auto [r,  sr] = getVar(2);
                if (!dst || !nf || !v || !r) continue;
                // op_specular_batch(result, Nf, V, roughness, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy, ptrTy, ptrTy, ptrTy, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_specular_batch", ty);
                B.CreateCall(fn, {dst, nf, v, r, numVerts, tags});
            }
            else if (op == "smoothstep") {
                auto [e0,s0] = getVar(0); auto [e1,s1] = getVar(1); auto [x,sx] = getVar(2);
                if (!dst || !e0 || !e1 || !x) continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "rsl_smoothstep", ty);
                B.CreateCall(fn, {dst, dstStride, e0, B.getInt32(s0),
                                  e1, B.getInt32(s1), x, B.getInt32(sx), numVerts, tags});
            }
            // Unrecognised — skip silently (a warning could be added here).
        }
    }

    // Ensure a terminator exists.
    if (!B.GetInsertBlock()->getTerminator())
        B.CreateRetVoid();
}

// =========================================================================
// Public entry point: emitLLVMBitcode
// =========================================================================
bool emitLLVMBitcode(const IRModule &mod,
                     const std::string &outPath,
                     const std::string &shaderName) {
    llvm::LLVMContext ctx;
    auto llvmMod = std::make_unique<llvm::Module>(shaderName, ctx);
    // Leave target triple empty; the JIT resolves the target at load time.

    auto *voidTy = llvm::Type::getVoidTy(ctx);
    auto *i32Ty  = llvm::Type::getInt32Ty(ctx);
    auto *ptrTy  = llvm::PointerType::getUnqual(ctx);

    // Entry function: void @shadername(i32 numVertices, ptr stuff, ptr tags)
    auto *funcTy = llvm::FunctionType::get(voidTy, {i32Ty, ptrTy, ptrTy}, false);
    auto *func   = llvm::Function::Create(
        funcTy, llvm::Function::ExternalLinkage, shaderName, llvmMod.get());

    auto args = func->arg_begin();
    llvm::Value *numVerts = &*args++;
    llvm::Value *stuffPtr = &*args++;
    llvm::Value *tags     = &*args++;

    // Extract slot pointers from stuff.
    auto *entry = llvm::BasicBlock::Create(ctx, "entry", func);
    llvm::IRBuilder<> B(entry);

    auto *slot1_pp = B.CreateGEP(ptrTy, stuffPtr, B.getInt32(1), "globals_pp");
    auto *slot1    = B.CreateLoad(ptrTy, slot1_pp, "globals");
    auto *slot2_pp = B.CreateGEP(ptrTy, stuffPtr, B.getInt32(2), "locals_pp");
    auto *slot2    = B.CreateLoad(ptrTy, slot2_pp, "locals");

    // Build variable lookup table.
    auto varTbl = buildVarTable(mod);

    // Emit code section (the main shader body).
    emitFunction(mod.codeFn, entry,
                 numVerts, slot1, slot2, tags,
                 varTbl, ctx, *llvmMod);

    // Embed C3 named metadata.
    embedMetadata(*llvmMod, shaderName, mod, ctx);

    // Write bitcode to file.
    std::error_code ec;
    llvm::raw_fd_ostream os(outPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        fprintf(stderr, "llvmEmitter: cannot open '%s': %s\n",
                outPath.c_str(), ec.message().c_str());
        return false;
    }
    llvm::WriteBitcodeToFile(*llvmMod, os);
    return true;
}
