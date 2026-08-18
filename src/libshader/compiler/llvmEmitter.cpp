/**
 * File: llvmEmitter.cpp
 * Description: CLLVMEmitter — RSL IR → LLVM bitcode (.slo) compiler backend.
 *
 * Entry signature emitted:
 *   void @shadername(i32 numVertices, ptr stuff, ptr tags)
 *   void @shadername_init(i32 numVertices, ptr stuff, ptr tags)  [if init section non-trivial]
 *
 *   stuff[0] = void** constantEntries (SL_IMMEDIATE_OPERAND — unused in JIT path)
 *   stuff[1] = float** varying        (SL_GLOBAL_OPERAND   — RSL globals P,N,Ci…)
 *   stuff[2] = float** locals         (SL_VARYING_OPERAND  — shader params + temps)
 *
 * Strides passed to op_* functions:
 *   0  = uniform (one value, no per-vertex advance)
 *   1  = varying float
 *   3  = varying vector
 *  16  = varying matrix
 *
 * Layers implemented here (see plan):
 *   A — Literal constant resolution   (allocLiteral helper + getVar fallback)
 *   B — Init section compilation      (shadername_init function + hasinit metadata)
 *   C — Control flow if/else/endif    (op_if_update / op_else_update / op_endif_update)
 *   D — illuminate / endilluminate    (op_illuminate_begin / op_illuminate_end)
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
#include <llvm/IR/Verifier.h>
#pragma GCC diagnostic pop

#include <unordered_map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cerrno>

// =========================================================================
// kHandledOpcodes — single source of truth for every mnemonic emitFunction()
// dispatches on. Seeded from the current if/else-if chain (spec
// 011-jit-opcode-parity T003); the libshader coverage-guard ctest links
// this symbol directly (research.md D3), so it must stay in sync with the
// chain below — any new `else if (op == "...")` case added to emitFunction()
// needs a matching entry here or it silently regresses to a no-op via the
// gate a few lines below.
// =========================================================================
const char *const kHandledOpcodes[] = {
    "abs", "acos", "addff", "addvf", "addvf2", "addvv", "ambient", "and",
    "andf", "area", "asin", "atan", "atan2", "break", "calculatenormal",
    "ceil", "cellnoise", "cfrom", "clamp", "clampf", "clampv", "continue",
    "cos", "cross", "ctransform",
    "depth", "diffuse", "divff", "divvf", "divvv", "dot", "Du", "Dv",
    "else", "endfor", "endif", "endilluminance", "endilluminate",
    "endsolar", "endwhile", "environment", "exp", "faceforward", "felt",
    "feq", "feql", "fge", "fgt", "filterstep", "fle", "floor", "flt",
    "fne", "fneql", "for", "forbegin", "forend", "fresnel", "ftoa", "if",
    "illuminance", "illuminate", "inversesqrt", "jmp", "length",
    "lightsource", "log", "max", "maxf", "mfrom", "mix", "mixf", "mixv",
    "mod", "moveff", "movevv", "mulff", "mulvf", "mulvf2", "mulvv", "negf",
    "negv", "nfrom", "noise", "normalize", "ntransform", "or", "orf",
    "pfrom", "pow", "printf", "radians", "reflect", "return", "seql",
    "setxcomp", "setycomp", "setzcomp", "shadow", "sign", "sin",
    "smoothstep", "sneql", "snoise", "solar", "specular", "spline",
    "sqrt", "subff", "subvf", "subvv", "tan", "texture", "transform",
    "vfrom", "vfromf", "vfromfff", "vfromvff", "vtransform", "vufloat",
    "vuvector", "while", "whilebegin", "xcomp", "ycomp", "zcomp",
    nullptr
};

static bool isHandledOpcode(const std::string &op) {
    for (int i = 0; kHandledOpcodes[i] != nullptr; ++i) {
        if (op == kHandledOpcodes[i]) return true;
    }
    return false;
}

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

// RSL global strides: 0=uniform-scalar, 1=varying-float, 3=varying-vector, 16=matrix
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
// Build variable table from IRModule.
// Order: parameters first (SLC_PARAMETER), then locals — matching the order
// in which the .rslo runtime assigns locals[] indices.
// =========================================================================
static std::unordered_map<std::string, VarDesc>
buildVarTable(const IRModule &mod) {
    std::unordered_map<std::string, VarDesc> tbl;

    int slot2Idx = 0;

    // Parameters first (SLC_PARAMETER set, SLC_GLOBAL clear)
    for (const IRVarInfo &v : mod.vars) {
        if (v.slcType & SLC_GLOBAL)    continue;
        if (!(v.slcType & SLC_PARAMETER)) continue;

        int stride = 0;
        if (v.slcType & SLC_UNIFORM)      stride = 0;
        else if (v.slcType & SLC_MATRIX)  stride = 16;
        else if (v.slcType & SLC_VECTOR)  stride = 3;
        else                              stride = 1;

        tbl[v.cName]      = {2, slot2Idx, stride};
        tbl[v.symbolName] = {2, slot2Idx, stride};
        ++slot2Idx;
    }

    // Local temporaries (SLC_PARAMETER clear, SLC_GLOBAL clear)
    for (const IRVarInfo &v : mod.vars) {
        if (v.slcType & SLC_GLOBAL)    continue;
        if (v.slcType & SLC_PARAMETER) continue;

        int stride = 0;
        if (v.slcType & SLC_UNIFORM)      stride = 0;
        else if (v.slcType & SLC_MATRIX)  stride = 16;
        else if (v.slcType & SLC_VECTOR)  stride = 3;
        else                              stride = 1;

        tbl[v.cName]      = {2, slot2Idx, stride};
        tbl[v.symbolName] = {2, slot2Idx, stride};
        ++slot2Idx;
    }

    // RSL globals in slot 1
    for (const auto &[name, idx] : s_rslGlobals) {
        auto strideIt = s_rslGlobalStrides.find(name);
        int  stride   = (strideIt != s_rslGlobalStrides.end()) ? strideIt->second : 3;
        tbl[name] = {1, idx, stride};
    }

    return tbl;
}

// =========================================================================
// Embed named metadata in the LLVM module.
// =========================================================================
static void embedMetadata(llvm::Module &mod,
                          const std::string &shaderName,
                          const IRModule &ir,
                          bool hasInit,
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

    if (hasInit) {
        mod.getOrInsertNamedMetadata("openrender.shader.hasinit")
            ->addOperand(mkMD(mkStr("1")));
    }

    // Compute usedParameters bitmask from RSL globals used in the shader.
    {
        static const struct { const char *name; unsigned int bit; } kParamBits[] = {
            {"s",        1u},
            {"t",        1u << 1},
            {"u",        1u << 2},
            {"v",        1u << 3},
            {"du",       (1u << 4) | (1u << 14)},
            {"dv",       (1u << 5) | (1u << 14)},
            {"time",     1u << 6},
            {"dtime",    1u << 7},
            {"ncomps",   1u << 8},
            {"alpha",    1u << 9},
            {"P",        1u << 10},
            {"Ps",       1u << 11},
            {"Pw",       1u << 10},
            {"dPdu",     1u << 12},
            {"dPdv",     1u << 13},
            {"dPdtime",  1u << 15},
            {"Ng",       1u << 16},
            {"N",        (1u << 17) | (1u << 16)},
            {"Ci",       1u << 18},
            {"Oi",       1u << 19},
            {"Cl",       1u << 20},
            {"Ol",       1u << 21},
            {"Cs",       1u << 22},
            {"Os",       1u << 23},
            {"E",        1u << 24},
            {"I",        1u << 25},
            {"L",        1u << 26},
        };
        unsigned int usedParams = 0;
        for (const IRVarInfo &v : ir.vars) {
            if (!(v.slcType & SLC_GLOBAL)) continue;
            for (const auto &e : kParamBits) {
                if (v.symbolName == e.name) { usedParams |= e.bit; break; }
            }
        }
        // PARAMETER_NONAMBIENT (bit 30): set only when the shader uses illuminate/solar/illuminance.
        // An ambient light shader does NOT use these opcodes and must NOT have this bit set.
        bool hasNonAmbientOp = false;
        for (const IRFunction *fn : {&ir.initFn, &ir.codeFn}) {
            for (const IRBlock &blk : fn->blocks) {
                for (const IRInstr &ins : blk.instrs) {
                    if (ins.opcode == "illuminate" || ins.opcode == "solar" ||
                        ins.opcode == "illuminance")
                        hasNonAmbientOp = true;
                }
            }
        }
        if (hasNonAmbientOp)
            usedParams |= (1u << 30); // PARAMETER_NONAMBIENT
        mod.getOrInsertNamedMetadata("openrender.shader.usedparameters")
            ->addOperand(mkMD(mkStr(std::to_string(usedParams))));
    }

    auto embedVars = [&](const char *key, int slcFilter) {
        llvm::NamedMDNode *nmd = mod.getOrInsertNamedMetadata(key);
        for (const IRVarInfo &v : ir.vars) {
            if ((v.slcType & SLC_PARAMETER) == 0 && slcFilter == SLC_PARAMETER) continue;
            if ((v.slcType & SLC_PARAMETER) != 0 && slcFilter == 0) continue;
            if (v.slcType & SLC_GLOBAL) continue;

            const std::string typeStr = (v.slcType & SLC_FLOAT)   ? "float"  :
                                        (v.slcType & SLC_MATRIX)  ? "matrix" :
                                        (v.slcType & SLC_STRING)  ? "string" :
                                        (v.slcType & SLC_VPOINT)  ? "point"  :
                                        (v.slcType & SLC_VNORMAL) ? "normal" :
                                        (v.slcType & SLC_VCOLOR)  ? "color"  :
                                        (v.slcType & SLC_VECTOR)  ? "vector" : "float";
            const std::string storage = (v.slcType & SLC_UNIFORM) ? "uniform" : "varying";
            const std::string writable = (v.slcType & SLC_OUTPUT) ? "true" : "false";

            llvm::Metadata *fields[] = {
                mkStr(v.symbolName), mkStr(typeStr), mkStr(storage),
                mkStr(writable), mkStr(std::to_string(v.numItems)), mkStr(v.defaultValue)
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
// Attempt to parse `tok` as a numeric literal.
// On success fills `*out` and returns true; leaves *out unchanged on failure.
// =========================================================================
static bool parseLiteralFloat(const std::string &tok, float *out) {
    if (tok.empty()) return false;
    char *end = nullptr;
    errno = 0;
    float v = std::strtof(tok.c_str(), &end);
    if (errno != 0 || end == tok.c_str() || *end != '\0') return false;
    *out = v;
    return true;
}

// =========================================================================
// Generate LLVM IR for one IRFunction (init or code section).
//
// Parameters:
//   irFn     — the IR function to compile
//   func     — the LLVM function (for creating new BBs in Layers E/F)
//   entryBB  — existing entry basic block (slot loads already added); emission
//              continues at the END of this block via IRBuilder
//   numVerts — i32 argument: number of vertices in the batch
//   slot1    — ptr argument: varying[] (RSL globals)
//   slot2    — ptr argument: locals[]  (shader params + temps)
//   tags     — ptr argument: per-vertex activity tags (may be nullptr for init)
//   varTbl   — variable descriptor table
//   ctx, mod — LLVM context and module
// =========================================================================
static void emitFunction(const IRFunction &irFn,
                         llvm::Function *func,   // for new BBs in loop layers
                         llvm::BasicBlock *entryBB,
                         llvm::Value *numVerts,
                         llvm::Value *slot1,
                         llvm::Value *slot2,
                         llvm::Value *tags,
                         const std::unordered_map<std::string, VarDesc> &varTbl,
                         llvm::LLVMContext &ctx,
                         llvm::Module &mod) {
    // Insert at the END of the existing entry block (after slot loads).
    llvm::IRBuilder<> B(entryBB);

    // -----------------------------------------------------------------------
    // Layer E: illuminance loop scope tracking.
    // When an `illuminance` instruction is seen, we create 3 basic blocks
    // (body, latch, exit) and record the exit-label so the outer block loop
    // can wire them when the exit block is entered.
    // -----------------------------------------------------------------------
    struct IllumScope {
        std::string            exitLabel; // "#!LabelN" of the endilluminance block
        llvm::BasicBlock      *bodyBB;
        llvm::BasicBlock      *latchBB;
        llvm::BasicBlock      *exitBB;
    };
    std::vector<IllumScope> illumStack;

    // -----------------------------------------------------------------------
    // Layer F: for / while loop scope tracking.
    // forbegin condLabel latchLabel exitLabel
    //   condBB  — condition + first body instructions (up to 'for' check)
    //   bodyBB  — post-condition body (switched to by 'for' instruction)
    //   latchBB — increment block (at latchLabel boundary)
    //   exitBB  — post-loop code (at exitLabel boundary, emits op_forend)
    // -----------------------------------------------------------------------
    struct ForScope {
        std::string       latchLabel;   // "#!LabelN" of the increment block
        std::string       exitLabel;    // "#!LabelN" of the forend block
        llvm::BasicBlock *condBB;
        llvm::BasicBlock *bodyBB;
        llvm::BasicBlock *latchBB;
        llvm::BasicBlock *exitBB;
        llvm::Value      *execCountPtr; // alloca i32 for forExecCount
    };
    std::vector<ForScope> forStack;

    auto *i32Ty  = llvm::Type::getInt32Ty(ctx);
    auto *f32Ty  = llvm::Type::getFloatTy(ctx);
    auto *ptrTy  = llvm::PointerType::getUnqual(ctx);
    auto *voidTy = llvm::Type::getVoidTy(ctx);

    // -----------------------------------------------------------------------
    // Layer C: numActive / numPassive stack slots for conditional execution.
    // op_if_update / op_else_update / op_endif_update write into these.
    // -----------------------------------------------------------------------
    auto *numActivePtr  = B.CreateAlloca(i32Ty, nullptr, "numActive");
    auto *numPassivePtr = B.CreateAlloca(i32Ty, nullptr, "numPassive");
    B.CreateStore(numVerts, numActivePtr);
    B.CreateStore(B.getInt32(0), numPassivePtr);

    // -----------------------------------------------------------------------
    // Helper: load a float* pointer from one of the stuff[] slot arrays.
    // -----------------------------------------------------------------------
    auto loadVarPtr = [&](const VarDesc &d) -> llvm::Value * {
        llvm::Value *slot = (d.slot == 1) ? slot1 : slot2;
        auto *gep = B.CreateGEP(ptrTy, slot, B.getInt32(d.idx));
        return B.CreateLoad(ptrTy, gep);
    };

    // -----------------------------------------------------------------------
    // Helper: resolve a named variable → (float*, stride).
    // -----------------------------------------------------------------------
    auto resolveVar = [&](const std::string &tok, VarDesc &out) -> bool {
        auto it = varTbl.find(tok);
        if (it == varTbl.end()) return false;
        out = it->second;
        return true;
    };

    // -----------------------------------------------------------------------
    // Layer A: allocLiteral — allocate a stack float for a numeric literal.
    // Returns {alloca_ptr, 0} on success or {nullptr, 0} if not a literal.
    // -----------------------------------------------------------------------
    auto allocLiteral = [&](const std::string &tok) -> std::pair<llvm::Value*, int> {
        float v = 0.f;
        if (!parseLiteralFloat(tok, &v)) return {nullptr, 0};
        auto *alloca = B.CreateAlloca(f32Ty, nullptr, "lit");
        B.CreateStore(llvm::ConstantFP::get(f32Ty, v), alloca);
        return {alloca, 0};
    };

    // -----------------------------------------------------------------------
    // getVar: resolve operand at position i.
    // Tries variable table first, then falls back to literal allocation.
    // Returns {nullptr, 0} if neither succeeds.
    // -----------------------------------------------------------------------
    auto getVar = [&](const IRInstr &ins, int i) -> std::pair<llvm::Value*, int> {
        if (i >= (int)ins.operands.size()) return {nullptr, 0};
        const std::string &tok = ins.operands[i].token;

        VarDesc d;
        if (resolveVar(tok, d)) return {loadVarPtr(d), d.stride};

        // Layer A: numeric literal
        auto lit = allocLiteral(tok);
        if (lit.first) return lit;

        return {nullptr, 0};
    };

    // -----------------------------------------------------------------------
    // Pre-declare common function types.
    // -----------------------------------------------------------------------
    // Binary: (dst,sd, a,sa, b,sb, n, tags)
    auto *binOpTy = llvm::FunctionType::get(voidTy,
        {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
    // Unary: (dst,sd, a,sa, n, tags)
    auto *unOpTy = llvm::FunctionType::get(voidTy,
        {ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
    // Ternary: (dst,sd, a,sa, b,sb, c,sc, n, tags)
    auto *ternOpTy = llvm::FunctionType::get(voidTy,
        {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
    // op_if_update(cond,scond, tags, n, numActive*, numPassive*)
    auto *ifUpdTy = llvm::FunctionType::get(voidTy,
        {ptrTy,i32Ty, ptrTy, i32Ty, ptrTy, ptrTy}, false);
    // op_else_update / op_endif_update(tags, n, numActive*, numPassive*)
    auto *elseUpdTy = llvm::FunctionType::get(voidTy,
        {ptrTy, i32Ty, ptrTy, ptrTy}, false);

    // -----------------------------------------------------------------------
    // Emit helpers for common op patterns.
    // -----------------------------------------------------------------------
    auto emitBin = [&](const IRInstr &ins, const char *name,
                        llvm::Value *dst, llvm::Value *dstStride) {
        auto [a, sa] = getVar(ins, 0);
        auto [b, sb] = getVar(ins, 1);
        if (!dst || !a || !b) return;
        auto *fn = declareOp(mod, name, binOpTy);
        B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa), b, B.getInt32(sb), numVerts, tags});
    };

    auto emitUn = [&](const IRInstr &ins, const char *name,
                       llvm::Value *dst, llvm::Value *dstStride) {
        auto [a, sa] = getVar(ins, 0);
        if (!dst || !a) return;
        auto *fn = declareOp(mod, name, unOpTy);
        B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa), numVerts, tags});
    };

    auto emitTern = [&](const IRInstr &ins, const char *name,
                         llvm::Value *dst, llvm::Value *dstStride) {
        auto [a, sa] = getVar(ins, 0);
        auto [b, sb] = getVar(ins, 1);
        auto [c, sc] = getVar(ins, 2);
        if (!dst || !a || !b || !c) return;
        auto *fn = declareOp(mod, name, ternOpTy);
        B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa),
                          b, B.getInt32(sb), c, B.getInt32(sc), numVerts, tags});
    };

    // -----------------------------------------------------------------------
    // Main instruction loop.
    // if/else/endif  — handled by tag-update calls (no LLVM branches).
    // illuminance    — generates real LLVM loop structure (Layer E).
    // -----------------------------------------------------------------------
    for (const IRBlock &blk : irFn.blocks) {

        // -----------------------------------------------------------------
        // Layer F: block-boundary checks for for/while loop.
        // -----------------------------------------------------------------
        if (!blk.label.empty() && !forStack.empty()) {
            ForScope &fs = forStack.back();
            if (blk.label == fs.latchLabel) {
                // Body falls through to latch.
                if (!B.GetInsertBlock()->getTerminator())
                    B.CreateBr(fs.latchBB);
                B.SetInsertPoint(fs.latchBB);
            }
            if (blk.label == fs.exitLabel) {
                // Latch falls through back to condition (loop back).
                if (!B.GetInsertBlock()->getTerminator())
                    B.CreateBr(fs.condBB);
                // Emit forend in the exit BB.
                B.SetInsertPoint(fs.exitBB);
                auto *endTy = llvm::FunctionType::get(voidTy,
                    {ptrTy, ptrTy, i32Ty, ptrTy, ptrTy}, false);
                auto *endFn = declareOp(mod, "op_forend", endTy);
                B.CreateCall(endFn, {fs.execCountPtr, tags, numVerts,
                                     numActivePtr, numPassivePtr});
                forStack.pop_back();
            }
        }

        // -----------------------------------------------------------------
        // Layer E: block-boundary check for illuminance scope exit.
        // When we enter the block whose label matches the innermost scope's
        // exitLabel, we close the loop: fall through body → latch → check,
        // then set the insert point to the exit BB.
        // -----------------------------------------------------------------
        if (!blk.label.empty() && !illumStack.empty() &&
                blk.label == illumStack.back().exitLabel) {
            IllumScope &sc = illumStack.back();

            // If the body BB has no terminator yet (normal fall-through path),
            // branch to the latch.
            if (!B.GetInsertBlock()->getTerminator())
                B.CreateBr(sc.latchBB);

            // Emit latch: call op_illuminance_next, branch body or exit.
            B.SetInsertPoint(sc.latchBB);
            auto *nextTy = llvm::FunctionType::get(i32Ty,
                {ptrTy, i32Ty, ptrTy, ptrTy}, false);
            auto *nextFn = declareOp(mod, "op_illuminance_next", nextTy);
            auto *more   = B.CreateCall(nextFn,
                               {tags, numVerts, numActivePtr, numPassivePtr});
            auto *hasmr  = B.CreateICmpNE(more, B.getInt32(0));
            B.CreateCondBr(hasmr, sc.bodyBB, sc.exitBB);

            // Continue emitting into the exit BB.
            B.SetInsertPoint(sc.exitBB);
            illumStack.pop_back();
        }

        for (const IRInstr &ins : blk.instrs) {
            const std::string &op = ins.opcode;

            // ----------------------------------------------------------------
            // Terminator
            // ----------------------------------------------------------------
            if (op == "return") {
                if (!B.GetInsertBlock()->getTerminator())
                    B.CreateRetVoid();
                return;
            }

            // ----------------------------------------------------------------
            // Labels / jumps — no-ops in the flat batch model.
            // Tags control per-vertex activity; we never skip instructions.
            // ----------------------------------------------------------------
            if (op == "jmp") continue;

            // ----------------------------------------------------------------
            // Resolve destination variable.
            // ----------------------------------------------------------------
            VarDesc dstDesc{};
            bool    hasDst = !ins.result.empty() && resolveVar(ins.result, dstDesc);
            llvm::Value *dst      = hasDst ? loadVarPtr(dstDesc) : nullptr;
            llvm::Value *dstStride = B.getInt32(hasDst ? dstDesc.stride : 3);

            // Coverage gate: kHandledOpcodes is the single source of truth
            // the libshader coverage-guard ctest also reads (research.md
            // D3). An opcode not in the table falls through here exactly as
            // it always has (silent skip, zero emitted IR) — this does not
            // change behavior for any opcode already dispatched below; it
            // only makes that fallthrough consult the same table the guard
            // test checks, instead of the two silently drifting apart.
            if (!isHandledOpcode(op)) continue;

            // ================================================================
            // Layer C: Conditional control flow
            // ================================================================
            if (op == "if") {
                auto [cond, sc] = getVar(ins, 0);
                if (cond) {
                    auto *fn = declareOp(mod, "op_if_update", ifUpdTy);
                    B.CreateCall(fn, {cond, B.getInt32(sc), tags, numVerts,
                                      numActivePtr, numPassivePtr});
                }
                continue;
            }
            if (op == "else") {
                auto *fn = declareOp(mod, "op_else_update", elseUpdTy);
                B.CreateCall(fn, {tags, numVerts, numActivePtr, numPassivePtr});
                continue;
            }
            if (op == "endif") {
                auto *fn = declareOp(mod, "op_endif_update", elseUpdTy);
                B.CreateCall(fn, {tags, numVerts, numActivePtr, numPassivePtr});
                continue;
            }

            // ================================================================
            // Layer D: illuminate / endilluminate
            // op_illuminate_begin/end are in rslOps.h
            // ================================================================
            if (op == "illuminate") {
                auto [from, sf] = getVar(ins, 0);
                if (ins.operands.size() >= 4) {
                    // ILLUMINATE3: illuminate from axis angle #!Label
                    auto [axis, sa] = getVar(ins, 1);
                    auto [ang,  st] = getVar(ins, 2);
                    if (from && axis && ang) {
                        // (from,sf, axis,sa, angle,st, tags, n, numActive*, numPassive*)
                        auto *ty = llvm::FunctionType::get(voidTy,
                            {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty,
                             ptrTy, i32Ty, ptrTy, ptrTy}, false);
                        auto *fn = declareOp(mod, "op_illuminate3_begin", ty);
                        B.CreateCall(fn, {from, B.getInt32(sf),
                                          axis, B.getInt32(sa),
                                          ang,  B.getInt32(st),
                                          tags, numVerts, numActivePtr, numPassivePtr});
                    }
                } else if (from) {
                    // ILLUMINATE1: illuminate from #!Label
                    auto *ty = llvm::FunctionType::get(voidTy,
                        {ptrTy,i32Ty, ptrTy, i32Ty, ptrTy, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_illuminate_begin", ty);
                    B.CreateCall(fn, {from, B.getInt32(sf), tags, numVerts,
                                      numActivePtr, numPassivePtr});
                }
                continue;
            }
            if (op == "endilluminate") {
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy, i32Ty, ptrTy, ptrTy}, false);
                auto *fn = declareOp(mod, "op_illuminate_end", ty);
                B.CreateCall(fn, {tags, numVerts, numActivePtr, numPassivePtr});
                continue;
            }

            // ================================================================
            // Layer D2: solar / endsolar (directional light shaders)
            // solar Nf thetaf #!LabelExit
            // ================================================================
            if (op == "solar") {
                // operand 0 = Nf (vector direction), 1 = thetaf (float), 2 = label (ignored)
                auto [Nf, sf] = getVar(ins, 0);
                auto [th, st] = getVar(ins, 1);
                if (Nf && th) {
                    // (Nf, sf, thetaf, st, tags, n, numActive*, numPassive*)
                    auto *ty = llvm::FunctionType::get(voidTy,
                        {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, ptrTy, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_solar_begin", ty);
                    B.CreateCall(fn, {Nf, B.getInt32(sf), th, B.getInt32(st),
                                      tags, numVerts, numActivePtr, numPassivePtr});
                }
                continue;
            }
            if (op == "endsolar") {
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy, i32Ty, ptrTy, ptrTy}, false);
                auto *fn = declareOp(mod, "op_solar_end", ty);
                B.CreateCall(fn, {tags, numVerts, numActivePtr, numPassivePtr});
                continue;
            }

            // ================================================================
            // Layer E: illuminance loop
            // op_illuminance_begin/next manage the lights list.
            // LLVM structure:  begin → (body | exit)
            //                  latch → begin|next → (body | exit)
            // ================================================================
            if (op == "illuminance") {
                // Operands: [P, N, angle, bodyLabel, exitLabel]
                // operand 3 = bodyLabel (the block we switch into, already handled
                //             by the fact we switch insert point right here)
                // operand 4 = exitLabel (the block that starts the endilluminance)
                auto [P,  sp] = getVar(ins, 0);
                auto [N,  sn] = getVar(ins, 1);
                auto [ang,sa] = getVar(ins, 2);
                // operand 3 = body label (unused: body starts right after branch)
                std::string exitLabel;
                if (ins.operands.size() >= 5)
                    exitLabel = ins.operands[4].token;  // "#!LabelN"

                // Function type: (ptr,i32, ptr,i32, ptr,i32, ptr, i32, ptr*, ptr*) → i32
                auto *beginTy = llvm::FunctionType::get(i32Ty,
                    {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty,
                     ptrTy, i32Ty, ptrTy, ptrTy}, false);
                auto *beginFn = declareOp(mod, "op_illuminance_begin", beginTy);

                llvm::BasicBlock *bodyBB  = nullptr;
                llvm::BasicBlock *latchBB = nullptr;
                llvm::BasicBlock *exitBB  = nullptr;

                if (!P || !N || !ang || exitLabel.empty()) {
                    // Malformed — skip body entirely (stub behaviour)
                    continue;
                }

                bodyBB  = llvm::BasicBlock::Create(ctx, "illum_body",  func);
                latchBB = llvm::BasicBlock::Create(ctx, "illum_latch", func);
                exitBB  = llvm::BasicBlock::Create(ctx, "illum_exit",  func);

                auto *has = B.CreateCall(beginFn,
                    {P,  B.getInt32(sp),
                     N,  B.getInt32(sn),
                     ang, B.getInt32(sa),
                     tags, numVerts, numActivePtr, numPassivePtr});
                auto *ok  = B.CreateICmpNE(has, B.getInt32(0));
                B.CreateCondBr(ok, bodyBB, exitBB);

                // Switch to body BB — subsequent instructions emit there.
                B.SetInsertPoint(bodyBB);

                illumStack.push_back({exitLabel, bodyBB, latchBB, exitBB});
                continue;
            }
            // endilluminance: handled at the block boundary in the outer loop.
            // By the time this instruction is reached, we have already switched
            // the insert point to the exitBB and popped the scope.
            if (op == "endilluminance") continue;

            // ================================================================
            // Layer F: for / while loop
            // forbegin condLabel latchLabel exitLabel
            // ================================================================
            if (op == "forbegin" || op == "whilebegin") {
                // Operands: [condLabel, latchLabel, exitLabel]
                std::string latchLabel, exitLabel;
                if (ins.operands.size() >= 2) latchLabel = ins.operands[1].token;
                if (ins.operands.size() >= 3) exitLabel  = ins.operands[2].token;
                if (latchLabel.empty() || exitLabel.empty()) continue;

                auto *condBB  = llvm::BasicBlock::Create(ctx, "for_cond",  func);
                auto *bodyBB  = llvm::BasicBlock::Create(ctx, "for_body",  func);
                auto *latchBB = llvm::BasicBlock::Create(ctx, "for_latch", func);
                auto *exitBB  = llvm::BasicBlock::Create(ctx, "for_exit",  func);

                auto *execCountPtr = B.CreateAlloca(i32Ty, nullptr, "forExecCount");
                B.CreateStore(B.getInt32(0), execCountPtr);

                // Fall from current block into condition block.
                B.CreateBr(condBB);
                B.SetInsertPoint(condBB);

                forStack.push_back({latchLabel, exitLabel,
                                    condBB, bodyBB, latchBB, exitBB,
                                    execCountPtr});
                continue;
            }

            // 'for condVar': condition check; may also appear as 'while'.
            if (op == "for" || op == "while") {
                // forStack should be non-empty (well-formed IR).
                if (forStack.empty()) continue;
                ForScope &fs = forStack.back();

                auto [cond, sc] = getVar(ins, 0);
                if (cond) {
                    // void op_for_check(cond, sc, execCountPtr, tags, n, numActive*, numPassive*)
                    auto *chkTy = llvm::FunctionType::get(voidTy,
                        {ptrTy,i32Ty, ptrTy, ptrTy, i32Ty, ptrTy, ptrTy}, false);
                    auto *chkFn = declareOp(mod, "op_for_check", chkTy);
                    B.CreateCall(chkFn, {cond, B.getInt32(sc),
                                         fs.execCountPtr, tags, numVerts,
                                         numActivePtr, numPassivePtr});
                }
                // Branch: numActive > 0 → body, else → exit.
                auto *na  = B.CreateLoad(i32Ty, numActivePtr);
                auto *ok  = B.CreateICmpNE(na, B.getInt32(0));
                B.CreateCondBr(ok, fs.bodyBB, fs.exitBB);
                B.SetInsertPoint(fs.bodyBB);
                continue;
            }

            // 'forend' / 'endfor': handled at the exitLabel block boundary.
            if (op == "forend" || op == "endfor" || op == "endwhile") continue;

            // 'break': mark all active vertices as loop-exited.
            if (op == "break") {
                if (!forStack.empty()) {
                    auto *brkTy = llvm::FunctionType::get(voidTy,
                        {ptrTy, ptrTy, i32Ty, ptrTy, ptrTy}, false);
                    auto *brkFn = declareOp(mod, "op_for_break", brkTy);
                    B.CreateCall(brkFn, {forStack.back().execCountPtr,
                                         tags, numVerts, numActivePtr, numPassivePtr});
                }
                continue;
            }

            // 'continue': jump to the latch (increment) block.
            if (op == "continue") {
                if (!forStack.empty() && !B.GetInsertBlock()->getTerminator())
                    B.CreateBr(forStack.back().latchBB);
                continue;
            }

            // ================================================================
            // Binary arithmetic
            // ================================================================
            if      (op == "addvv")             emitBin(ins, "op_addvv", dst, dstStride);
            else if (op == "subvv")             emitBin(ins, "op_subvv", dst, dstStride);
            else if (op == "mulvv")             emitBin(ins, "op_mulvv", dst, dstStride);
            else if (op == "divvv")             emitBin(ins, "op_divvv", dst, dstStride);
            else if (op == "addff")             emitBin(ins, "op_addff", dst, dstStride);
            else if (op == "subff")             emitBin(ins, "op_subff", dst, dstStride);
            else if (op == "mulff")             emitBin(ins, "op_mulff", dst, dstStride);
            else if (op == "divff")             emitBin(ins, "op_divff", dst, dstStride);
            else if (op == "addvf" || op == "addvf2") emitBin(ins, "op_addvf", dst, dstStride);
            else if (op == "subvf")             emitBin(ins, "op_subvf", dst, dstStride);
            else if (op == "mulvf" || op == "mulvf2") emitBin(ins, "op_mulvf", dst, dstStride);
            else if (op == "divvf")             emitBin(ins, "op_divvf", dst, dstStride);
            else if (op == "dot")               emitBin(ins, "op_dot",   dst, dstStride);
            else if (op == "cross")             emitBin(ins, "op_cross", dst, dstStride);
            else if (op == "pow")               emitBin(ins, "op_pow",   dst, dstStride);
            else if (op == "mod")               emitBin(ins, "op_mod",   dst, dstStride);
            else if (op == "atan2")             emitBin(ins, "op_atan2", dst, dstStride);
            else if (op == "flt")               emitBin(ins, "op_flt",   dst, dstStride);
            else if (op == "fle")               emitBin(ins, "op_fle",   dst, dstStride);
            else if (op == "fgt")               emitBin(ins, "op_fgt",   dst, dstStride);
            else if (op == "fge")               emitBin(ins, "op_fge",   dst, dstStride);
            else if (op == "feq")               emitBin(ins, "op_feq",   dst, dstStride);
            else if (op == "fne")               emitBin(ins, "op_fne",   dst, dstStride);

            // ================================================================
            // Unary arithmetic / math
            // ================================================================
            else if (op == "movevv")         emitUn(ins, "op_movevv",      dst, dstStride);
            else if (op == "moveff")         emitUn(ins, "op_moveff",      dst, dstStride);
            else if (op == "negv")           emitUn(ins, "op_negv",        dst, dstStride);
            else if (op == "negf")           emitUn(ins, "op_negf",        dst, dstStride);
            else if (op == "normalize")      emitUn(ins, "op_normalize",   dst, dstStride);
            else if (op == "length")         emitUn(ins, "op_length",      dst, dstStride);
            else if (op == "sqrt")           emitUn(ins, "op_sqrt",        dst, dstStride);
            else if (op == "inversesqrt")    emitUn(ins, "op_inversesqrt", dst, dstStride);
            else if (op == "abs")            emitUn(ins, "op_abs",         dst, dstStride);
            else if (op == "sign")           emitUn(ins, "op_sign",        dst, dstStride);
            else if (op == "floor")          emitUn(ins, "op_floor",       dst, dstStride);
            else if (op == "ceil")           emitUn(ins, "op_ceil",        dst, dstStride);
            else if (op == "exp")            emitUn(ins, "op_exp",         dst, dstStride);
            else if (op == "log")            emitUn(ins, "op_log",         dst, dstStride);
            else if (op == "sin")            emitUn(ins, "op_sin",         dst, dstStride);
            else if (op == "cos")            emitUn(ins, "op_cos",         dst, dstStride);
            else if (op == "tan")            emitUn(ins, "op_tan",         dst, dstStride);
            else if (op == "asin")           emitUn(ins, "op_asin",        dst, dstStride);
            else if (op == "acos")           emitUn(ins, "op_acos",        dst, dstStride);
            else if (op == "atan")           emitUn(ins, "op_atan",        dst, dstStride);
            else if (op == "xcomp")          emitUn(ins, "op_xcomp",       dst, dstStride);
            else if (op == "ycomp")          emitUn(ins, "op_ycomp",       dst, dstStride);
            else if (op == "zcomp")          emitUn(ins, "op_zcomp",       dst, dstStride);

            // ================================================================
            // Ternary (clamp, mix)
            // ================================================================
            else if (op == "clampf")         emitTern(ins, "op_clampf", dst, dstStride);
            else if (op == "clampv")         emitTern(ins, "op_clampv", dst, dstStride);
            // Generic clamp: dispatch by proto (f=fff → clampf, else → clampv)
            else if (op == "clamp") {
                bool isFloat = !ins.proto.empty() && ins.proto[0] == 'f';
                emitTern(ins, isFloat ? "op_clampf" : "op_clampv", dst, dstStride);
            }
            else if (op == "mixf")           emitTern(ins, "op_mixf",   dst, dstStride);
            else if (op == "mixv")           emitTern(ins, "op_mixv",   dst, dstStride);
            // Generic mix: dispatch by proto (f=fff → mixf, else → mixv)
            else if (op == "mix") {
                bool isFloat = !ins.proto.empty() && ins.proto[0] == 'f';
                emitTern(ins, isFloat ? "op_mixf" : "op_mixv", dst, dstStride);
            }

            // ================================================================
            // Data movement — vufloat / vuvector (uniform broadcast)
            // Source stride is always 0 (uniform), regardless of declared stride.
            // ================================================================
            else if (op == "vuvector") {
                auto [a, sa] = getVar(ins, 0);
                if (!dst || !a) continue;
                auto *fn = declareOp(mod, "op_movevv", unOpTy);
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(0), numVerts, tags});
            }
            else if (op == "vufloat") {
                auto [a, sa] = getVar(ins, 0);
                if (!dst || !a) continue;
                auto *fn = declareOp(mod, "op_moveff", unOpTy);
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(0), numVerts, tags});
            }

            // ================================================================
            // Vector construction
            // vfromf: 1 operand → broadcast; 3 operands → construct from floats
            // ================================================================
            else if (op == "vfromf") {
                if (ins.operands.size() >= 3) {
                    // 3-operand form: construct (f0, f1, f2)
                    auto [f0,s0] = getVar(ins, 0);
                    auto [f1,s1] = getVar(ins, 1);
                    auto [f2,s2] = getVar(ins, 2);
                    if (!dst || !f0 || !f1 || !f2) continue;
                    auto *fn = declareOp(mod, "op_vfromfff", ternOpTy);
                    B.CreateCall(fn, {dst, dstStride,
                                      f0, B.getInt32(s0), f1, B.getInt32(s1),
                                      f2, B.getInt32(s2), numVerts, tags});
                } else {
                    // 1-operand form: broadcast single float
                    emitUn(ins, "op_vfromf", dst, dstStride);
                }
            }
            else if (op == "vfromvff") {
                auto [v, sv] = getVar(ins, 0);
                auto [f1,s1] = getVar(ins, 1);
                auto [f2,s2] = getVar(ins, 2);
                if (!dst || !v || !f1 || !f2) continue;
                auto *fn = declareOp(mod, "op_vfromvff",
                    llvm::FunctionType::get(voidTy,
                        {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false));
                B.CreateCall(fn, {dst, dstStride, v, B.getInt32(sv),
                                  f1, B.getInt32(s1), f2, B.getInt32(s2), numVerts, tags});
            }
            else if (op == "vfromfff") {
                auto [f0,s0] = getVar(ins, 0);
                auto [f1,s1] = getVar(ins, 1);
                auto [f2,s2] = getVar(ins, 2);
                if (!dst || !f0 || !f1 || !f2) continue;
                auto *fn = declareOp(mod, "op_vfromfff", ternOpTy);
                B.CreateCall(fn, {dst, dstStride,
                                  f0, B.getInt32(s0), f1, B.getInt32(s1),
                                  f2, B.getInt32(s2), numVerts, tags});
            }

            // ================================================================
            // Component set/get
            // ================================================================
            else if (op == "setxcomp" || op == "setycomp" || op == "setzcomp") {
                auto [a, sa] = getVar(ins, 0);
                if (!dst || !a) continue;
                const char *fnName = (op == "setxcomp") ? "op_setxcomp" :
                                     (op == "setycomp") ? "op_setycomp" : "op_setzcomp";
                auto *fn = declareOp(mod, fnName, unOpTy);
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa), numVerts, tags});
            }

            // ================================================================
            // Geometry
            // ================================================================
            else if (op == "faceforward") {
                auto [nIn,  sn]  = getVar(ins, 0);
                auto [iIn,  si]  = getVar(ins, 1);
                auto [ngIn, sng] = (ins.operands.size() > 2)
                                   ? getVar(ins, 2)
                                   : std::make_pair((llvm::Value*)nullptr, 0);
                if (!dst || !nIn || !iIn) continue;
                if (!ngIn) {
                    VarDesc ngDesc;
                    if (resolveVar("Ng", ngDesc)) {
                        ngIn = loadVarPtr(ngDesc);
                        sng  = ngDesc.stride;
                    } else {
                        ngIn = nIn; sng = sn;
                    }
                }
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_faceforward", ty);
                B.CreateCall(fn, {dst, dstStride,
                                  nIn,  B.getInt32(sn),
                                  iIn,  B.getInt32(si),
                                  ngIn, B.getInt32(sng),
                                  numVerts, tags});
            }
            else if (op == "smoothstep") {
                auto [e0,s0] = getVar(ins, 0);
                auto [e1,s1] = getVar(ins, 1);
                auto [x, sx] = getVar(ins, 2);
                if (!dst || !e0 || !e1 || !x) continue;
                auto *fn = declareOp(mod, "rsl_smoothstep", ternOpTy);
                B.CreateCall(fn, {dst, dstStride,
                                  e0, B.getInt32(s0), e1, B.getInt32(s1),
                                  x,  B.getInt32(sx), numVerts, tags});
            }

            // ================================================================
            // Space transforms (Layer G stub — op_pfrom, op_vtransform,
            // op_ntransform already in rslOps.h)
            // Quoted string operand is passed as a global char* constant.
            // ================================================================
            else if (op == "pfrom"      || op == "vtransform" ||
                     op == "ntransform" || op == "transform") {
                // operands: space_string src
                if (ins.operands.size() < 2) continue;
                const std::string &spaceToken = ins.operands[0].token;
                auto [src, ss] = getVar(ins, 1);
                if (!dst || !src) continue;

                // Strip surrounding quotes from the space name token
                std::string spaceName = spaceToken;
                if (spaceName.size() >= 2 && spaceName.front() == '"')
                    spaceName = spaceName.substr(1, spaceName.size() - 2);

                llvm::Value *spacePtr = B.CreateGlobalString(spaceName, "space_str");

                // (float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                // For generic "transform", use proto to pick the right function:
                //   p=Sp → op_pfrom, n=Sn → op_ntransform, v=Sv → op_vtransform
                const char *fnName;
                if (op == "ntransform") {
                    fnName = "op_ntransform";
                } else if (op == "vtransform") {
                    fnName = "op_vtransform";
                } else if (op == "transform") {
                    // Determine by proto: "n=..." → ntransform, "v=..." → vtransform, else ptransform.
                    // RSL transform() goes current→named (uses "to" matrix), unlike pfrom which is named→current.
                    fnName = (!ins.proto.empty() && ins.proto[0] == 'n') ? "op_ntransform"
                           : (!ins.proto.empty() && ins.proto[0] == 'v') ? "op_vtransform"
                           :                                                "op_ptransform";
                } else {
                    fnName = "op_pfrom";
                }
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, spacePtr, src, B.getInt32(ss), numVerts, tags});
            }

            // ================================================================
            // Color/matrix space constructors + ctransform — mirror of the
            // pfrom/vtransform/ntransform block above, but distinct math
            // families: op_cfrom/op_ctransform delegate to convertColorFrom/
            // convertColorTo, op_mfrom to mulmm(from, src) (spec
            // 011-jit-opcode-parity T023/T024 — see rslOps.cpp for the
            // wrapper bodies and shaderOpcodes.h/shaderFunctions.h for the
            // interpreter macros they mirror).
            // ================================================================
            else if (op == "cfrom" || op == "mfrom" || op == "ctransform") {
                // operands: space_string src
                if (ins.operands.size() < 2) continue;
                const std::string &spaceToken = ins.operands[0].token;
                auto [src, ss] = getVar(ins, 1);
                if (!dst || !src) continue;

                std::string spaceName = spaceToken;
                if (spaceName.size() >= 2 && spaceName.front() == '"')
                    spaceName = spaceName.substr(1, spaceName.size() - 2);

                llvm::Value *spacePtr = B.CreateGlobalString(spaceName, "space_str");

                // (float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                const char *fnName = (op == "cfrom")      ? "op_cfrom"
                                    : (op == "mfrom")      ? "op_mfrom"
                                    :                         "op_ctransform";
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, spacePtr, src, B.getInt32(ss), numVerts, tags});
            }

            // ================================================================
            // Lighting (batch wrappers)
            // ================================================================
            else if (op == "ambient") {
                if (!dst) continue;
                // op_ambient_batch(result, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy, {ptrTy, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_ambient_batch", ty);
                B.CreateCall(fn, {dst, numVerts, tags});
            }
            else if (op == "diffuse") {
                auto [nf, sn] = getVar(ins, 0);
                if (!dst || !nf) continue;
                // op_diffuse_batch(result, sr, Nf, sn, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_diffuse_batch", ty);
                B.CreateCall(fn, {dst, dstStride, nf, B.getInt32(sn), numVerts, tags});
            }
            else if (op == "specular") {
                auto [nf, sn] = getVar(ins, 0);
                auto [v,  sv] = getVar(ins, 1);
                auto [r,  sr] = getVar(ins, 2);
                if (!dst || !nf || !v || !r) continue;
                // op_specular_batch(result, Nf, V, roughness, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy, ptrTy, ptrTy, ptrTy, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_specular_batch", ty);
                B.CreateCall(fn, {dst, nf, v, r, numVerts, tags});
            }

            // ================================================================
            // Layer G — extra math / comparison opcodes
            // ================================================================
            else if (op == "max" || op == "maxf") {
                emitBin(ins, "op_maxf", dst, dstStride);
            }
            else if (op == "and" || op == "andf") {
                emitBin(ins, "op_andf", dst, dstStride);
            }
            else if (op == "or" || op == "orf") {
                emitBin(ins, "op_orf", dst, dstStride);
            }
            // Comparison aliases used by some shader compilers
            else if (op == "feql") {
                emitBin(ins, "op_feq", dst, dstStride);
            }
            else if (op == "felt") {
                emitBin(ins, "op_flt", dst, dstStride);
            }
            else if (op == "fneql") {
                emitBin(ins, "op_fne", dst, dstStride);
            }
            else if (op == "radians") {
                emitUn(ins, "op_radians", dst, dstStride);
            }
            else if (op == "filterstep") {
                emitBin(ins, "op_filterstep", dst, dstStride);
            }

            // ================================================================
            // Layer G — reflect
            // ================================================================
            else if (op == "reflect") {
                auto [I, si] = getVar(ins, 0);
                auto [N, sn] = getVar(ins, 1);
                if (!dst || !I || !N) continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_reflect", ty);
                B.CreateCall(fn, {dst, dstStride,
                                  I, B.getInt32(si), N, B.getInt32(sn),
                                  numVerts, tags});
            }

            // ================================================================
            // Layer G — fresnel (outputs Kr, Kt and optionally R, T)
            // IR: fresnel I N eta Kr Kt [R T]
            // ================================================================
            else if (op == "fresnel") {
                auto [I,   si]  = getVar(ins, 0);
                auto [N,   sn]  = getVar(ins, 1);
                auto [eta, se]  = getVar(ins, 2);
                auto [Kr,  skr] = getVar(ins, 3);
                auto [Kt,  skt] = getVar(ins, 4);
                llvm::Value *R = nullptr; int sr = 0;
                llvm::Value *T = nullptr; int st = 0;
                if (ins.operands.size() > 5) { auto [rv,rs] = getVar(ins,5); R=rv; sr=rs; }
                if (ins.operands.size() > 6) { auto [tv,ts] = getVar(ins,6); T=tv; st=ts; }
                if (!I || !N || !eta || !Kr || !Kt) continue;
                // Null-out optional pointers when not present
                llvm::Value *nullPtr = llvm::ConstantPointerNull::get(
                    llvm::PointerType::getUnqual(mod.getContext()));
                if (!R) { R = nullPtr; sr = 0; }
                if (!T) { T = nullPtr; st = 0; }
                // (I,si, N,sn, eta,se, Kr,skr, Kt,skt, R,sr, T,st, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty,
                     ptrTy,i32Ty, ptrTy,i32Ty,
                     ptrTy,i32Ty, ptrTy,i32Ty,
                     i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_fresnel", ty);
                B.CreateCall(fn, {I,  B.getInt32(si),
                                  N,  B.getInt32(sn),
                                  eta,B.getInt32(se),
                                  Kr, B.getInt32(skr),
                                  Kt, B.getInt32(skt),
                                  R,  B.getInt32(sr),
                                  T,  B.getInt32(st),
                                  numVerts, tags});
            }

            // ================================================================
            // Layer G — noise: select variant by operand stride
            // noise(f→f): op_noise_ff, noise(p→f): op_noise_fp
            // noise(f→v): op_noise_vf, noise(p→v): op_noise_vp
            // ================================================================
            else if (op == "noise" || op == "snoise") {
                auto [x, sx] = getVar(ins, 0);
                if (!dst || !x) continue;
                bool dstIsVec = (dstDesc.stride == 3);
                bool srcIsVec = (sx == 3);
                const char *fnName = dstIsVec
                    ? (srcIsVec ? "op_noise_vp" : "op_noise_vf")
                    : (srcIsVec ? "op_noise_fp" : "op_noise_ff");
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, x, B.getInt32(sx), numVerts, tags});
            }

            // ================================================================
            // Layer G — cellnoise: single- and two-source variants
            // (f→f/p): op_cellnoise_ff/fp   (f→ff/pf): op_cellnoise_fff/fpf
            // (v→f/p): op_cellnoise_vf/vp   (v→ff/pf): op_cellnoise_vff/vpf
            // ================================================================
            else if (op == "cellnoise") {
                auto [x, sx] = getVar(ins, 0);
                if (!dst || !x) continue;
                bool dstIsVec = (dstDesc.stride == 3);
                bool srcIsVec = (sx == 3);
                if (ins.operands.size() >= 2) {
                    auto [y, sy] = getVar(ins, 1);
                    if (!y) continue;
                    const char *fnName = dstIsVec
                        ? (srcIsVec ? "op_cellnoise_vpf" : "op_cellnoise_vff")
                        : (srcIsVec ? "op_cellnoise_fpf" : "op_cellnoise_fff");
                    auto *ty = llvm::FunctionType::get(voidTy,
                        {ptrTy,i32Ty, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, fnName, ty);
                    B.CreateCall(fn, {dst, dstStride,
                                      x, B.getInt32(sx), y, B.getInt32(sy),
                                      numVerts, tags});
                } else {
                    const char *fnName = dstIsVec
                        ? (srcIsVec ? "op_cellnoise_vp" : "op_cellnoise_vf")
                        : (srcIsVec ? "op_cellnoise_fp" : "op_cellnoise_ff");
                    auto *ty = llvm::FunctionType::get(voidTy,
                        {ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, fnName, ty);
                    B.CreateCall(fn, {dst, dstStride, x, B.getInt32(sx), numVerts, tags});
                }
            }

            // ================================================================
            // Layer G — string equality
            // seql / sneql: operands are char** (string locals), not float*.
            // ================================================================
            else if (op == "seql" || op == "sneql") {
                if (ins.operands.size() < 2 || !dst) continue;
                const std::string &tokA = ins.operands[0].token;
                const std::string &tokB = ins.operands[1].token;

                // Look up variables; they resolve to char** locals.
                VarDesc descA, descB;
                llvm::Value *ptrA = nullptr, *ptrB = nullptr;
                if (resolveVar(tokA, descA)) ptrA = loadVarPtr(descA);
                if (resolveVar(tokB, descB)) ptrB = loadVarPtr(descB);

                // If not found, embed as a string literal constant.
                auto makeStrLit = [&](const std::string &tok) -> llvm::Value* {
                    std::string s = tok;
                    if (s.size() >= 2 && s.front() == '"')
                        s = s.substr(1, s.size() - 2);
                    // Store char* in a local alloca so we have a char**.
                    llvm::Value *sptr = B.CreateGlobalString(s, "strlit");
                    llvm::Value *alloca = B.CreateAlloca(ptrTy, nullptr, "strlit_pp");
                    B.CreateStore(sptr, alloca);
                    return alloca;
                };
                if (!ptrA) ptrA = makeStrLit(tokA);
                if (!ptrB) ptrB = makeStrLit(tokB);

                const char *fnName = (op == "seql") ? "op_seql" : "op_sneql";
                // (float* dst, int sd, const char* const* a, const char* const* b, int n, const int* tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,ptrTy, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, ptrA, ptrB, numVerts, tags});
            }

            // ================================================================
            // Layer G — derivatives (Du / Dv)
            // ================================================================
            else if (op == "Du" || op == "Dv") {
                auto [src, ss] = getVar(ins, 0);
                if (!dst || !src) continue;
                bool isVec = (ss == 3);
                const char *fnName = (op == "Du")
                    ? (isVec ? "op_Du_vv" : "op_Du_ff")
                    : (isVec ? "op_Dv_vv" : "op_Dv_ff");
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, src, B.getInt32(ss), numVerts, tags});
            }

            // ================================================================
            // Layer G — geometric built-ins: area, calculatenormal, depth
            // ================================================================
            else if (op == "area") {
                auto [P, sp] = getVar(ins, 0);
                if (!dst || !P) continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_area", ty);
                B.CreateCall(fn, {dst, dstStride, P, B.getInt32(sp), numVerts, tags});
            }
            else if (op == "calculatenormal") {
                auto [P, sp] = getVar(ins, 0);
                if (!dst || !P) continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_calculatenormal", ty);
                B.CreateCall(fn, {dst, dstStride, P, B.getInt32(sp), numVerts, tags});
            }
            else if (op == "depth") {
                auto [P, sp] = getVar(ins, 0);
                if (!dst || !P) continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_depth", ty);
                B.CreateCall(fn, {dst, dstStride, P, B.getInt32(sp), numVerts, tags});
            }

            // ================================================================
            // Layer G — texture lookup
            // IR: texture name [channel] s t
            // ================================================================
            else if (op == "texture") {
                // Operands: [name, channel_or_s, t]  or  [name, s, t]
                // If 4 operands: name channel s t (float channel)
                // If 3 operands: name s t (color form)
                if (ins.operands.size() < 3 || !dst) continue;
                const std::string &nameTok = ins.operands[0].token;

                // Resolve name to a char** locals slot (or embed a string literal).
                llvm::Value *namePP = nullptr;
                VarDesc nameDesc;
                if (resolveVar(nameTok, nameDesc)) {
                    namePP = loadVarPtr(nameDesc);
                } else {
                    // String literal operand
                    std::string s = nameTok;
                    if (s.size() >= 2 && s.front() == '"') s = s.substr(1, s.size()-2);
                    llvm::Value *sptr = B.CreateGlobalString(s, "texname");
                    llvm::Value *alloca = B.CreateAlloca(ptrTy, nullptr, "texname_pp");
                    B.CreateStore(sptr, alloca);
                    namePP = alloca;
                }

                if (ins.operands.size() >= 4) {
                    // Float-channel form: texture name channel s t
                    auto [chan, sc] = getVar(ins, 1);
                    auto [s,   ss] = getVar(ins, 2);
                    auto [t,   st] = getVar(ins, 3);
                    if (!chan || !s || !t) continue;
                    // (dst,sd, namepp, chan, s,ss, t,st, n, tags)
                    auto *ty = llvm::FunctionType::get(voidTy,
                        {ptrTy,i32Ty, ptrTy, ptrTy, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_texture_f", ty);
                    B.CreateCall(fn, {dst, dstStride, namePP, chan,
                                      s, B.getInt32(ss), t, B.getInt32(st), numVerts, tags});
                } else {
                    // Color form: texture name s t
                    auto [s, ss] = getVar(ins, 1);
                    auto [t, st] = getVar(ins, 2);
                    if (!s || !t) continue;
                    // (dst,sd, namepp, s,ss, t,st, n, tags)
                    auto *ty = llvm::FunctionType::get(voidTy,
                        {ptrTy,i32Ty, ptrTy, ptrTy,i32Ty, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_texture_c", ty);
                    B.CreateCall(fn, {dst, dstStride, namePP,
                                      s, B.getInt32(ss), t, B.getInt32(st), numVerts, tags});
                }
            }

            // ================================================================
            // Layer G — environment / shadow lookups
            // ================================================================
            else if (op == "environment") {
                if (ins.operands.size() < 2 || !dst) continue;
                const std::string &nameTok = ins.operands[0].token;
                llvm::Value *namePP = nullptr;
                VarDesc nameDesc;
                if (resolveVar(nameTok, nameDesc)) {
                    namePP = loadVarPtr(nameDesc);
                } else {
                    std::string s = nameTok;
                    if (s.size() >= 2 && s.front() == '"') s = s.substr(1, s.size()-2);
                    llvm::Value *sptr = B.CreateGlobalString(s, "envname");
                    llvm::Value *alloca = B.CreateAlloca(ptrTy, nullptr, "envname_pp");
                    B.CreateStore(sptr, alloca);
                    namePP = alloca;
                }

                if (ins.operands.size() >= 3) {
                    // Float-channel form: environment name channel D
                    auto [chan, sc] = getVar(ins, 1);
                    auto [D,   sD] = getVar(ins, 2);
                    if (!chan || !D) continue;
                    auto *ty = llvm::FunctionType::get(voidTy,
                        {ptrTy,i32Ty, ptrTy, ptrTy, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_environment_f", ty);
                    B.CreateCall(fn, {dst, dstStride, namePP, chan,
                                      D, B.getInt32(sD), numVerts, tags});
                } else {
                    // Color form: environment name D
                    auto [D, sD] = getVar(ins, 1);
                    if (!D) continue;
                    auto *ty = llvm::FunctionType::get(voidTy,
                        {ptrTy,i32Ty, ptrTy, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_environment_c", ty);
                    B.CreateCall(fn, {dst, dstStride, namePP,
                                      D, B.getInt32(sD), numVerts, tags});
                }
            }
            else if (op == "shadow") {
                if (ins.operands.size() < 2 || !dst) continue;
                const std::string &nameTok = ins.operands[0].token;
                llvm::Value *namePP = nullptr;
                VarDesc nameDesc;
                if (resolveVar(nameTok, nameDesc)) {
                    namePP = loadVarPtr(nameDesc);
                } else {
                    std::string s = nameTok;
                    if (s.size() >= 2 && s.front() == '"') s = s.substr(1, s.size()-2);
                    llvm::Value *sptr = B.CreateGlobalString(s, "shadowname");
                    llvm::Value *alloca = B.CreateAlloca(ptrTy, nullptr, "shadowname_pp");
                    B.CreateStore(sptr, alloca);
                    namePP = alloca;
                }
                auto [Ps, sPs] = getVar(ins, 1);
                if (!Ps) continue;
                // (dst,sd, namepp, Ps,sPs, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy,i32Ty, ptrTy, ptrTy,i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_shadow_f", ty);
                B.CreateCall(fn, {dst, dstStride, namePP,
                                  Ps, B.getInt32(sPs), numVerts, tags});
            }

            // ================================================================
            // Layer G — vfrom aliases used by some shader variants
            // vtransform alias 'vfrom' (point transform); 'ntransform' alias 'nfrom'
            // ================================================================
            else if (op == "vfrom" || op == "nfrom") {
                if (ins.operands.size() < 2) continue;
                const std::string &spaceToken = ins.operands[0].token;
                auto [src, ss] = getVar(ins, 1);
                if (!dst || !src) continue;
                std::string spaceName = spaceToken;
                if (spaceName.size() >= 2 && spaceName.front() == '"')
                    spaceName = spaceName.substr(1, spaceName.size() - 2);
                llvm::Value *spacePtr = B.CreateGlobalString(spaceName, "space_str");
                auto *ty = llvm::FunctionType::get(voidTy,
                    {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                const char *fnName = (op == "nfrom") ? "op_ntransform" : "op_vtransform";
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, spacePtr, src, B.getInt32(ss), numVerts, tags});
            }

            // ================================================================
            // lightsource — query current light's attribute (inside illuminance)
            // lightsource("f=SF") result attrName outParam
            // ================================================================
            else if (op == "lightsource") {
                if (ins.operands.size() < 2 || !dst) { /* no-op */ }
                else {
                    const std::string &nameTok = ins.operands[0].token;
                    std::string nameStr = nameTok;
                    if (nameStr.size() >= 2 && nameStr.front() == '"')
                        nameStr = nameStr.substr(1, nameStr.size() - 2);
                    llvm::Value *namePtr = B.CreateGlobalString(nameStr, "lsattr");
                    auto [out, so] = getVar(ins, 1);
                    if (out) {
                        // (result, sr, attrName, outParam, so, n, tags) → void
                        auto *ty = llvm::FunctionType::get(voidTy,
                            {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                        auto *fn = declareOp(mod, "op_lightsource_f", ty);
                        B.CreateCall(fn, {dst, dstStride, namePtr,
                                          out, B.getInt32(so), numVerts, tags});
                    }
                }
            }

            // ================================================================
            // spline — Catmull-Rom spline interpolation
            // spline("c=fccc...") dst t knot0 knot1 ... knotN-1
            // ================================================================
            else if (op == "spline") {
                if (!dst || ins.operands.size() < 3) { /* no-op */ }
                else {
                    auto [t, st] = getVar(ins, 0);
                    if (!t) { /* no-op */ }
                    else {
                        int numKnots = (int)ins.operands.size() - 1;
                        // Allocate stack array of float* pointers
                        auto *arrTy = llvm::ArrayType::get(ptrTy, numKnots);
                        auto *arr   = B.CreateAlloca(arrTy, nullptr, "spline_knots");
                        for (int k = 0; k < numKnots; ++k) {
                            auto [kp, ks] = getVar(ins, k + 1);
                            if (!kp) { arr = nullptr; break; }
                            (void)ks;
                            auto *gep = B.CreateGEP(arrTy, arr,
                                {B.getInt32(0), B.getInt32(k)});
                            B.CreateStore(kp, gep);
                        }
                        if (arr) {
                            // arr is already ptr; no bitcast needed with opaque pointers
                            // Determine color vs float from proto: starts with 'f' → spline_f
                            bool isFloat = !ins.proto.empty() && ins.proto[0] == 'f';
                            const char *fnName = isFloat ? "op_spline_f" : "op_spline_c";
                            // (dst, sd, t, st, numKnots, knots**, n, tags)
                            auto *ty = llvm::FunctionType::get(voidTy,
                                {ptrTy, i32Ty, ptrTy, i32Ty,
                                 i32Ty, ptrTy, i32Ty, ptrTy}, false);
                            auto *fn = declareOp(mod, fnName, ty);
                            B.CreateCall(fn, {dst, dstStride, t, B.getInt32(st),
                                              B.getInt32(numKnots), arr,
                                              numVerts, tags});
                        }
                    }
                }
            }

            // ================================================================
            // No-op opcodes (string/print built-ins with no per-vertex effect)
            // ================================================================
            else if (op == "printf" || op == "ftoa" ||
                     op == "return" || op == "jmp") {
                // Silently skip — no per-vertex output.
            }

            // Unrecognised opcode — skip silently.
        }
    }

    // Ensure a terminator exists.
    if (!B.GetInsertBlock()->getTerminator())
        B.CreateRetVoid();
}

// =========================================================================
// Check whether an IRFunction is non-trivial (has instructions besides return).
// =========================================================================
static bool isFnNonTrivial(const IRFunction &fn) {
    for (const IRBlock &blk : fn.blocks)
        for (const IRInstr &ins : blk.instrs)
            if (ins.opcode != "return") return true;
    return false;
}

// =========================================================================
// Public entry point: emitLLVMBitcode
// =========================================================================
bool emitLLVMBitcode(const IRModule &mod,
                     const std::string &outPath,
                     const std::string &shaderName) {
    llvm::LLVMContext ctx;
    auto llvmMod = std::make_unique<llvm::Module>(shaderName, ctx);

    auto *voidTy = llvm::Type::getVoidTy(ctx);
    auto *i32Ty  = llvm::Type::getInt32Ty(ctx);
    auto *ptrTy  = llvm::PointerType::getUnqual(ctx);

    auto varTbl = buildVarTable(mod);

    // Helper: build and emit one function with the standard shader signature.
    auto buildAndEmit = [&](const std::string &fnName, const IRFunction &irFn) {
        auto *funcTy = llvm::FunctionType::get(voidTy, {i32Ty, ptrTy, ptrTy}, false);
        auto *func   = llvm::Function::Create(
            funcTy, llvm::Function::ExternalLinkage, fnName, llvmMod.get());

        auto args      = func->arg_begin();
        llvm::Value *numVerts  = &*args++;
        llvm::Value *stuffPtr  = &*args++;
        llvm::Value *tags      = &*args++;

        // Entry block: extract slot1 and slot2 from stuffPtr.
        // emitFunction() will append its instructions to this same block.
        auto *entry = llvm::BasicBlock::Create(ctx, "entry", func);
        llvm::IRBuilder<> B(entry);
        auto *slot1_pp = B.CreateGEP(ptrTy, stuffPtr, B.getInt32(1), "globals_pp");
        llvm::Value *slot1 = B.CreateLoad(ptrTy, slot1_pp, "globals");
        auto *slot2_pp = B.CreateGEP(ptrTy, stuffPtr, B.getInt32(2), "locals_pp");
        llvm::Value *slot2 = B.CreateLoad(ptrTy, slot2_pp, "locals");

        // Continue appending to entry (after the slot loads).
        emitFunction(irFn, func, entry, numVerts, slot1, slot2, tags, varTbl, ctx, *llvmMod);
    };

    // -----------------------------------------------------------------------
    // Layer B: Compile #!Init section (if non-trivial) as shadername_init.
    // -----------------------------------------------------------------------
    bool hasInit = isFnNonTrivial(mod.initFn);
    if (hasInit) {
        buildAndEmit(shaderName + "_init", mod.initFn);
    }

    // -----------------------------------------------------------------------
    // Compile #!Code section as the main shader entry function.
    // -----------------------------------------------------------------------
    buildAndEmit(shaderName, mod.codeFn);

    // -----------------------------------------------------------------------
    // Embed metadata (including hasinit flag).
    // -----------------------------------------------------------------------
    embedMetadata(*llvmMod, shaderName, mod, hasInit, ctx);

    // -----------------------------------------------------------------------
    // Verify the module before writing — catches invalid IR before crash.
    // -----------------------------------------------------------------------
    std::string verifyErr;
    llvm::raw_string_ostream verifyStream(verifyErr);
    if (llvm::verifyModule(*llvmMod, &verifyStream)) {
        verifyStream.flush();
        fprintf(stderr, "llvmEmitter: IR verification failed for '%s':\n%s\n",
                shaderName.c_str(), verifyErr.c_str());
        if (const char *dump = getenv("OPENRENDER_DUMP_IR")) {
            (void)dump;
            llvmMod->print(llvm::errs(), nullptr);
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Write bitcode to file.
    // -----------------------------------------------------------------------
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
