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
#include "rslo.h" // SLC_* bit constants (compiler-side)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#pragma GCC diagnostic pop

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// =========================================================================
// kHandledOpcodes — single source of truth for every mnemonic emitFunction()
// dispatches on. Seeded from the current if/else-if chain (spec
// 011-jit-opcode-parity T003); the libshader coverage-guard ctest links
// this symbol directly (research.md D3), so it must stay in sync with the
// chain below — any new `else if (op == "...")` case added to emitFunction()
// needs a matching entry here or it silently regresses to a no-op via the
// gate a few lines below.
// =========================================================================
extern const char *const kHandledOpcodes[] = {
    "abs", "acos", "addff", "addmm", "addvf", "addvf2", "addvv", "ambient",
    "and",
    "andf", "area", "asin", "atan", "atan2", "atmosphere", "attribute",
    "bake3d", "break", "calculatenormal",
    "ceil", "cellnoise", "cfrom", "clamp", "clampf", "clampv", "clearlighting", "comp", "concat", "continue",
    "cos", "cross", "ctransform",
    "debug", "degrees", "depth", "Deriv", "determinant", "diffuse", "displacement", "distance", "divff", "divmm", "divvf", "divvv", "dot", "Du", "Dv",
    "else", "endfor", "endif", "endilluminance", "endilluminate",
    "endsolar", "endwhile", "environment", "exp", "faceforward", "felt",
    "feq", "feql", "fegt", "fge", "fgt", "ffroma", "filterstep", "fle",
    "floor", "flt",
    "fne", "fneql", "for", "forbegin", "forend", "format", "fresnel", "ftoa",
    "gather", "gatherElse", "gatherEnd", "gatherHeader", "if",
    "illuminance", "illuminate", "incident", "indirectdiffuse", "inversesqrt", "jmp", "length",
    "lightsource", "log", "match", "max", "maxf", "mfrom", "mfromf", "mfromv", "min", "minf",
    "mfroma", "mix", "mixf", "mixv",
    "mod", "moveff", "movemm", "movess", "movevv", "mtoa", "mulff", "mulmm",
    "mulvf", "mulvf2", "mulvv", "negf", "negm",
    "negv", "nfrom", "noise", "normalize", "not", "ntransform", "occlusion", "opposite", "option", "or", "orf",
    "pfrom", "phong", "photonmap", "pnoise", "pow", "printf", "ptlined", "radians", "random", "raydepth", "rayinfo", "raylabel", "reflect", "refract", "rendererinfo", "return", "rotate", "round", "seql",
    "scale", "setcomp", "setxcomp", "setycomp", "setzcomp", "sfroma", "shadername", "shadow", "sign", "sin",
    "smoothstep", "sneql", "snoise", "solar", "specular", "specularbrdf", "spline", "step",
    "sqrt", "stoa", "subff", "submm", "subvf", "subvv", "surface", "tan", "texture",
    "texture3d", "textureinfo",
    "trace", "translate", "transform", "transmission", "uffroma", "umfroma", "urandom", "usfroma", "uvfroma",
    "veql", "vegt", "velt", "vfrom", "vfroma", "vfromf", "vfromfff",
    "vfromvff", "vgt", "visibility", "vlt", "vneql", "vtoa", "vtransform", "vufloat",
    "vumatrix", "vustring", "vuvector", "while", "whilebegin", "xcomp",
    "ycomp", "zcomp",
    nullptr};

static bool isHandledOpcode(const std::string &op) {
    for (int i = 0; kHandledOpcodes[i] != nullptr; ++i) {
        if (op == kHandledOpcodes[i])
            return true;
    }
    return false;
}

// =========================================================================
// currentBlockHasTerminator — does the builder's insertion block already end?
//
// Used in the six places this file decides whether to append a br/ret. Two
// separate hazards make it a function rather than an inline expression.
//
// 1. BasicBlock::getTerminator() CHANGED CONTRACT. Through LLVM 22 it returned
//    nullptr for a block with no terminator, and this file relied on that:
//        if (!B.GetInsertBlock()->getTerminator()) B.CreateRetVoid();
//    LLVM 23 turned it into a precondition -- assert(hasTerminator()) and then
//    `return &InstList.back();` unconditionally. With NDEBUG the assert is gone,
//    so it hands back the last *non-terminator* instruction, the old test reads
//    as "already terminated", and no terminator is appended. That is exactly why
//    `oshader --jit` started emitting functions with no `ret void` and failing
//    module verification ("Basic Block in function 'X' does not have
//    terminator!") on LLVM 23, while remaining correct on LLVM 18.
//
//    LLVM 23 provides hasTerminator() and getTerminatorOrNull() for this, but
//    NEITHER exists in LLVM 15 -- OPENRENDER_LLVM_MIN_VERSION -- so using them
//    would need a version conditional. empty() and back() are stable public API
//    across the whole supported range and are precisely what LLVM 23's own
//    hasTerminator() is implemented in terms of, so spell the test out here and
//    it is correct on every LLVM we support.
//
// 2. IRBuilder::GetInsertBlock() may legitimately return null (LLVM allows it
//    after ClearInsertionPoint()). Every path here sets an insertion point
//    first, but GCC cannot see that and reported a potential null dereference
//    against llvm/IR/Value.h -- a middle-end warning raised after inlining, so
//    neither -isystem on the LLVM headers nor a #pragma around the #includes
//    reaches it. The explicit test removes the unguarded dereference, and the
//    assert turns a would-be crash into a clean failure if that ever changes.
// =========================================================================
static bool currentBlockHasTerminator(llvm::IRBuilder<> &B) {
    const llvm::BasicBlock *bb = B.GetInsertBlock();
    assert(bb != nullptr && "IRBuilder has no insertion point");
    if (bb == nullptr)
        return false;
    return !bb->empty() && bb->back().isTerminator();
}

// =========================================================================
// kOpcodeParamTable — re-expansion of the interpreter's own opcode/function
// tables into (text, params) rows (spec 014-jit-shading-parity, research.md
// D3). Mirrors the enum-only precedent at rslo_code.h:31-49 (same
// DEFOPCODE/DEFFUNC-family macros, same scriptFunctions.h/scriptOpcodes.h
// #includes), except the macro bodies capture `params` instead of
// discarding it — that's what lets computeUsedParameters() OR in
// PARAMETER_RAYTRACE/MESSAGEPASSING/NONAMBIENT/derivative bits by
// opcode/function name, not just by declared-global-variable name.
//
// `params` arguments are literal expressions referencing PARAMETER_* (e.g.
// "PARAMETER_RAYTRACE | PARAMETER_DERIVATIVE | PARAMETER_DU | PARAMETER_DV"
// on giFunctions.h's TraceF/TraceV rows), so rendererc.h must be visible
// here — unlike rslo_code.h, which never references `params` and so never
// needed it.
// =========================================================================
#include "rendererc.h"

#define DEFOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, params) {text, static_cast<unsigned int>(params)},
#define DEFSHORTOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, params) {text, static_cast<unsigned int>(params)},
#define DEFLINKOPCODE(name, text, nargs) {text, 0u},
#define DEFLINKFUNC(name, text, prototype, par) {text, static_cast<unsigned int>(par)},
#define DEFFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},
#define DEFLIGHTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},
#define DEFSHORTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},

extern const OpcodeParamEntry kOpcodeParamTable[] = {
#include "scriptFunctions.h"
#include "scriptOpcodes.h"
    {nullptr, 0u}};

#undef DEFOPCODE
#undef DEFSHORTOPCODE
#undef DEFLINKOPCODE
#undef DEFLINKFUNC
#undef DEFFUNC
#undef DEFLIGHTFUNC
#undef DEFSHORTFUNC

// =========================================================================
// kAllFunctionMnemonics — every RSL builtin FUNCTION_ mnemonic, re-expanded
// from ONLY scriptFunctions.h's own #include chain (-> shaderFunctions.h ->
// giFunctions.h), capturing just `text` (spec 017-jit-builtin-function-
// coverage, research.md D3) — the FUNCTION_-family mirror of opcodes.cpp's
// kAllOpcodeMnemonics. Confirmed via grep: zero DEFOPCODE-family macros
// anywhere in this #include chain, so this array is exclusively builtin
// FUNCTIONs, never OPCODE_ bytecode mnemonics (those live in
// kAllOpcodeMnemonics/opcodes.cpp instead). Two rows carry `text == "XXX"`
// (scriptFunctions.h:1135,1166) -- the DSO/plugin-shadeop dispatcher rows,
// whose real name/prototype is resolved from the loaded plugin at runtime,
// never a static mnemonic (research.md D7); the coverage-guard test
// hand-excludes "XXX", the same way it hand-excludes kDeadOpcodes entries
// for the OPCODE_ family.
// =========================================================================
#define DEFLINKFUNC(name, text, prototype, par) text,
#define DEFFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) text,
#define DEFLIGHTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) text,
#define DEFSHORTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) text,

extern const char *const kAllFunctionMnemonics[] = {
#include "scriptFunctions.h"
    nullptr};

#undef DEFLINKFUNC
#undef DEFFUNC
#undef DEFLIGHTFUNC
#undef DEFSHORTFUNC

// =========================================================================
// computeUsedParameters — the openrender.shader.usedparameters bitmask
// (spec 014-jit-shading-parity, FR-001–FR-005).
//
// Global-variable half (this function's current scope, T011/US1): scans
// every IRInstr's `result` *and* `operands` for a name match against
// kParamBits, instead of trusting `v.slcType & SLC_GLOBAL` on the
// pre-seeded ir.vars list (rslo.cpp's CScriptContext constructor
// unconditionally seeds all 26 RSL built-in globals into that list before
// any shader source is parsed, so the old gate was true for virtually
// every shader regardless of content). `result` is included so a
// write-only reference (e.g. `Ci = ...` with Ci never read) still sets its
// bit, matching the interpreter's rslo.y:487-490 semantics, which fire on
// any reference regardless of read/write position.
//
// Opcode/function half (kOpcodeParamTable, T016/US2): for every IRInstr,
// OR in the bits from EVERY kOpcodeParamTable row whose `text` matches
// ins.opcode -- not first-match-wins. Multiple DEFFUNC rows can share the
// same opcode text with different `params` (e.g. texture's 4 overloads in
// shaderFunctions.h: two carry the derivative-family bits, two don't,
// distinguished only by argument-count/signature, which the emitter's IR
// doesn't retain at this point). ORing every matching row is conservative
// (over-inclusive, never under-inclusive) and needs no signature
// disambiguation -- this table also naturally supersedes the old
// hand-written hasNonAmbientOp scan: illuminance/endilluminance carry
// params=0 in the interpreter's own shaderOpcodes.h, so they're correctly
// excluded from PARAMETER_NONAMBIENT by construction, while
// illuminate/solar/endilluminate/endsolar correctly set it.
// =========================================================================
unsigned int computeUsedParameters(const IRModule &ir) {
    static const struct {
            const char *name;
            unsigned int bit;
    } kParamBits[] = {
        {"s", 1u},
        {"t", 1u << 1},
        {"u", 1u << 2},
        {"v", 1u << 3},
        {"du", (1u << 4) | (1u << 14)},
        {"dv", (1u << 5) | (1u << 14)},
        {"time", 1u << 6},
        {"dtime", 1u << 7},
        {"ncomps", 1u << 8},
        {"alpha", 1u << 9},
        {"P", 1u << 10},
        {"Ps", 1u << 11},
        {"Pw", 1u << 10},
        {"dPdu", 1u << 12},
        {"dPdv", 1u << 13},
        {"dPdtime", 1u << 15},
        {"Ng", 1u << 16},
        {"N", (1u << 17) | (1u << 16)},
        {"Ci", 1u << 18},
        {"Oi", 1u << 19},
        {"Cl", 1u << 20},
        {"Ol", 1u << 21},
        {"Cs", 1u << 22},
        {"Os", 1u << 23},
        {"E", 1u << 24},
        {"I", 1u << 25},
        {"L", 1u << 26},
    };

    unsigned int usedParams = 0;

    auto scanToken = [&](const std::string &tok) {
        for (const auto &e : kParamBits) {
            if (tok == e.name) {
                usedParams |= e.bit;
                break;
            }
        }
    };

    // OR in bits from every kOpcodeParamTable row whose text matches
    // ins.opcode -- not first-match-wins, since multiple DEFFUNC rows can
    // share opcode text with different params (e.g. texture()'s overloads).
    auto scanOpcode = [&](const std::string &opcode) {
        for (const OpcodeParamEntry *e = kOpcodeParamTable; e->text != nullptr; ++e) {
            if (opcode == e->text)
                usedParams |= e->params;
        }
    };

    for (const IRFunction *fn : {&ir.initFn, &ir.codeFn}) {
        for (const IRBlock &blk : fn->blocks) {
            for (const IRInstr &ins : blk.instrs) {
                if (ins.hasResult())
                    scanToken(ins.result);
                for (const IROperand &op : ins.operands) {
                    if (op.isLiteral() || op.isQuoted() || op.isLabel())
                        continue;
                    scanToken(op.token);
                }
                scanOpcode(ins.opcode);
            }
        }
    }

    return usedParams;
}

// =========================================================================
// RSL global variable name → VARIABLE_* index (slot 1 / SL_GLOBAL_OPERAND)
//
// Indices reference the interpreter's own VARIABLE_* constants
// (src/ri/render/rendererc.h, already #include'd above at line 115 for
// kOpcodeParamTable's `params` macro expansions) instead of hand-transcribed
// literals (spec 014-jit-shading-parity, research.md D7a) -- a future
// addition/removal in rendererc.h now either compiles or fails to compile
// here, instead of silently drifting the way a parallel literal table could.
// =========================================================================
static const std::unordered_map<std::string, int> s_rslGlobals = {
    {"P", VARIABLE_P},
    {"Ps", VARIABLE_PS},
    {"N", VARIABLE_N},
    {"Ng", VARIABLE_NG},
    {"dPdu", VARIABLE_DPDU},
    {"dPdv", VARIABLE_DPDV},
    {"L", VARIABLE_L},
    {"Cs", VARIABLE_CS},
    {"Os", VARIABLE_OS},
    {"Cl", VARIABLE_CL},
    {"Ol", VARIABLE_OL},
    {"Ci", VARIABLE_CI},
    {"Oi", VARIABLE_OI},
    {"s", VARIABLE_S},
    {"t", VARIABLE_T},
    {"du", VARIABLE_DU},
    {"dv", VARIABLE_DV},
    {"u", VARIABLE_U},
    {"v", VARIABLE_V},
    {"I", VARIABLE_I},
    {"E", VARIABLE_E},
    {"alpha", VARIABLE_ALPHA},
    {"time", VARIABLE_TIME},
    {"Pw", VARIABLE_PW},
    {"ncomps", VARIABLE_NCOMPS},
    {"dtime", VARIABLE_DTIME},
    {"dPdtime", VARIABLE_DPDTIME},
    {"width", VARIABLE_WIDTH},
    {"constantwidth", VARIABLE_CONSTANTWIDTH},
};

// =========================================================================
// VarDesc — describes where to find a variable in the stuff[][] arrays.
// =========================================================================
struct VarDesc {
        int slot;   // 0=constants, 1=globals/varying, 2=locals
        int idx;    // index within the slot array
        int stride; // 0=uniform, 1=float, 3=vector, 16=matrix
        // A string-typed variable's stride collapses to the same value as a
        // plain float's (both "1 item"), so `stride` alone can't tell them
        // apart -- needed by rayinfo() (spec 017-jit-builtin-function-
        // coverage, US3), whose "f=s." wildcard destination prototype gives
        // no static result-type hint the way surface()/etc.'s per-type
        // DEFFUNC overloads do. Defaults false (matrix/vector/float/globals
        // all leave it unset); safe to ignore everywhere else.
        bool isString = false;
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
        if (v.slcType & SLC_GLOBAL)
            continue;
        if (!(v.slcType & SLC_PARAMETER))
            continue;

        int elemSize = (v.slcType & SLC_MATRIX)   ? 16
                       : (v.slcType & SLC_VECTOR) ? 3
                                                  : 1;
        int stride = (v.slcType & SLC_UNIFORM) ? 0 : elemSize * v.numItems;

        tbl[v.cName] = {2, slot2Idx, stride, v.isString()};
        tbl[v.symbolName] = {2, slot2Idx, stride, v.isString()};
        ++slot2Idx;
    }

    // Local temporaries (SLC_PARAMETER clear, SLC_GLOBAL clear)
    for (const IRVarInfo &v : mod.vars) {
        if (v.slcType & SLC_GLOBAL)
            continue;
        if (v.slcType & SLC_PARAMETER)
            continue;

        int elemSize = (v.slcType & SLC_MATRIX)   ? 16
                       : (v.slcType & SLC_VECTOR) ? 3
                                                  : 1;
        int stride = (v.slcType & SLC_UNIFORM) ? 0 : elemSize * v.numItems;

        tbl[v.cName] = {2, slot2Idx, stride, v.isString()};
        tbl[v.symbolName] = {2, slot2Idx, stride, v.isString()};
        ++slot2Idx;
    }

    // RSL globals in slot 1. Index comes from s_rslGlobals (VARIABLE_*
    // constants); stride is derived from mod.vars' own SLC_GLOBAL entries via
    // the same elemSize/stride formula used for parameters/locals above,
    // since CIRBuilder::addVar() already copies each global CVariable's
    // SLC_* type flags into IRVarInfo verbatim (research.md D7a) -- no
    // separate hand-maintained stride table needed.
    std::unordered_map<std::string, int> globalStrides;
    for (const IRVarInfo &v : mod.vars) {
        if (!(v.slcType & SLC_GLOBAL))
            continue;

        int elemSize = (v.slcType & SLC_MATRIX)   ? 16
                       : (v.slcType & SLC_VECTOR) ? 3
                                                  : 1;
        int stride = (v.slcType & SLC_UNIFORM) ? 0 : elemSize * v.numItems;
        globalStrides[v.symbolName] = stride;
    }

    for (const auto &[name, idx] : s_rslGlobals) {
        auto strideIt = globalStrides.find(name);
        int stride = (strideIt != globalStrides.end()) ? strideIt->second : 3;
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
    auto mkMD = [&](llvm::Metadata *m) { return llvm::MDNode::get(ctx, m); };

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

    // usedParameters bitmask (spec 014-jit-shading-parity) — single source
    // of truth in computeUsedParameters(), shared with the gating-condition
    // ctest (llvmEmitter.h).
    {
        unsigned int usedParams = computeUsedParameters(ir);
        mod.getOrInsertNamedMetadata("openrender.shader.usedparameters")
            ->addOperand(mkMD(mkStr(std::to_string(usedParams))));
    }

    auto embedVars = [&](const char *key, int slcFilter) {
        llvm::NamedMDNode *nmd = mod.getOrInsertNamedMetadata(key);
        for (const IRVarInfo &v : ir.vars) {
            if ((v.slcType & SLC_PARAMETER) == 0 && slcFilter == SLC_PARAMETER)
                continue;
            if ((v.slcType & SLC_PARAMETER) != 0 && slcFilter == 0)
                continue;
            if (v.slcType & SLC_GLOBAL)
                continue;

            const std::string typeStr = (v.slcType & SLC_FLOAT) ? "float" : (v.slcType & SLC_MATRIX) ? "matrix"
                                                                        : (v.slcType & SLC_STRING)   ? "string"
                                                                        : (v.slcType & SLC_VPOINT)   ? "point"
                                                                        : (v.slcType & SLC_VNORMAL)  ? "normal"
                                                                        : (v.slcType & SLC_VCOLOR)   ? "color"
                                                                        : (v.slcType & SLC_VECTOR)   ? "vector"
                                                                                                     : "float";
            const std::string storage = (v.slcType & SLC_UNIFORM) ? "uniform" : "varying";
            const std::string writable = (v.slcType & SLC_OUTPUT) ? "true" : "false";

            llvm::Metadata *fields[] = {
                mkStr(v.symbolName), mkStr(typeStr), mkStr(storage),
                mkStr(writable), mkStr(std::to_string(v.numItems)), mkStr(v.defaultValue)};
            nmd->addOperand(llvm::MDNode::get(ctx, fields));
        }
    };

    embedVars("openrender.shader.params", SLC_PARAMETER);
    embedVars("openrender.shader.vars", 0);
}

// =========================================================================
// External function declaration helper
// =========================================================================
static llvm::Function *declareOp(llvm::Module &mod, const std::string &name, llvm::FunctionType *ty) {
    if (auto *existing = mod.getFunction(name))
        return existing;
    return llvm::Function::Create(ty, llvm::Function::ExternalLinkage, name, &mod);
}

// =========================================================================
// Attempt to parse `tok` as a numeric literal.
// On success fills `*out` and returns true; leaves *out unchanged on failure.
// =========================================================================
static bool parseLiteralFloat(const std::string &tok, float *out) {
    if (tok.empty())
        return false;
    char *end = nullptr;
    errno = 0;
    float v = std::strtof(tok.c_str(), &end);
    if (errno != 0 || end == tok.c_str() || *end != '\0')
        return false;
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
static bool emitFunction(const IRFunction &irFn,
                         llvm::Function *func, // for new BBs in loop layers
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
            std::string exitLabel; // "#!LabelN" of the endilluminance block
            llvm::BasicBlock *bodyBB;
            llvm::BasicBlock *latchBB;
            llvm::BasicBlock *exitBB;
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
            std::string latchLabel; // "#!LabelN" of the increment block
            std::string exitLabel;  // "#!LabelN" of the forend block
            llvm::BasicBlock *condBB;
            llvm::BasicBlock *bodyBB;
            llvm::BasicBlock *latchBB;
            llvm::BasicBlock *exitBB;
            llvm::Value *execCountPtr; // alloca i32 for forExecCount
    };
    std::vector<ForScope> forStack;

    // -----------------------------------------------------------------------
    // Layer G: gather() loop scope tracking.
    // Unlike illuminance/for, gatherEnd's backward jump targets the label
    // right after `gather` itself (GATHERENDEXPR_PRE's jmp(argument(0))),
    // so the header/back-edge is wired entirely inline at the gather/
    // gatherEnd opcode sites — no outer block-boundary matching needed.
    // gather/gatherElse's own jmp(argument(0)) targets are forward
    // numActive==0 skips, handled for free by tag masking (same as if/else).
    // -----------------------------------------------------------------------
    struct GatherScope {
            llvm::BasicBlock *headerBB;
            llvm::BasicBlock *exitBB;
    };
    std::vector<GatherScope> gatherStack;

    auto *i32Ty = llvm::Type::getInt32Ty(ctx);
    auto *f32Ty = llvm::Type::getFloatTy(ctx);
    auto *ptrTy = llvm::PointerType::getUnqual(ctx);
    auto *voidTy = llvm::Type::getVoidTy(ctx);

    // -----------------------------------------------------------------------
    // Layer C: numActive / numPassive stack slots for conditional execution.
    // op_if_update / op_else_update / op_endif_update write into these.
    // -----------------------------------------------------------------------
    auto *numActivePtr = B.CreateAlloca(i32Ty, nullptr, "numActive");
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
        if (it == varTbl.end())
            return false;
        out = it->second;
        return true;
    };

    // -----------------------------------------------------------------------
    // Layer A: allocLiteral — materialize a bare literal token that isn't a
    // declared variable. Handles numeric literals (stack float) and quoted
    // string literals (global string + a ptrTy alloca, matching how string
    // locals are represented as char** so callers can treat both uniformly).
    // Returns {alloca_ptr, 0} on success or {nullptr, 0} if not a literal.
    // -----------------------------------------------------------------------
    auto allocLiteral = [&](const std::string &tok) -> std::pair<llvm::Value *, int> {
        float v = 0.f;
        if (parseLiteralFloat(tok, &v)) {
            auto *alloca = B.CreateAlloca(f32Ty, nullptr, "lit");
            B.CreateStore(llvm::ConstantFP::get(f32Ty, v), alloca);
            return {alloca, 0};
        }
        if (tok.size() >= 2 && tok.front() == '"' && tok.back() == '"') {
            std::string s = tok.substr(1, tok.size() - 2);
            // The runtime .rslo loader unescapes \n/\t/\r/\\ via
            // osProcessEscapes() when it re-parses a compiled string
            // literal (libshader/runtime/rslo.l, libshader/shading/rslo.l)
            // -- the JIT path materializes this token directly from the
            // IR's raw text and never round-trips through that loader, so
            // without this call a literal like "...\n" would carry a
            // literal backslash-n into the shader instead of a real
            // newline. Found via printf() (GitHub #11) parity testing
            // against the interpreter; applies to every JIT string literal,
            // not just printf's.
            osProcessEscapes(s.data());
            s.resize(std::strlen(s.data()));
            llvm::Value *sptr = B.CreateGlobalString(s, "strlit");
            auto *alloca = B.CreateAlloca(ptrTy, nullptr, "strlit_pp");
            B.CreateStore(sptr, alloca);
            return {alloca, 0};
        }
        return {nullptr, 0};
    };

    // -----------------------------------------------------------------------
    // getVar: resolve operand at position i.
    // Tries variable table first, then falls back to literal allocation.
    // Returns {nullptr, 0} if neither succeeds.
    // -----------------------------------------------------------------------
    auto getVar = [&](const IRInstr &ins, int i) -> std::pair<llvm::Value *, int> {
        if (i >= (int)ins.operands.size())
            return {nullptr, 0};
        const std::string &tok = ins.operands[i].token;

        VarDesc d;
        if (resolveVar(tok, d))
            return {loadVarPtr(d), d.stride};

        // Layer A: literal (numeric or string)
        auto lit = allocLiteral(tok);
        if (lit.first)
            return lit;

        return {nullptr, 0};
    };

    // -----------------------------------------------------------------------
    // Pre-declare common function types.
    // -----------------------------------------------------------------------
    // Binary: (dst,sd, a,sa, b,sb, n, tags)
    auto *binOpTy = llvm::FunctionType::get(voidTy,
                                            {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
    // Unary: (dst,sd, a,sa, n, tags)
    auto *unOpTy = llvm::FunctionType::get(voidTy,
                                           {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
    // Ternary: (dst,sd, a,sa, b,sb, c,sc, n, tags)
    auto *ternOpTy = llvm::FunctionType::get(voidTy,
                                             {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
    // op_if_update(cond,scond, tags, n, numActive*, numPassive*)
    auto *ifUpdTy = llvm::FunctionType::get(voidTy,
                                            {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, ptrTy}, false);
    // op_else_update / op_endif_update(tags, n, numActive*, numPassive*)
    auto *elseUpdTy = llvm::FunctionType::get(voidTy,
                                              {ptrTy, i32Ty, ptrTy, ptrTy}, false);
    // op_gather_begin / op_gather_else / op_gather_end(numActive*, numPassive*) -> i32
    // (real signatures in rslOps.h take no tags/n — gather state lives on
    // currentShadingState, read internally by jitGatherBegin/Else/End).
    auto *gatherOpTy = llvm::FunctionType::get(i32Ty,
                                               {ptrTy, ptrTy}, false);

    // -----------------------------------------------------------------------
    // Emit helpers for common op patterns.
    // -----------------------------------------------------------------------
    // T025/T026 (contracts/op-uniform-collapse.md): an instruction is
    // uniform-classified iff its destination stride and every operand
    // stride are 0. When it is, the emitted call MUST pass n=1 and
    // tags=null TOGETHER (§2.3 forbids n=1 with a live tag pointer) —
    // this helper is the single place that pairing is enforced, so no
    // call site can emit one half without the other.
    auto collapseArgs = [&](int dstStrideVal,
                            std::initializer_list<int> operandStrides)
        -> std::pair<llvm::Value *, llvm::Value *> {
        bool uniform = (dstStrideVal == 0);
        for (int s : operandStrides)
            uniform = uniform && (s == 0);
        if (uniform)
            return {B.getInt32(1), llvm::ConstantPointerNull::get(ptrTy)};
        return {numVerts, tags};
    };

    auto emitBin = [&](const IRInstr &ins, const char *name,
                       llvm::Value *dst, llvm::Value *dstStride, int dstStrideVal) {
        auto [a, sa] = getVar(ins, 0);
        auto [b, sb] = getVar(ins, 1);
        if (!dst || !a || !b)
            return;
        auto *fn = declareOp(mod, name, binOpTy);
        auto [n, tg] = collapseArgs(dstStrideVal, {sa, sb});
        B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa), b, B.getInt32(sb), n, tg});
    };

    auto emitUn = [&](const IRInstr &ins, const char *name,
                      llvm::Value *dst, llvm::Value *dstStride, int dstStrideVal) {
        auto [a, sa] = getVar(ins, 0);
        if (!dst || !a)
            return;
        auto *fn = declareOp(mod, name, unOpTy);
        auto [n, tg] = collapseArgs(dstStrideVal, {sa});
        B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa), n, tg});
    };

    auto emitTern = [&](const IRInstr &ins, const char *name,
                        llvm::Value *dst, llvm::Value *dstStride, int dstStrideVal) {
        auto [a, sa] = getVar(ins, 0);
        auto [b, sb] = getVar(ins, 1);
        auto [c, sc] = getVar(ins, 2);
        if (!dst || !a || !b || !c)
            return;
        auto *fn = declareOp(mod, name, ternOpTy);
        auto [n, tg] = collapseArgs(dstStrideVal, {sa, sb, sc});
        B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa),
                          b, B.getInt32(sb), c, B.getInt32(sc), n, tg});
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
                if (!currentBlockHasTerminator(B))
                    B.CreateBr(fs.latchBB);
                B.SetInsertPoint(fs.latchBB);
            }
            if (blk.label == fs.exitLabel) {
                // Latch falls through back to condition (loop back).
                if (!currentBlockHasTerminator(B))
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
            if (!currentBlockHasTerminator(B))
                B.CreateBr(sc.latchBB);

            // Emit latch: call op_illuminance_next, branch body or exit.
            B.SetInsertPoint(sc.latchBB);
            auto *nextTy = llvm::FunctionType::get(i32Ty,
                                                   {ptrTy, i32Ty, ptrTy, ptrTy}, false);
            auto *nextFn = declareOp(mod, "op_illuminance_next", nextTy);
            auto *more = B.CreateCall(nextFn,
                                      {tags, numVerts, numActivePtr, numPassivePtr});
            auto *hasmr = B.CreateICmpNE(more, B.getInt32(0));
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
                if (!currentBlockHasTerminator(B))
                    B.CreateRetVoid();
                return true;
            }

            // ----------------------------------------------------------------
            // Labels / jumps — no-ops in the flat batch model.
            // Tags control per-vertex activity; we never skip instructions.
            // ----------------------------------------------------------------
            if (op == "jmp")
                continue;

            // ----------------------------------------------------------------
            // Resolve destination variable.
            // ----------------------------------------------------------------
            VarDesc dstDesc{};
            bool hasDst = !ins.result.empty() && resolveVar(ins.result, dstDesc);
            llvm::Value *dst = hasDst ? loadVarPtr(dstDesc) : nullptr;
            int dstStrideVal = hasDst ? dstDesc.stride : 3;
            llvm::Value *dstStride = B.getInt32(dstStrideVal);

            // Coverage gate: kHandledOpcodes is the single source of truth
            // the libshader coverage-guard ctest also reads (research.md
            // D3). Hardened (spec 017-jit-builtin-function-coverage, US2,
            // gate-hardening-contract.md): an opcode/function not in the
            // table is now a hard compile failure, not a silent skip — the
            // silent-skip behavior is exactly what let random()/urandom()
            // (issue #1) and 26 more builtin functions ship broken under
            // --jit with zero diagnostic. Sequencing requirement (FR-009):
            // this MUST NOT land before every builtin function any
            // currently-shipped shader calls has JIT support (confirmed
            // via research.md D4 -- zero shipped shaders reference any
            // function outside spec 017 US1's six once US1 lands).
            if (!isHandledOpcode(op)) {
                fprintf(stderr,
                        "llvmEmitter: JIT coverage gap for '%s': builtin '%s' has no "
                        "emitFunction() case (kHandledOpcodes[] is missing it)\n",
                        std::string(func->getName()).c_str(), op.c_str());
                return false;
            }

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
            // Layer G: gatherHeader — sets up the CGatherBundle consumed by
            // gather/gatherElse/gatherEnd below. IR operand layout (see
            // expression.cpp CGatherFunction::getCode, the sole producer of
            // this opcode's text): operands[0]=category (a ray-trace category
            // string; unread by the interpreter's own GATHERHEADEREXPR_PRE/
            // GATHEREXPR_PRE — gather() category filtering isn't implemented
            // there, so the JIT must match by ignoring it too, not add
            // filtering the interpreter lacks), [1]=P, [2]=D, [3]=sampleCone,
            // [4]=samples, [5..]=alternating name/value pairs (the 5 named
            // overrides bias/maxdist/samplebase/distribution/label, and any
            // surface:/ray: output bindings). Marshals those pairs into the
            // names/valuePtrs/steps/isVarying arrays op_gatherHeader expects
            // (rslOps.h) and delegates everything else to
            // CShadingContext::jitGatherHeaderBegin via that trampoline —
            // per-name dispatch (override vs. output) happens at runtime
            // inside the real CGatherLookup::bind()/addOutput(), not here.
            // ================================================================
            if (op == "gatherHeader") {
                if (ins.operands.size() < 5)
                    continue;
                auto [P, sP] = getVar(ins, 1);
                auto [D, sD] = getVar(ins, 2);
                auto [sampleCone, ssc] = getVar(ins, 3);
                auto [samplesPtr, sSam] = getVar(ins, 4);
                if (!P || !D || !sampleCone || !samplesPtr)
                    continue;
                llvm::Value *samplesVal = B.CreateLoad(f32Ty, samplesPtr);

                const size_t numPairOperands = ins.operands.size() - 5;
                if (numPairOperands % 2 != 0)
                    continue;
                const int numPairs = (int)(numPairOperands / 2);

                llvm::Value *namesArr, *valuePtrsArr, *stepsArr, *isVaryingArr;
                bool ok = true;
                if (numPairs > 0) {
                    auto *arrPtrTy = llvm::ArrayType::get(ptrTy, numPairs);
                    auto *arrI32Ty = llvm::ArrayType::get(i32Ty, numPairs);
                    namesArr = B.CreateAlloca(arrPtrTy, nullptr, "gh_names");
                    valuePtrsArr = B.CreateAlloca(arrPtrTy, nullptr, "gh_values");
                    stepsArr = B.CreateAlloca(arrI32Ty, nullptr, "gh_steps");
                    isVaryingArr = B.CreateAlloca(arrI32Ty, nullptr, "gh_varying");

                    for (int k = 0; k < numPairs && ok; k++) {
                        const std::string &nameTok = ins.operands[5 + 2 * k].token;
                        if (nameTok.size() < 2 || nameTok.front() != '"') {
                            ok = false;
                            break;
                        }
                        std::string nameStr = nameTok.substr(1, nameTok.size() - 2);

                        // Element size for the 5 fixed CShadingScratch override
                        // fields (shading.h: bias/sampleBase/maxDist are float,
                        // distribution/label are const char*) — known statically
                        // from the name, not derived from the RSL value's own
                        // type. Output-token pairs (anything else) leave these
                        // unused: jitGatherHeaderBegin only consults step/
                        // isVarying for pairs that actually grew CGatherLookup's
                        // uniforms/varyings arrays via bind(), which addOutput()
                        // never does.
                        llvm::Value *valPtr = nullptr;
                        int stepBytes = 0;
                        bool isVarying = false;
                        if (nameStr == "distribution" || nameStr == "label") {
                            // String-valued override: the value token is a
                            // quoted string literal (or, rarely, a string
                            // variable) — getVar() only resolves numeric
                            // literals/variables, so mirror the texture/
                            // environment name-resolution pattern instead.
                            const std::string &valTok = ins.operands[5 + 2 * k + 1].token;
                            VarDesc valDesc;
                            if (resolveVar(valTok, valDesc)) {
                                valPtr = loadVarPtr(valDesc);
                            }
                            else {
                                std::string s = valTok;
                                if (s.size() >= 2 && s.front() == '"')
                                    s = s.substr(1, s.size() - 2);
                                llvm::Value *sptr = B.CreateGlobalString(s, "gh_strval");
                                llvm::Value *alloc = B.CreateAlloca(ptrTy, nullptr, "gh_strval_pp");
                                B.CreateStore(sptr, alloc);
                                valPtr = alloc;
                            }
                            stepBytes = (int)sizeof(const char *);
                            isVarying = false; // RSL strings are always uniform
                        }
                        else {
                            auto [vp, valStride] = getVar(ins, 5 + 2 * k + 1);
                            if (!vp) {
                                ok = false;
                                break;
                            }
                            valPtr = vp;
                            if (nameStr == "bias" || nameStr == "maxdist" || nameStr == "samplebase") {
                                stepBytes = (int)sizeof(float);
                                isVarying = (valStride != 0);
                            }
                        }

                        llvm::Value *namePtr = B.CreateGlobalString(nameStr, "gh_name");
                        B.CreateStore(namePtr, B.CreateGEP(arrPtrTy, namesArr, {B.getInt32(0), B.getInt32(k)}));
                        B.CreateStore(valPtr, B.CreateGEP(arrPtrTy, valuePtrsArr, {B.getInt32(0), B.getInt32(k)}));
                        B.CreateStore(B.getInt32(stepBytes), B.CreateGEP(arrI32Ty, stepsArr, {B.getInt32(0), B.getInt32(k)}));
                        B.CreateStore(B.getInt32(isVarying ? 1 : 0), B.CreateGEP(arrI32Ty, isVaryingArr, {B.getInt32(0), B.getInt32(k)}));
                    }
                }
                else {
                    llvm::Value *nullPtr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(ctx));
                    namesArr = valuePtrsArr = stepsArr = isVaryingArr = nullPtr;
                }
                if (!ok)
                    continue;

                // strideP/sD/ssc come from getVar() above (0=uniform, 1=float,
                // 3=vector) -- must be threaded through, not discarded, so
                // op_gatherHeader's per-vertex walk doesn't advance a uniform
                // source (e.g. a compile-time-constant sampleCone promoted by
                // CUniformLiftingPass) past its single-element allocation.
                auto *ghTy = llvm::FunctionType::get(voidTy,
                                                     {ptrTy, ptrTy, ptrTy, ptrTy, i32Ty,
                                                      ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, f32Ty},
                                                     false);
                auto *fn = declareOp(mod, "op_gatherHeader", ghTy);
                B.CreateCall(fn, {namesArr, valuePtrsArr, stepsArr, isVaryingArr, B.getInt32(numPairs),
                                  P, B.getInt32(sP), D, B.getInt32(sD), sampleCone, B.getInt32(ssc),
                                  samplesVal});
                continue;
            }

            // ================================================================
            // Layer G: gather() / gatherElse / gatherEnd
            // gather/gatherElse are tag-mask-only (like if/else — their own
            // jmp(argument(0)) forward-skip targets are subsumed by masking).
            // gatherEnd is the one genuine backward branch (GI sample loop),
            // wired inline via headerBB/exitBB — see GatherScope above.
            // ================================================================
            if (op == "gather") {
                // headerBB must contain the op_gather_begin call itself, since
                // gatherEnd's backward branch re-enters here — GATHERENDEXPR_PRE's
                // jmp(argument(0)) re-runs the interpreter's GATHEREXPR_PRE (fresh
                // sampling + traceEx) on every remaining-sample iteration, not just
                // the tag-mask body that follows.
                auto *headerBB = llvm::BasicBlock::Create(ctx, "gather.header", func);
                auto *exitBB = llvm::BasicBlock::Create(ctx, "gather.exit", func);
                B.CreateBr(headerBB);
                B.SetInsertPoint(headerBB);

                auto *fn = declareOp(mod, "op_gather_begin", gatherOpTy);
                B.CreateCall(fn, {numActivePtr, numPassivePtr});

                gatherStack.push_back({headerBB, exitBB});
                continue;
            }
            if (op == "gatherElse") {
                auto *fn = declareOp(mod, "op_gather_else", gatherOpTy);
                B.CreateCall(fn, {numActivePtr, numPassivePtr});
                continue;
            }
            if (op == "gatherEnd") {
                if (gatherStack.empty())
                    continue;
                GatherScope sc = gatherStack.back();
                gatherStack.pop_back();

                auto *fn = declareOp(mod, "op_gather_end", gatherOpTy);
                auto *res = B.CreateCall(fn, {numActivePtr, numPassivePtr});
                auto *more = B.CreateICmpNE(res, B.getInt32(0));
                B.CreateCondBr(more, sc.headerBB, sc.exitBB);

                B.SetInsertPoint(sc.exitBB);
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
                    auto [ang, st] = getVar(ins, 2);
                    if (from && axis && ang) {
                        // (from,sf, axis,sa, angle,st, tags, n, numActive*, numPassive*)
                        auto *ty = llvm::FunctionType::get(voidTy,
                                                           {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty,
                                                            ptrTy, i32Ty, ptrTy, ptrTy},
                                                           false);
                        auto *fn = declareOp(mod, "op_illuminate3_begin", ty);
                        B.CreateCall(fn, {from, B.getInt32(sf),
                                          axis, B.getInt32(sa),
                                          ang, B.getInt32(st),
                                          tags, numVerts, numActivePtr, numPassivePtr});
                    }
                }
                else if (from) {
                    // ILLUMINATE1: illuminate from #!Label
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, ptrTy}, false);
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
                                                       {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, ptrTy}, false);
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
                auto [P, sp] = getVar(ins, 0);
                auto [N, sn] = getVar(ins, 1);
                auto [ang, sa] = getVar(ins, 2);
                // operand 3 = body label (unused: body starts right after branch)
                std::string exitLabel;
                if (ins.operands.size() >= 5)
                    exitLabel = ins.operands[4].token; // "#!LabelN"

                // Function type: (ptr,i32, ptr,i32, ptr,i32, ptr, i32, ptr*, ptr*) → i32
                auto *beginTy = llvm::FunctionType::get(i32Ty,
                                                        {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty,
                                                         ptrTy, i32Ty, ptrTy, ptrTy},
                                                        false);
                auto *beginFn = declareOp(mod, "op_illuminance_begin", beginTy);

                llvm::BasicBlock *bodyBB = nullptr;
                llvm::BasicBlock *latchBB = nullptr;
                llvm::BasicBlock *exitBB = nullptr;

                if (!P || !N || !ang || exitLabel.empty()) {
                    // Malformed — skip body entirely (stub behaviour)
                    continue;
                }

                bodyBB = llvm::BasicBlock::Create(ctx, "illum_body", func);
                latchBB = llvm::BasicBlock::Create(ctx, "illum_latch", func);
                exitBB = llvm::BasicBlock::Create(ctx, "illum_exit", func);

                auto *has = B.CreateCall(beginFn,
                                         {P, B.getInt32(sp),
                                          N, B.getInt32(sn),
                                          ang, B.getInt32(sa),
                                          tags, numVerts, numActivePtr, numPassivePtr});
                auto *ok = B.CreateICmpNE(has, B.getInt32(0));
                B.CreateCondBr(ok, bodyBB, exitBB);

                // Switch to body BB — subsequent instructions emit there.
                B.SetInsertPoint(bodyBB);

                illumStack.push_back({exitLabel, bodyBB, latchBB, exitBB});
                continue;
            }
            // endilluminance: handled at the block boundary in the outer loop.
            // By the time this instruction is reached, we have already switched
            // the insert point to the exitBB and popped the scope.
            if (op == "endilluminance")
                continue;

            // ================================================================
            // Layer F: for / while loop
            // forbegin condLabel latchLabel exitLabel
            // ================================================================
            if (op == "forbegin" || op == "whilebegin") {
                // Operands: [condLabel, latchLabel, exitLabel]
                std::string latchLabel, exitLabel;
                if (ins.operands.size() >= 2)
                    latchLabel = ins.operands[1].token;
                if (ins.operands.size() >= 3)
                    exitLabel = ins.operands[2].token;
                if (latchLabel.empty() || exitLabel.empty())
                    continue;

                auto *condBB = llvm::BasicBlock::Create(ctx, "for_cond", func);
                auto *bodyBB = llvm::BasicBlock::Create(ctx, "for_body", func);
                auto *latchBB = llvm::BasicBlock::Create(ctx, "for_latch", func);
                auto *exitBB = llvm::BasicBlock::Create(ctx, "for_exit", func);

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
                if (forStack.empty())
                    continue;
                ForScope &fs = forStack.back();

                auto [cond, sc] = getVar(ins, 0);
                if (cond) {
                    // void op_for_check(cond, sc, execCountPtr, tags, n, numActive*, numPassive*)
                    auto *chkTy = llvm::FunctionType::get(voidTy,
                                                          {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, ptrTy, ptrTy}, false);
                    auto *chkFn = declareOp(mod, "op_for_check", chkTy);
                    B.CreateCall(chkFn, {cond, B.getInt32(sc),
                                         fs.execCountPtr, tags, numVerts,
                                         numActivePtr, numPassivePtr});
                }
                // Branch: numActive > 0 → body, else → exit.
                auto *na = B.CreateLoad(i32Ty, numActivePtr);
                auto *ok = B.CreateICmpNE(na, B.getInt32(0));
                B.CreateCondBr(ok, fs.bodyBB, fs.exitBB);
                B.SetInsertPoint(fs.bodyBB);
                continue;
            }

            // 'forend' / 'endfor': handled at the exitLabel block boundary.
            if (op == "forend" || op == "endfor" || op == "endwhile")
                continue;

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
                if (!forStack.empty() && !currentBlockHasTerminator(B))
                    B.CreateBr(forStack.back().latchBB);
                continue;
            }

            // ================================================================
            // Binary arithmetic
            // ================================================================
            if (op == "addvv")
                emitBin(ins, "op_addvv", dst, dstStride, dstStrideVal);
            else if (op == "subvv")
                emitBin(ins, "op_subvv", dst, dstStride, dstStrideVal);
            else if (op == "mulvv")
                emitBin(ins, "op_mulvv", dst, dstStride, dstStrideVal);
            else if (op == "divvv")
                emitBin(ins, "op_divvv", dst, dstStride, dstStrideVal);
            else if (op == "addff")
                emitBin(ins, "op_addff", dst, dstStride, dstStrideVal);
            else if (op == "subff")
                emitBin(ins, "op_subff", dst, dstStride, dstStrideVal);
            else if (op == "mulff")
                emitBin(ins, "op_mulff", dst, dstStride, dstStrideVal);
            else if (op == "divff")
                emitBin(ins, "op_divff", dst, dstStride, dstStrideVal);
            else if (op == "addvf" || op == "addvf2")
                emitBin(ins, "op_addvf", dst, dstStride, dstStrideVal);
            else if (op == "subvf")
                emitBin(ins, "op_subvf", dst, dstStride, dstStrideVal);
            else if (op == "mulvf" || op == "mulvf2")
                emitBin(ins, "op_mulvf", dst, dstStride, dstStrideVal);
            else if (op == "divvf")
                emitBin(ins, "op_divvf", dst, dstStride, dstStrideVal);
            else if (op == "dot")
                emitBin(ins, "op_dot", dst, dstStride, dstStrideVal);
            else if (op == "cross")
                emitBin(ins, "op_cross", dst, dstStride, dstStrideVal);
            else if (op == "pow")
                emitBin(ins, "op_pow", dst, dstStride, dstStrideVal);
            else if (op == "mod")
                emitBin(ins, "op_mod", dst, dstStride, dstStrideVal);
            else if (op == "atan2")
                emitBin(ins, "op_atan2", dst, dstStride, dstStrideVal);
            else if (op == "flt")
                emitBin(ins, "op_flt", dst, dstStride, dstStrideVal);
            else if (op == "fle")
                emitBin(ins, "op_fle", dst, dstStride, dstStrideVal);
            else if (op == "fgt")
                emitBin(ins, "op_fgt", dst, dstStride, dstStrideVal);
            else if (op == "fge")
                emitBin(ins, "op_fge", dst, dstStride, dstStrideVal);
            else if (op == "feq")
                emitBin(ins, "op_feq", dst, dstStride, dstStrideVal);
            else if (op == "fne")
                emitBin(ins, "op_fne", dst, dstStride, dstStrideVal);
            // "fegt" (float >=) has no dedicated op_* — the interpreter's
            // >= and op_fge share the same comparison, so delegate there.
            else if (op == "fegt")
                emitBin(ins, "op_fge", dst, dstStride, dstStrideVal);

            // ================================================================
            // Vector comparison / logic
            // ================================================================
            else if (op == "veql")
                emitBin(ins, "op_veql", dst, dstStride, dstStrideVal);
            else if (op == "vneql")
                emitBin(ins, "op_vneql", dst, dstStride, dstStrideVal);
            else if (op == "velt")
                emitBin(ins, "op_velt", dst, dstStride, dstStrideVal);
            else if (op == "vlt")
                emitBin(ins, "op_vlt", dst, dstStride, dstStrideVal);
            else if (op == "vegt")
                emitBin(ins, "op_vegt", dst, dstStride, dstStrideVal);
            else if (op == "vgt")
                emitBin(ins, "op_vgt", dst, dstStride, dstStrideVal);

            // ================================================================
            // Matrix arithmetic
            // ================================================================
            else if (op == "mulmm")
                emitBin(ins, "op_mulmm", dst, dstStride, dstStrideVal);
            else if (op == "addmm")
                emitBin(ins, "op_addmm", dst, dstStride, dstStrideVal);
            else if (op == "submm")
                emitBin(ins, "op_submm", dst, dstStride, dstStrideVal);
            else if (op == "divmm")
                emitBin(ins, "op_divmm", dst, dstStride, dstStrideVal);

            // rotate()/scale()/translate() matrix-builder overloads
            // (spec 017-jit-builtin-function-coverage, US4): only
            // Translatem "m=mp"/Rotatem "m=mfv"/Scalem "m=mp" -- NOT
            // Rotatep "p=pfpp" (point-about-axis rotation, a separate,
            // unscoped overload).
            else if (op == "translate")
                emitBin(ins, "op_translate", dst, dstStride, dstStrideVal);
            else if (op == "scale")
                emitBin(ins, "op_scale", dst, dstStride, dstStrideVal);
            else if (op == "rotate") {
                auto [m, sm] = getVar(ins, 0);
                auto [angle, sa] = getVar(ins, 1);
                auto [axis, sax] = getVar(ins, 2);
                if (!dst || !m || !angle || !axis)
                    continue;
                auto *fn = declareOp(mod, "op_rotate", ternOpTy);
                auto [n, tg] = collapseArgs(dstStrideVal, {sm, sa, sax});
                B.CreateCall(fn, {dst, dstStride, m, B.getInt32(sm),
                                  angle, B.getInt32(sa), axis, B.getInt32(sax), n, tg});
            }

            // ================================================================
            // Unary arithmetic / math
            // ================================================================
            else if (op == "movevv")
                emitUn(ins, "op_movevv", dst, dstStride, dstStrideVal);
            else if (op == "moveff")
                emitUn(ins, "op_moveff", dst, dstStride, dstStrideVal);
            else if (op == "movess")
                emitUn(ins, "op_movess", dst, dstStride, dstStrideVal);
            else if (op == "not")
                emitUn(ins, "op_not", dst, dstStride, dstStrideVal);
            else if (op == "negm")
                emitUn(ins, "op_negm", dst, dstStride, dstStrideVal);
            else if (op == "movemm")
                emitUn(ins, "op_movemm", dst, dstStride, dstStrideVal);
            else if (op == "mfromv")
                emitUn(ins, "op_mfromv", dst, dstStride, dstStrideVal);
            else if (op == "negv")
                emitUn(ins, "op_negv", dst, dstStride, dstStrideVal);
            else if (op == "negf")
                emitUn(ins, "op_negf", dst, dstStride, dstStrideVal);
            else if (op == "normalize")
                emitUn(ins, "op_normalize", dst, dstStride, dstStrideVal);
            else if (op == "length")
                emitUn(ins, "op_length", dst, dstStride, dstStrideVal);
            else if (op == "sqrt")
                emitUn(ins, "op_sqrt", dst, dstStride, dstStrideVal);
            else if (op == "inversesqrt")
                emitUn(ins, "op_inversesqrt", dst, dstStride, dstStrideVal);
            else if (op == "abs")
                emitUn(ins, "op_abs", dst, dstStride, dstStrideVal);
            else if (op == "sign")
                emitUn(ins, "op_sign", dst, dstStride, dstStrideVal);
            else if (op == "floor")
                emitUn(ins, "op_floor", dst, dstStride, dstStrideVal);
            else if (op == "ceil")
                emitUn(ins, "op_ceil", dst, dstStride, dstStrideVal);
            else if (op == "exp")
                emitUn(ins, "op_exp", dst, dstStride, dstStrideVal);
            else if (op == "log")
                emitUn(ins, "op_log", dst, dstStride, dstStrideVal);
            else if (op == "sin")
                emitUn(ins, "op_sin", dst, dstStride, dstStrideVal);
            else if (op == "cos")
                emitUn(ins, "op_cos", dst, dstStride, dstStrideVal);
            else if (op == "tan")
                emitUn(ins, "op_tan", dst, dstStride, dstStrideVal);
            else if (op == "asin")
                emitUn(ins, "op_asin", dst, dstStride, dstStrideVal);
            else if (op == "acos")
                emitUn(ins, "op_acos", dst, dstStride, dstStrideVal);
            else if (op == "atan")
                emitUn(ins, "op_atan", dst, dstStride, dstStrideVal);
            else if (op == "xcomp")
                emitUn(ins, "op_xcomp", dst, dstStride, dstStrideVal);
            else if (op == "ycomp")
                emitUn(ins, "op_ycomp", dst, dstStride, dstStrideVal);
            else if (op == "zcomp")
                emitUn(ins, "op_zcomp", dst, dstStride, dstStrideVal);

            // ================================================================
            // Ternary (clamp, mix)
            // ================================================================
            else if (op == "clampf")
                emitTern(ins, "op_clampf", dst, dstStride, dstStrideVal);
            else if (op == "clampv")
                emitTern(ins, "op_clampv", dst, dstStride, dstStrideVal);
            // Generic clamp: dispatch by proto (f=fff → clampf, else → clampv)
            else if (op == "clamp") {
                bool isFloat = !ins.proto.empty() && ins.proto[0] == 'f';
                emitTern(ins, isFloat ? "op_clampf" : "op_clampv", dst, dstStride, dstStrideVal);
            }
            else if (op == "mixf")
                emitTern(ins, "op_mixf", dst, dstStride, dstStrideVal);
            else if (op == "mixv")
                emitTern(ins, "op_mixv", dst, dstStride, dstStrideVal);
            // Generic mix: dispatch by proto (f=fff → mixf, else → mixv)
            else if (op == "mix") {
                bool isFloat = !ins.proto.empty() && ins.proto[0] == 'f';
                emitTern(ins, isFloat ? "op_mixf" : "op_mixv", dst, dstStride, dstStrideVal);
            }

            // ================================================================
            // Data movement — vufloat / vuvector (uniform broadcast)
            // Source stride is always 0 (uniform), regardless of declared stride.
            // ================================================================
            else if (op == "vuvector") {
                auto [a, sa] = getVar(ins, 0);
                if (!dst || !a)
                    continue;
                auto *fn = declareOp(mod, "op_movevv", unOpTy);
                auto [n, tg] = collapseArgs(dstStrideVal, {0});
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(0), n, tg});
            }
            else if (op == "vufloat") {
                auto [a, sa] = getVar(ins, 0);
                if (!dst || !a)
                    continue;
                auto *fn = declareOp(mod, "op_moveff", unOpTy);
                auto [n, tg] = collapseArgs(dstStrideVal, {0});
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(0), n, tg});
            }
            else if (op == "vumatrix") {
                auto [a, sa] = getVar(ins, 0);
                if (!dst || !a)
                    continue;
                auto *fn = declareOp(mod, "op_movemm", unOpTy);
                auto [n, tg] = collapseArgs(dstStrideVal, {0});
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(0), n, tg});
            }
            else if (op == "vustring") {
                auto [a, sa] = getVar(ins, 0);
                if (!dst || !a)
                    continue;
                auto *fn = declareOp(mod, "op_movess", unOpTy);
                auto [n, tg] = collapseArgs(dstStrideVal, {0});
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(0), n, tg});
            }

            // ================================================================
            // mfromf: 1 operand → op_mfromf (uniform broadcast);
            // 16 operands → op_mfromf16 (explicit element list)
            // ================================================================
            else if (op == "mfromf") {
                if (ins.operands.size() >= 16) {
                    llvm::Value *eArr = B.CreateAlloca(
                        llvm::ArrayType::get(ptrTy, 16), nullptr, "mfromf16_e");
                    llvm::Value *seArr = B.CreateAlloca(
                        llvm::ArrayType::get(i32Ty, 16), nullptr, "mfromf16_se");
                    bool ok = true;
                    bool uniform = (dstStrideVal == 0);
                    for (int i = 0; i < 16; ++i) {
                        auto [ei, sei] = getVar(ins, i);
                        if (!ei) {
                            ok = false;
                            break;
                        }
                        uniform = uniform && (sei == 0);
                        llvm::Value *ePtr = B.CreateGEP(
                            llvm::ArrayType::get(ptrTy, 16), eArr,
                            {B.getInt32(0), B.getInt32(i)});
                        B.CreateStore(ei, ePtr);
                        llvm::Value *sePtr = B.CreateGEP(
                            llvm::ArrayType::get(i32Ty, 16), seArr,
                            {B.getInt32(0), B.getInt32(i)});
                        B.CreateStore(B.getInt32(sei), sePtr);
                    }
                    if (!dst || !ok)
                        continue;
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_mfromf16", ty);
                    llvm::Value *n16 = uniform ? B.getInt32(1) : numVerts;
                    llvm::Value *tg16 = uniform ? llvm::ConstantPointerNull::get(ptrTy) : tags;
                    B.CreateCall(fn, {dst, dstStride, eArr, seArr, n16, tg16});
                }
                else {
                    emitUn(ins, "op_mfromf", dst, dstStride, dstStrideVal);
                }
            }

            // ================================================================
            // Vector construction
            // vfromf: 1 operand → broadcast; 3 operands → construct from floats
            // ================================================================
            else if (op == "vfromf") {
                if (ins.operands.size() >= 3) {
                    // 3-operand form: construct (f0, f1, f2)
                    auto [f0, s0] = getVar(ins, 0);
                    auto [f1, s1] = getVar(ins, 1);
                    auto [f2, s2] = getVar(ins, 2);
                    if (!dst || !f0 || !f1 || !f2)
                        continue;
                    auto *fn = declareOp(mod, "op_vfromfff", ternOpTy);
                    auto [n, tg] = collapseArgs(dstStrideVal, {s0, s1, s2});
                    B.CreateCall(fn, {dst, dstStride,
                                      f0, B.getInt32(s0), f1, B.getInt32(s1),
                                      f2, B.getInt32(s2), n, tg});
                }
                else {
                    // 1-operand form: broadcast single float
                    emitUn(ins, "op_vfromf", dst, dstStride, dstStrideVal);
                }
            }
            else if (op == "vfromvff") {
                auto [v, sv] = getVar(ins, 0);
                auto [f1, s1] = getVar(ins, 1);
                auto [f2, s2] = getVar(ins, 2);
                if (!dst || !v || !f1 || !f2)
                    continue;
                auto *fn = declareOp(mod, "op_vfromvff",
                                     llvm::FunctionType::get(voidTy,
                                                             {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false));
                auto [n, tg] = collapseArgs(dstStrideVal, {sv, s1, s2});
                B.CreateCall(fn, {dst, dstStride, v, B.getInt32(sv),
                                  f1, B.getInt32(s1), f2, B.getInt32(s2), n, tg});
            }
            else if (op == "vfromfff") {
                auto [f0, s0] = getVar(ins, 0);
                auto [f1, s1] = getVar(ins, 1);
                auto [f2, s2] = getVar(ins, 2);
                if (!dst || !f0 || !f1 || !f2)
                    continue;
                auto *fn = declareOp(mod, "op_vfromfff", ternOpTy);
                auto [n, tg] = collapseArgs(dstStrideVal, {s0, s1, s2});
                B.CreateCall(fn, {dst, dstStride,
                                  f0, B.getInt32(s0), f1, B.getInt32(s1),
                                  f2, B.getInt32(s2), n, tg});
            }

            // ================================================================
            // Component set/get
            // ================================================================
            else if (op == "setxcomp" || op == "setycomp" || op == "setzcomp") {
                auto [a, sa] = getVar(ins, 0);
                if (!dst || !a)
                    continue;
                const char *fnName = (op == "setxcomp") ? "op_setxcomp" : (op == "setycomp") ? "op_setycomp"
                                                                                             : "op_setzcomp";
                auto *fn = declareOp(mod, fnName, unOpTy);
                auto [n, tg] = collapseArgs(dstStrideVal, {sa});
                B.CreateCall(fn, {dst, dstStride, a, B.getInt32(sa), n, tg});
            }

            // comp()/MComp() (spec 017-jit-builtin-function-coverage, US1):
            // pure indexed read, same mnemonic "comp" for both the 2-operand
            // vector form (Comp, "f=vf") and the 3-operand matrix form
            // (MComp, "f=mff") -- branch on operand count like noise/snoise
            // branches on operand type.
            else if (op == "comp") {
                if (!dst)
                    continue;
                if (ins.operands.size() >= 3) {
                    auto [m, sm] = getVar(ins, 0);
                    auto [r, sr] = getVar(ins, 1);
                    auto [c, sc] = getVar(ins, 2);
                    if (!m || !r || !c)
                        continue;
                    auto *fn = declareOp(mod, "op_mcomp", ternOpTy);
                    auto [n, tg] = collapseArgs(dstStrideVal, {sm, sr, sc});
                    B.CreateCall(fn, {dst, dstStride, m, B.getInt32(sm),
                                      r, B.getInt32(sr), c, B.getInt32(sc), n, tg});
                }
                else {
                    auto [v, sv] = getVar(ins, 0);
                    auto [idx, si] = getVar(ins, 1);
                    if (!v || !idx)
                        continue;
                    auto *fn = declareOp(mod, "op_comp", binOpTy);
                    auto [n, tg] = collapseArgs(dstStrideVal, {sv, si});
                    B.CreateCall(fn, {dst, dstStride, v, B.getInt32(sv),
                                      idx, B.getInt32(si), n, tg});
                }
            }

            // setcomp()/setmcomp() (spec 017-jit-builtin-function-coverage,
            // US4): runtime-indexed write, generalizing the fixed-index
            // setxcomp/setycomp/setzcomp shape above. Same mnemonic
            // "setcomp" for both the 2-operand vector form (SetComp,
            // "o=Vff") and the 3-operand matrix form (SetMComp,
            // "o=Mfff") -- branch on operand count like comp() above.
            // `dst` IS the vector/matrix being mutated (the IR's bytecode
            // text lists it as the instruction's destination even though
            // the RSL prototype's result type is 'o'/void, matching
            // setxcomp's own dst-as-mutated-vector convention) -- no
            // separate "v"/"m" operand to resolve.
            else if (op == "setcomp") {
                if (!dst)
                    continue;
                if (ins.operands.size() >= 3) {
                    auto [r, sr] = getVar(ins, 0);
                    auto [c, sc] = getVar(ins, 1);
                    auto [val, sf] = getVar(ins, 2);
                    if (!r || !c || !val)
                        continue;
                    auto *fn = declareOp(mod, "op_setmcomp", ternOpTy);
                    auto [n, tg] = collapseArgs(dstStrideVal, {sr, sc, sf});
                    B.CreateCall(fn, {dst, dstStride, r, B.getInt32(sr),
                                      c, B.getInt32(sc), val, B.getInt32(sf), n, tg});
                }
                else {
                    auto [idx, si] = getVar(ins, 0);
                    auto [val, sf] = getVar(ins, 1);
                    if (!idx || !val)
                        continue;
                    auto *fn = declareOp(mod, "op_setcomp", binOpTy);
                    auto [n, tg] = collapseArgs(dstStrideVal, {si, sf});
                    B.CreateCall(fn, {dst, dstStride, idx, B.getInt32(si),
                                      val, B.getInt32(sf), n, tg});
                }
            }

            // ================================================================
            // Geometry
            // ================================================================
            else if (op == "faceforward") {
                auto [nIn, sn] = getVar(ins, 0);
                auto [iIn, si] = getVar(ins, 1);
                auto [ngIn, sng] = (ins.operands.size() > 2)
                                       ? getVar(ins, 2)
                                       : std::make_pair((llvm::Value *)nullptr, 0);
                if (!dst || !nIn || !iIn)
                    continue;
                if (!ngIn) {
                    VarDesc ngDesc;
                    if (resolveVar("Ng", ngDesc)) {
                        ngIn = loadVarPtr(ngDesc);
                        sng = ngDesc.stride;
                    }
                    else {
                        ngIn = nIn;
                        sng = sn;
                    }
                }
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_faceforward", ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {sn, si, sng});
                B.CreateCall(fn, {dst, dstStride,
                                  nIn, B.getInt32(sn),
                                  iIn, B.getInt32(si),
                                  ngIn, B.getInt32(sng),
                                  n, tg});
            }
            else if (op == "smoothstep") {
                auto [e0, s0] = getVar(ins, 0);
                auto [e1, s1] = getVar(ins, 1);
                auto [x, sx] = getVar(ins, 2);
                if (!dst || !e0 || !e1 || !x)
                    continue;
                auto *fn = declareOp(mod, "rsl_smoothstep", ternOpTy);
                auto [n, tg] = collapseArgs(dstStrideVal, {s0, s1, sx});
                B.CreateCall(fn, {dst, dstStride,
                                  e0, B.getInt32(s0), e1, B.getInt32(s1),
                                  x, B.getInt32(sx), n, tg});
            }

            // ================================================================
            // Space transforms (Layer G stub — op_pfrom, op_vtransform,
            // op_ntransform already in rslOps.h)
            // Quoted string operand is passed as a global char* constant.
            // ================================================================
            else if (op == "pfrom" || op == "vtransform" ||
                     op == "ntransform" || op == "transform") {
                // operands: space_string src
                if (ins.operands.size() < 2)
                    continue;
                const std::string &spaceToken = ins.operands[0].token;
                auto [src, ss] = getVar(ins, 1);
                if (!dst || !src)
                    continue;

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
                }
                else if (op == "vtransform") {
                    fnName = "op_vtransform";
                }
                else if (op == "transform") {
                    // Determine by proto: "n=..." → ntransform, "v=..." → vtransform, else ptransform.
                    // RSL transform() goes current→named (uses "to" matrix), unlike pfrom which is named→current.
                    fnName = (!ins.proto.empty() && ins.proto[0] == 'n')   ? "op_ntransform"
                             : (!ins.proto.empty() && ins.proto[0] == 'v') ? "op_vtransform"
                                                                           : "op_ptransform";
                }
                else {
                    fnName = "op_pfrom";
                }
                auto *fn = declareOp(mod, fnName, ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {ss});
                B.CreateCall(fn, {dst, dstStride, spacePtr, src, B.getInt32(ss), n, tg});
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
                if (ins.operands.size() < 2)
                    continue;
                const std::string &spaceToken = ins.operands[0].token;
                auto [src, ss] = getVar(ins, 1);
                if (!dst || !src)
                    continue;

                std::string spaceName = spaceToken;
                if (spaceName.size() >= 2 && spaceName.front() == '"')
                    spaceName = spaceName.substr(1, spaceName.size() - 2);

                llvm::Value *spacePtr = B.CreateGlobalString(spaceName, "space_str");

                // (float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                const char *fnName = (op == "cfrom")   ? "op_cfrom"
                                     : (op == "mfrom") ? "op_mfrom"
                                                       : "op_ctransform";
                auto *fn = declareOp(mod, fnName, ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {ss});
                B.CreateCall(fn, {dst, dstStride, spacePtr, src, B.getInt32(ss), n, tg});
            }

            // ================================================================
            // Lighting (batch wrappers)
            // ================================================================
            else if (op == "ambient") {
                if (!dst)
                    continue;
                // op_ambient_batch(result, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy, {ptrTy, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_ambient_batch", ty);
                B.CreateCall(fn, {dst, numVerts, tags});
            }
            else if (op == "diffuse") {
                auto [nf, sn] = getVar(ins, 0);
                if (!dst || !nf)
                    continue;
                // op_diffuse_batch(result, sr, Nf, sn, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_diffuse_batch", ty);
                B.CreateCall(fn, {dst, dstStride, nf, B.getInt32(sn), numVerts, tags});
            }
            else if (op == "specular") {
                auto [nf, sn] = getVar(ins, 0);
                auto [v, sv] = getVar(ins, 1);
                auto [r, sr] = getVar(ins, 2);
                if (!dst || !nf || !v || !r)
                    continue;
                // op_specular_batch(result, Nf, V, roughness, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, ptrTy, ptrTy, ptrTy, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_specular_batch", ty);
                B.CreateCall(fn, {dst, nf, v, r, numVerts, tags});
            }
            else if (op == "phong") {
                // phong() (spec 017-jit-builtin-function-coverage, US5):
                // same dispatch shape as specular immediately above --
                // `size` is uniform-only, matching specular's own
                // `roughness` argument precedent (see callPhong's comment
                // in shading.cpp for the full rationale).
                auto [nf, sn] = getVar(ins, 0);
                auto [v, sv] = getVar(ins, 1);
                auto [size, ssize] = getVar(ins, 2);
                if (!dst || !nf || !v || !size)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, ptrTy, ptrTy, ptrTy, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_phong_batch", ty);
                B.CreateCall(fn, {dst, nf, v, size, numVerts, tags});
            }

            // ================================================================
            // Layer G — random / urandom (stateful RNG via
            // libshader::activeContext()->urand()). Variant selected by
            // dstDesc.stride exactly like the noise/snoise case below.
            // ================================================================
            else if (op == "random" || op == "urandom") {
                if (!dst)
                    continue;
                bool dstIsVec = (dstDesc.stride == 3);
                const char *fnName = dstIsVec ? "op_random_v" : "op_random_f";
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, numVerts, tags});
            }

            // ================================================================
            // Layer G — extra math / comparison opcodes
            // ================================================================
            else if (op == "max" || op == "maxf") {
                emitBin(ins, "op_maxf", dst, dstStride, dstStrideVal);
            }
            // min() (spec 017-jit-builtin-function-coverage, US4): float
            // 2-argument form only, matching max/maxf's own coverage level.
            else if (op == "min" || op == "minf") {
                emitBin(ins, "op_minf", dst, dstStride, dstStrideVal);
            }
            else if (op == "and" || op == "andf") {
                emitBin(ins, "op_andf", dst, dstStride, dstStrideVal);
            }
            else if (op == "or" || op == "orf") {
                emitBin(ins, "op_orf", dst, dstStride, dstStrideVal);
            }
            // Comparison aliases used by some shader compilers
            else if (op == "feql") {
                emitBin(ins, "op_feq", dst, dstStride, dstStrideVal);
            }
            else if (op == "felt") {
                emitBin(ins, "op_fle", dst, dstStride, dstStrideVal);
            }
            else if (op == "fneql") {
                emitBin(ins, "op_fne", dst, dstStride, dstStrideVal);
            }
            else if (op == "radians") {
                emitUn(ins, "op_radians", dst, dstStride, dstStrideVal);
            }
            else if (op == "degrees") {
                emitUn(ins, "op_degrees", dst, dstStride, dstStrideVal);
            }
            else if (op == "round") {
                emitUn(ins, "op_round", dst, dstStride, dstStrideVal);
            }
            else if (op == "determinant") {
                emitUn(ins, "op_determinant", dst, dstStride, dstStrideVal);
            }
            else if (op == "distance") {
                emitBin(ins, "op_distance", dst, dstStride, dstStrideVal);
            }
            else if (op == "filterstep") {
                emitBin(ins, "op_filterstep", dst, dstStride, dstStrideVal);
            }
            // step() (spec 017-jit-builtin-function-coverage, US4):
            // STEPEXP (scriptFunctions.h) is `*res = (*op2 < *op1 ? 0 : 1)`
            // -- i.e. `(x >= edge) ? 1 : 0` for the "f=ff" (edge, x)
            // argument order, which is EXACTLY op_filterstep's own formula
            // (rslOps.cpp) -- op_filterstep is itself a simplified,
            // non-antialiased JIT implementation of the interpreter's real
            // (derivative/filter-width-based) filterstep(), a pre-existing
            // simplification from an earlier spec, not something this task
            // changes. No comparison-sense flip is needed (verified by
            // reading both formulas directly, not assumed) -- step()
            // aliases directly to op_filterstep, same as match() aliased
            // to op_seql.
            else if (op == "step") {
                emitBin(ins, "op_filterstep", dst, dstStride, dstStrideVal);
            }

            // ================================================================
            // Layer G — reflect
            // ================================================================
            else if (op == "reflect") {
                auto [I, si] = getVar(ins, 0);
                auto [N, sn] = getVar(ins, 1);
                if (!dst || !I || !N)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_reflect", ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {si, sn});
                B.CreateCall(fn, {dst, dstStride,
                                  I, B.getInt32(si), N, B.getInt32(sn),
                                  n, tg});
            }

            // refract() (spec 017-jit-builtin-function-coverage, US4):
            // same shape as reflect, plus a 4th scalar eta operand.
            else if (op == "refract") {
                auto [I, si] = getVar(ins, 0);
                auto [N, sn] = getVar(ins, 1);
                auto [eta, se] = getVar(ins, 2);
                if (!dst || !I || !N || !eta)
                    continue;
                auto *ty = llvm::FunctionType::get(
                    voidTy, {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_refract", ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {si, sn, se});
                B.CreateCall(fn, {dst, dstStride,
                                  I, B.getInt32(si), N, B.getInt32(sn), eta, B.getInt32(se),
                                  n, tg});
            }

            // ptlined() (spec 017-jit-builtin-function-coverage, US3):
            // pure geometry, 3 point operands -> scalar dst, zero
            // CShadingContext state. A plain DEFFUNC, so collapseArgs is
            // appropriate (no numRealVertices discipline needed).
            else if (op == "ptlined") {
                if (ins.operands.size() < 3 || !dst)
                    continue;
                auto [pt, sPt] = getVar(ins, 0);
                auto [lineA, sA] = getVar(ins, 1);
                auto [lineB, sB] = getVar(ins, 2);
                if (!pt || !lineA || !lineB)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty,
                                                    i32Ty, ptrTy},
                                                   false);
                auto *fn = declareOp(mod, "op_ptlined", ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {sPt, sA, sB});
                B.CreateCall(fn, {dst, dstStride, pt, B.getInt32(sPt), lineA, B.getInt32(sA),
                                  lineB, B.getInt32(sB), n, tg});
            }

            // specularbrdf() (spec 017-jit-builtin-function-coverage,
            // US5): pure math, 4 operands (L, N, V, roughness) -> color.
            else if (op == "specularbrdf") {
                if (ins.operands.size() < 4 || !dst)
                    continue;
                auto [L, sL] = getVar(ins, 0);
                auto [N, sN] = getVar(ins, 1);
                auto [V, sV] = getVar(ins, 2);
                auto [R, sR] = getVar(ins, 3);
                if (!L || !N || !V || !R)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty,
                                                    ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy},
                                                   false);
                auto *fn = declareOp(mod, "op_specularbrdf", ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {sL, sN, sV, sR});
                B.CreateCall(fn, {dst, dstStride, L, B.getInt32(sL), N, B.getInt32(sN),
                                  V, B.getInt32(sV), R, B.getInt32(sR), n, tg});
            }

            // ================================================================
            // Layer G — fresnel (outputs Kr, Kt and optionally R, T)
            // IR: fresnel I N eta Kr Kt [R T]
            // ================================================================
            else if (op == "fresnel") {
                auto [I, si] = getVar(ins, 0);
                auto [N, sn] = getVar(ins, 1);
                auto [eta, se] = getVar(ins, 2);
                auto [Kr, skr] = getVar(ins, 3);
                auto [Kt, skt] = getVar(ins, 4);
                llvm::Value *R = nullptr;
                int sr = 0;
                llvm::Value *T = nullptr;
                int st = 0;
                if (ins.operands.size() > 5) {
                    auto [rv, rs] = getVar(ins, 5);
                    R = rv;
                    sr = rs;
                }
                if (ins.operands.size() > 6) {
                    auto [tv, ts] = getVar(ins, 6);
                    T = tv;
                    st = ts;
                }
                if (!I || !N || !eta || !Kr || !Kt)
                    continue;
                // Null-out optional pointers when not present
                llvm::Value *nullPtr = llvm::ConstantPointerNull::get(
                    llvm::PointerType::getUnqual(mod.getContext()));
                if (!R) {
                    R = nullPtr;
                    sr = 0;
                }
                if (!T) {
                    T = nullPtr;
                    st = 0;
                }
                // (I,si, N,sn, eta,se, Kr,skr, Kt,skt, R,sr, T,st, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty,
                                                    ptrTy, i32Ty, ptrTy, i32Ty,
                                                    ptrTy, i32Ty, ptrTy, i32Ty,
                                                    i32Ty, ptrTy},
                                                   false);
                auto *fn = declareOp(mod, "op_fresnel", ty);
                auto [n, tg] = collapseArgs(0, {si, sn, se, skr, skt, sr, st});
                B.CreateCall(fn, {I, B.getInt32(si),
                                  N, B.getInt32(sn),
                                  eta, B.getInt32(se),
                                  Kr, B.getInt32(skr),
                                  Kt, B.getInt32(skt),
                                  R, B.getInt32(sr),
                                  T, B.getInt32(st),
                                  n, tg});
            }

            // ================================================================
            // Layer G — noise: select variant by operand stride
            // noise(f→f): op_noise_ff, noise(p→f): op_noise_fp
            // noise(f→v): op_noise_vf, noise(p→v): op_noise_vp
            // ================================================================
            else if (op == "noise" || op == "snoise") {
                auto [x, sx] = getVar(ins, 0);
                if (!dst || !x)
                    continue;
                bool dstIsVec = (dstDesc.stride == 3);
                bool srcIsVec = (sx == 3);
                const char *fnName = dstIsVec
                                         ? (srcIsVec ? "op_noise_vp" : "op_noise_vf")
                                         : (srcIsVec ? "op_noise_fp" : "op_noise_ff");
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, fnName, ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {sx});
                B.CreateCall(fn, {dst, dstStride, x, B.getInt32(sx), n, tg});
            }

            // ================================================================
            // Layer G — cellnoise: single- and two-source variants
            // (f→f/p): op_cellnoise_ff/fp   (f→ff/pf): op_cellnoise_fff/fpf
            // (v→f/p): op_cellnoise_vf/vp   (v→ff/pf): op_cellnoise_vff/vpf
            // ================================================================
            else if (op == "cellnoise") {
                auto [x, sx] = getVar(ins, 0);
                if (!dst || !x)
                    continue;
                bool dstIsVec = (dstDesc.stride == 3);
                bool srcIsVec = (sx == 3);
                if (ins.operands.size() >= 2) {
                    auto [y, sy] = getVar(ins, 1);
                    if (!y)
                        continue;
                    const char *fnName = dstIsVec
                                             ? (srcIsVec ? "op_cellnoise_vpf" : "op_cellnoise_vff")
                                             : (srcIsVec ? "op_cellnoise_fpf" : "op_cellnoise_fff");
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, fnName, ty);
                    auto [n, tg] = collapseArgs(dstStrideVal, {sx, sy});
                    B.CreateCall(fn, {dst, dstStride,
                                      x, B.getInt32(sx), y, B.getInt32(sy),
                                      n, tg});
                }
                else {
                    const char *fnName = dstIsVec
                                             ? (srcIsVec ? "op_cellnoise_vp" : "op_cellnoise_vf")
                                             : (srcIsVec ? "op_cellnoise_fp" : "op_cellnoise_ff");
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, fnName, ty);
                    auto [n, tg] = collapseArgs(dstStrideVal, {sx});
                    B.CreateCall(fn, {dst, dstStride, x, B.getInt32(sx), n, tg});
                }
            }

            // ================================================================
            // Layer G — pnoise (spec 017-jit-builtin-function-coverage,
            // US5): periodic noise, 4 argument shapes (1D/2D/3D/4D)
            // disambiguated by operand count + operand[0]'s stride
            // (point-shaped = 3D/4D, float-shaped = 1D/2D), crossed with
            // dst stride (float vs vector result) -- same technique as
            // "noise"/"snoise" above, extended to 4 shapes instead of 2.
            // ================================================================
            else if (op == "pnoise") {
                if (ins.operands.empty() || !dst)
                    continue;
                bool dstIsVec = (dstDesc.stride == 3);
                auto [a0, sa0] = getVar(ins, 0);
                if (!a0)
                    continue;
                bool arg0IsPoint = (sa0 == 3);

                if (ins.operands.size() == 2) {
                    auto [a1, sa1] = getVar(ins, 1);
                    if (!a1)
                        continue;
                    const char *fnName = arg0IsPoint
                                             ? (dstIsVec ? "op_pnoise_3d_v" : "op_pnoise_3d_f")
                                             : (dstIsVec ? "op_pnoise_1d_v" : "op_pnoise_1d_f");
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy},
                                                       false);
                    auto *fn = declareOp(mod, fnName, ty);
                    auto [n, tg] = collapseArgs(dstStrideVal, {sa0, sa1});
                    B.CreateCall(fn, {dst, dstStride, a0, B.getInt32(sa0), a1, B.getInt32(sa1), n, tg});
                }
                else if (ins.operands.size() == 4) {
                    auto [a1, sa1] = getVar(ins, 1);
                    auto [a2, sa2] = getVar(ins, 2);
                    auto [a3, sa3] = getVar(ins, 3);
                    if (!a1 || !a2 || !a3)
                        continue;
                    const char *fnName = arg0IsPoint
                                             ? (dstIsVec ? "op_pnoise_4d_v" : "op_pnoise_4d_f")
                                             : (dstIsVec ? "op_pnoise_2d_v" : "op_pnoise_2d_f");
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty,
                                                        ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy},
                                                       false);
                    auto *fn = declareOp(mod, fnName, ty);
                    auto [n, tg] = collapseArgs(dstStrideVal, {sa0, sa1, sa2, sa3});
                    B.CreateCall(fn, {dst, dstStride, a0, B.getInt32(sa0), a1, B.getInt32(sa1),
                                      a2, B.getInt32(sa2), a3, B.getInt32(sa3), n, tg});
                }
            }

            // ================================================================
            // Layer G — string equality
            // seql / sneql: operands are char** (string locals), not float*.
            // ================================================================
            // match() (spec 017-jit-builtin-function-coverage, US4):
            // the interpreter's MATCHEXPR (scriptFunctions.h) is plain
            // strcmp equality today (documented FIXME: "Subpattern
            // matching is not implemented yet") -- functionally identical
            // to seql, so it aliases directly to op_seql rather than
            // getting its own op_* function. Do NOT implement real
            // regex/subpattern matching here (FR-017) -- mirror the
            // interpreter's current behavior exactly.
            else if (op == "seql" || op == "sneql" || op == "match") {
                if (ins.operands.size() < 2 || !dst)
                    continue;
                const std::string &tokA = ins.operands[0].token;
                const std::string &tokB = ins.operands[1].token;

                // Look up variables; they resolve to char** locals. Track each
                // operand's stride (0=uniform, 1=varying) so op_seql/op_sneql
                // can advance per-vertex instead of always reading slot 0 —
                // needed when an operand is a varying string (e.g. the result
                // of a usfroma extraction), not just a uniform.
                VarDesc descA, descB;
                llvm::Value *ptrA = nullptr, *ptrB = nullptr;
                int strideA = 0, strideB = 0;
                if (resolveVar(tokA, descA)) {
                    ptrA = loadVarPtr(descA);
                    strideA = descA.stride;
                }
                if (resolveVar(tokB, descB)) {
                    ptrB = loadVarPtr(descB);
                    strideB = descB.stride;
                }

                // If not found, embed as a string literal constant (uniform).
                auto makeStrLit = [&](const std::string &tok) -> llvm::Value * {
                    std::string s = tok;
                    if (s.size() >= 2 && s.front() == '"')
                        s = s.substr(1, s.size() - 2);
                    // Store char* in a local alloca so we have a char**.
                    llvm::Value *sptr = B.CreateGlobalString(s, "strlit");
                    llvm::Value *alloca = B.CreateAlloca(ptrTy, nullptr, "strlit_pp");
                    B.CreateStore(sptr, alloca);
                    return alloca;
                };
                if (!ptrA) {
                    ptrA = makeStrLit(tokA);
                    strideA = 0;
                }
                if (!ptrB) {
                    ptrB = makeStrLit(tokB);
                    strideB = 0;
                }

                const char *fnName = (op == "sneql") ? "op_sneql" : "op_seql"; // seql and match both alias op_seql
                // (float* dst, int sd, const char* const* a, int sa, const char* const* b, int sb, int n, const int* tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, fnName, ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {strideA, strideB});
                B.CreateCall(fn, {dst, dstStride, ptrA, B.getInt32(strideA), ptrB, B.getInt32(strideB), n, tg});
            }

            // concat() (spec 017-jit-builtin-function-coverage, US4):
            // N-ary string concatenation, "s=ss*" -- dst is a SEPARATE
            // result variable (confirmed by dumping a throwaway probe's
            // compiled .rslo: `concat ("s=ssss") c a "-" b "!"`, unlike
            // setcomp's dst-is-the-mutated-operand shape), and every
            // operand is a string (variable or literal). Builds two
            // parallel runtime stack arrays (operand char** pointers,
            // strides) the same way "spline" builds its knot-pointer
            // array, but ALSO resolves each operand's stride via
            // resolveVar/loadVarPtr with a string-literal fallback,
            // mirroring "seql"/"sneql"/"match" above -- unlike spline,
            // which ignores stride entirely (uniform-only knots).
            else if (op == "concat") {
                if (!dst || ins.operands.size() < 1)
                    continue;
                int numOperands = (int)ins.operands.size();
                auto *ptrArrTy = llvm::ArrayType::get(ptrTy, numOperands);
                auto *strideArrTy = llvm::ArrayType::get(i32Ty, numOperands);
                auto *ptrArr = B.CreateAlloca(ptrArrTy, nullptr, "concat_ops");
                auto *strideArr = B.CreateAlloca(strideArrTy, nullptr, "concat_strides");

                auto makeStrLit = [&](const std::string &tok) -> llvm::Value * {
                    std::string s = tok;
                    if (s.size() >= 2 && s.front() == '"')
                        s = s.substr(1, s.size() - 2);
                    llvm::Value *sptr = B.CreateGlobalString(s, "strlit");
                    llvm::Value *alloca = B.CreateAlloca(ptrTy, nullptr, "strlit_pp");
                    B.CreateStore(sptr, alloca);
                    return alloca;
                };

                // Resolve every operand once; track strides locally so the
                // uniform-vs-varying check below (collapseArgs's own logic,
                // inlined since collapseArgs takes a fixed initializer_list
                // and these strides are only known at this runtime-array
                // build time) doesn't need a second resolveVar pass.
                bool allUniform = (dstStrideVal == 0);
                for (int k = 0; k < numOperands; ++k) {
                    const std::string &tok = ins.operands[k].token;
                    VarDesc desc;
                    llvm::Value *ptr = nullptr;
                    int stride = 0;
                    if (resolveVar(tok, desc)) {
                        ptr = loadVarPtr(desc);
                        stride = desc.stride;
                    }
                    if (!ptr) {
                        ptr = makeStrLit(tok);
                        stride = 0;
                    }
                    allUniform = allUniform && (stride == 0);
                    auto *pGep = B.CreateGEP(ptrArrTy, ptrArr, {B.getInt32(0), B.getInt32(k)});
                    B.CreateStore(ptr, pGep);
                    auto *sGep = B.CreateGEP(strideArrTy, strideArr, {B.getInt32(0), B.getInt32(k)});
                    B.CreateStore(B.getInt32(stride), sGep);
                }

                // (dst, sd, operands**, strides*, numOperands, n, tags)
                auto *ty = llvm::FunctionType::get(
                    voidTy, {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_concat", ty);
                llvm::Value *n = allUniform ? B.getInt32(1) : numVerts;
                llvm::Value *tg = allUniform ? llvm::ConstantPointerNull::get(ptrTy) : tags;
                B.CreateCall(fn, {dst, dstStride, ptrArr, strideArr,
                                  B.getInt32(numOperands), n, tg});
            }

            // format() (spec 017-jit-builtin-function-coverage, US4):
            // "s=s.*" -- operand 0 is the format string, operands 1..N
            // are the trailing values to substitute. Confirmed via a
            // throwaway mixed-type probe's compiled .rslo (`format
            // ("s=sfvs") result "%f %v %s" f v_1 s_1`) that each trailing
            // operand is resolved with its OWN real RSL type (unlike the
            // interpreter's dual float*/char** resolution of the same
            // slot) -- so plain getVar() per operand already gives the
            // correctly-typed, correctly-strided pointer; jitFormat
            // reinterprets it based on the specifier character actually
            // encountered, mirroring what the interpreter's dual
            // resolution achieves.
            else if (op == "format") {
                if (!dst || ins.operands.size() < 1)
                    continue;
                auto [fmt, sf] = getVar(ins, 0);
                if (!fmt)
                    continue;
                int numOperands = (int)ins.operands.size() - 1;
                int arrLen = numOperands > 0 ? numOperands : 1;
                auto *ptrArrTy = llvm::ArrayType::get(ptrTy, arrLen);
                auto *strideArrTy = llvm::ArrayType::get(i32Ty, arrLen);
                auto *ptrArr = B.CreateAlloca(ptrArrTy, nullptr, "format_ops");
                auto *strideArr = B.CreateAlloca(strideArrTy, nullptr, "format_strides");

                bool allUniform = (dstStrideVal == 0) && (sf == 0);
                bool ok = true;
                for (int k = 0; k < numOperands; ++k) {
                    auto [p, s] = getVar(ins, k + 1);
                    if (!p) {
                        ok = false;
                        break;
                    }
                    allUniform = allUniform && (s == 0);
                    auto *pGep = B.CreateGEP(ptrArrTy, ptrArr, {B.getInt32(0), B.getInt32(k)});
                    B.CreateStore(p, pGep);
                    auto *sGep = B.CreateGEP(strideArrTy, strideArr, {B.getInt32(0), B.getInt32(k)});
                    B.CreateStore(B.getInt32(s), sGep);
                }
                if (ok) {
                    // (dst, sd, fmt, sf, operands**, strides*, numOperands, n, tags)
                    auto *ty = llvm::FunctionType::get(
                        voidTy,
                        {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_format", ty);
                    llvm::Value *n = allUniform ? B.getInt32(1) : numVerts;
                    llvm::Value *tg = allUniform ? llvm::ConstantPointerNull::get(ptrTy) : tags;
                    B.CreateCall(fn, {dst, dstStride, fmt, B.getInt32(sf), ptrArr, strideArr,
                                      B.getInt32(numOperands), n, tg});
                }
            }

            // ================================================================
            // Layer G — derivatives (Du / Dv)
            // ================================================================
            else if (op == "Du" || op == "Dv") {
                auto [src, ss] = getVar(ins, 0);
                if (!dst || !src)
                    continue;
                bool isVec = (ss == 3);
                const char *fnName = (op == "Du")
                                         ? (isVec ? "op_Du_vv" : "op_Du_ff")
                                         : (isVec ? "op_Dv_vv" : "op_Dv_ff");
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, fnName, ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {ss});
                B.CreateCall(fn, {dst, dstStride, src, B.getInt32(ss), n, tg});
            }

            // ================================================================
            // Layer G — geometric built-ins: area, calculatenormal, depth
            // ================================================================
            else if (op == "area") {
                auto [P, sp] = getVar(ins, 0);
                if (!dst || !P)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_area", ty);
                B.CreateCall(fn, {dst, dstStride, P, B.getInt32(sp), numVerts, tags});
            }
            else if (op == "calculatenormal") {
                auto [P, sp] = getVar(ins, 0);
                if (!dst || !P)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_calculatenormal", ty);
                B.CreateCall(fn, {dst, dstStride, P, B.getInt32(sp), numVerts, tags});
            }
            else if (op == "depth") {
                auto [P, sp] = getVar(ins, 0);
                if (!dst || !P)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
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
                if (ins.operands.size() < 3 || !dst)
                    continue;
                const std::string &nameTok = ins.operands[0].token;

                // Resolve name to a char** locals slot (or embed a string literal).
                llvm::Value *namePP = nullptr;
                VarDesc nameDesc;
                if (resolveVar(nameTok, nameDesc)) {
                    namePP = loadVarPtr(nameDesc);
                }
                else {
                    // String literal operand
                    std::string s = nameTok;
                    if (s.size() >= 2 && s.front() == '"')
                        s = s.substr(1, s.size() - 2);
                    llvm::Value *sptr = B.CreateGlobalString(s, "texname");
                    llvm::Value *alloca = B.CreateAlloca(ptrTy, nullptr, "texname_pp");
                    B.CreateStore(sptr, alloca);
                    namePP = alloca;
                }

                if (ins.operands.size() >= 4) {
                    // Float-channel form: texture name channel s t
                    auto [chan, sc] = getVar(ins, 1);
                    auto [s, ss] = getVar(ins, 2);
                    auto [t, st] = getVar(ins, 3);
                    if (!chan || !s || !t)
                        continue;
                    // (dst,sd, namepp, chan, s,ss, t,st, n, tags)
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, ptrTy, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_texture_f", ty);
                    auto [n, tg] = collapseArgs(dstStrideVal, {ss, st});
                    B.CreateCall(fn, {dst, dstStride, namePP, chan,
                                      s, B.getInt32(ss), t, B.getInt32(st), n, tg});
                }
                else {
                    // Color form: texture name s t
                    auto [s, ss] = getVar(ins, 1);
                    auto [t, st] = getVar(ins, 2);
                    if (!s || !t)
                        continue;
                    // (dst,sd, namepp, s,ss, t,st, n, tags)
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_texture_c", ty);
                    auto [n, tg] = collapseArgs(dstStrideVal, {ss, st});
                    B.CreateCall(fn, {dst, dstStride, namePP,
                                      s, B.getInt32(ss), t, B.getInt32(st), n, tg});
                }
            }

            // ================================================================
            // Layer G — environment / shadow lookups
            // ================================================================
            else if (op == "environment") {
                if (ins.operands.size() < 2 || !dst)
                    continue;
                const std::string &nameTok = ins.operands[0].token;
                llvm::Value *namePP = nullptr;
                VarDesc nameDesc;
                if (resolveVar(nameTok, nameDesc)) {
                    namePP = loadVarPtr(nameDesc);
                }
                else {
                    std::string s = nameTok;
                    if (s.size() >= 2 && s.front() == '"')
                        s = s.substr(1, s.size() - 2);
                    llvm::Value *sptr = B.CreateGlobalString(s, "envname");
                    llvm::Value *alloca = B.CreateAlloca(ptrTy, nullptr, "envname_pp");
                    B.CreateStore(sptr, alloca);
                    namePP = alloca;
                }

                if (ins.operands.size() >= 3) {
                    // Float-channel form: environment name channel D
                    auto [chan, sc] = getVar(ins, 1);
                    auto [D, sD] = getVar(ins, 2);
                    if (!chan || !D)
                        continue;
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_environment_f", ty);
                    auto [n, tg] = collapseArgs(dstStrideVal, {sD});
                    B.CreateCall(fn, {dst, dstStride, namePP, chan,
                                      D, B.getInt32(sD), n, tg});
                }
                else {
                    // Color form: environment name D
                    auto [D, sD] = getVar(ins, 1);
                    if (!D)
                        continue;
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_environment_c", ty);
                    auto [n, tg] = collapseArgs(dstStrideVal, {sD});
                    B.CreateCall(fn, {dst, dstStride, namePP,
                                      D, B.getInt32(sD), n, tg});
                }
            }
            else if (op == "shadow") {
                if (ins.operands.size() < 2 || !dst)
                    continue;
                const std::string &nameTok = ins.operands[0].token;
                llvm::Value *namePP = nullptr;
                VarDesc nameDesc;
                if (resolveVar(nameTok, nameDesc)) {
                    namePP = loadVarPtr(nameDesc);
                }
                else {
                    std::string s = nameTok;
                    if (s.size() >= 2 && s.front() == '"')
                        s = s.substr(1, s.size() - 2);
                    llvm::Value *sptr = B.CreateGlobalString(s, "shadowname");
                    llvm::Value *alloca = B.CreateAlloca(ptrTy, nullptr, "shadowname_pp");
                    B.CreateStore(sptr, alloca);
                    namePP = alloca;
                }
                auto [Ps, sPs] = getVar(ins, 1);
                if (!Ps)
                    continue;
                // (dst,sd, namepp, Ps,sPs, n, tags)
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_shadow_f", ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {sPs});
                B.CreateCall(fn, {dst, dstStride, namePP,
                                  Ps, B.getInt32(sPs), n, tg});
            }

            // ================================================================
            // visibility()/transmission()/trace() (spec 017-jit-builtin-function-
            // coverage, US1) -- the first JIT-emitted call to construct and
            // consume a real raytraced batch (research.md D2). du/dv/N/time are
            // resolved via the already-generic global-variable table (D2) --
            // no new plumbing. Deliberately NOT run through collapseArgs: these
            // are per-real-vertex stochastic operations (numRealVertices-bounded
            // inside op_visibility/etc. itself, D1), so passing the raw numVerts
            // (not a uniform-collapsed n) is required, matching ambient/diffuse/
            // specular's precedent rather than shadow's deterministic-lookup one.
            // ================================================================
            else if (op == "visibility" || op == "transmission" || op == "trace") {
                if (ins.operands.size() < 2 || !dst)
                    continue;
                auto [P, sP] = getVar(ins, 0);
                auto [D, sD] = getVar(ins, 1);
                if (!P || !D)
                    continue;
                VarDesc duDesc, dvDesc, nDesc, timeDesc;
                if (!resolveVar("du", duDesc) || !resolveVar("dv", dvDesc) ||
                    !resolveVar("N", nDesc) || !resolveVar("time", timeDesc))
                    continue;
                llvm::Value *duPtr = loadVarPtr(duDesc);
                llvm::Value *dvPtr = loadVarPtr(dvDesc);
                llvm::Value *nPtr = loadVarPtr(nDesc);
                llvm::Value *timePtr = loadVarPtr(timeDesc);
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty,
                                                    ptrTy, ptrTy, ptrTy, ptrTy, i32Ty, ptrTy},
                                                   false);
                const char *fnName;
                if (op == "visibility")
                    fnName = "op_visibility";
                else if (op == "transmission")
                    fnName = "op_transmission";
                else
                    fnName = (dstStrideVal == 3) ? "op_trace_c" : "op_trace_f";
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, P, B.getInt32(sP), D, B.getInt32(sD),
                                  duPtr, dvPtr, nPtr, timePtr, numVerts, tags});
            }

            // occlusion()/indirectdiffuse() (spec 017-jit-builtin-function-
            // coverage, US1): point-cloud/irradiance-cache lookup, not a
            // ray batch -- same "no collapseArgs" reasoning as visibility/
            // transmission/trace though (D1's stochastic hemisphere sampling).
            else if (op == "occlusion" || op == "indirectdiffuse") {
                if (ins.operands.size() < 3 || !dst)
                    continue;
                auto [P, sP] = getVar(ins, 0);
                auto [N, sN] = getVar(ins, 1);
                auto [samples, sSamples] = getVar(ins, 2);
                if (!P || !N || !samples)
                    continue;
                VarDesc duDesc, dvDesc;
                if (!resolveVar("du", duDesc) || !resolveVar("dv", dvDesc))
                    continue;
                llvm::Value *duPtr = loadVarPtr(duDesc);
                llvm::Value *dvPtr = loadVarPtr(dvDesc);
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty,
                                                    ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, ptrTy},
                                                   false);
                const char *fnName = (op == "occlusion") ? "op_occlusion" : "op_indirectdiffuse";
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, P, B.getInt32(sP), N, B.getInt32(sN),
                                  samples, B.getInt32(sSamples), duPtr, dvPtr, numVerts, tags});
            }

            // texture3d()/bake3d() (spec 017-jit-builtin-function-coverage,
            // US5): point-cloud read/write, same "no collapseArgs"
            // dispatch shape as occlusion/indirectdiffuse just above (only
            // the base positional arguments are supported -- see shading.h
            // for the full scoping rationale).
            else if (op == "texture3d") {
                if (ins.operands.size() < 3 || !dst)
                    continue;
                auto [name, sName] = getVar(ins, 0);
                auto [P, sP] = getVar(ins, 1);
                auto [N, sN] = getVar(ins, 2);
                if (!name || !P || !N)
                    continue;
                VarDesc duDesc, dvDesc;
                if (!resolveVar("du", duDesc) || !resolveVar("dv", dvDesc))
                    continue;
                llvm::Value *duPtr = loadVarPtr(duDesc);
                llvm::Value *dvPtr = loadVarPtr(dvDesc);
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, ptrTy, i32Ty,
                                                    ptrTy, ptrTy, i32Ty, ptrTy},
                                                   false);
                auto *fn = declareOp(mod, "op_texture3d", ty);
                B.CreateCall(fn, {dst, dstStride, name, P, B.getInt32(sP), N, B.getInt32(sN),
                                  duPtr, dvPtr, numVerts, tags});
            }
            else if (op == "bake3d") {
                if (ins.operands.size() < 4 || !dst)
                    continue;
                auto [name, sName] = getVar(ins, 0);
                auto [channels, sChannels] = getVar(ins, 1);
                auto [P, sP] = getVar(ins, 2);
                auto [N, sN] = getVar(ins, 3);
                if (!name || !channels || !P || !N)
                    continue;
                VarDesc duDesc, dvDesc;
                if (!resolveVar("du", duDesc) || !resolveVar("dv", dvDesc))
                    continue;
                llvm::Value *duPtr = loadVarPtr(duDesc);
                llvm::Value *dvPtr = loadVarPtr(dvDesc);
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, ptrTy, ptrTy, i32Ty, ptrTy, i32Ty,
                                                    ptrTy, ptrTy, i32Ty, ptrTy},
                                                   false);
                auto *fn = declareOp(mod, "op_bake3d", ty);
                B.CreateCall(fn, {dst, dstStride, name, channels, P, B.getInt32(sP), N, B.getInt32(sN),
                                  duPtr, dvPtr, numVerts, tags});
            }

            // surface()/displacement()/atmosphere()/incident()/opposite()/
            // attribute()/option()/rendererinfo() (spec 017-jit-builtin-
            // function-coverage, US5): named-parameter query against a bound
            // shader instance or scene-level table. All 4 result-type
            // overloads (float/vector/string/matrix) share the same "surface"
            // (etc.) mnemonic -- disambiguated via the prototype string's
            // LAST character ("f=SF"/"f=SV"/"f=SS"/"f=SM"), the same
            // ins.proto convention already used for "clamp"/"mix"'s
            // float-vs-vector dispatch above (there via proto[0], the return
            // type; here via proto.back(), the dest-operand type -- the
            // return itself is always 'f', the found/not-found indicator).
            else if (op == "surface" || op == "displacement" || op == "atmosphere" ||
                     op == "incident" || op == "opposite" ||
                     op == "attribute" || op == "option" || op == "rendererinfo") {
                if (ins.operands.size() < 2 || !dst)
                    continue;
                auto [namePtr, sNameUnused] = getVar(ins, 0);
                auto [destPtr, sDest] = getVar(ins, 1);
                if (!namePtr || !destPtr)
                    continue;
                const char resultChar = ins.proto.empty() ? 'F' : ins.proto.back();
                const int resultKind = (resultChar == 'V') ? 3 : (resultChar == 'M') ? 16
                                                              : (resultChar == 'S')   ? -1
                                                                                      : 1;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy, i32Ty},
                                                   false);
                const char *fnName;
                if (op == "surface")
                    fnName = "op_surface_param";
                else if (op == "displacement")
                    fnName = "op_displacement_param";
                else if (op == "atmosphere")
                    fnName = "op_atmosphere_param";
                else if (op == "incident")
                    fnName = "op_incident_param";
                else if (op == "opposite")
                    fnName = "op_opposite_param";
                else if (op == "attribute")
                    fnName = "op_attribute_param";
                else if (op == "option")
                    fnName = "op_option_param";
                else
                    fnName = "op_rendererinfo_param";
                auto *fn = declareOp(mod, fnName, ty);
                B.CreateCall(fn, {dst, dstStride, namePtr, destPtr, B.getInt32(sDest),
                                  numVerts, tags, B.getInt32(resultKind)});
            }

            // textureinfo() (spec 017-jit-builtin-function-coverage,
            // US5): the prototype string carries a trailing "!" (e.g.
            // "f=SSF!"), unlike surface()/etc.'s clean "f=SF" forms above
            // -- the result-type char is the SECOND-TO-LAST character,
            // not the last.
            else if (op == "textureinfo") {
                if (ins.operands.size() < 3 || !dst)
                    continue;
                auto [namePtr, sNameUnused] = getVar(ins, 0);
                auto [queryPtr, sQueryUnused] = getVar(ins, 1);
                auto [destPtr, sDest] = getVar(ins, 2);
                if (!namePtr || !queryPtr || !destPtr)
                    continue;
                char resultChar = 'F';
                if (ins.proto.size() >= 2)
                    resultChar = ins.proto[ins.proto.size() - 2];
                const int resultKind = (resultChar == 'V') ? 3 : (resultChar == 'M') ? 16
                                                              : (resultChar == 'S')   ? -1
                                                                                      : 1;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy, i32Ty},
                                                   false);
                auto *fn = declareOp(mod, "op_textureinfo", ty);
                B.CreateCall(fn, {dst, dstStride, namePtr, queryPtr, destPtr, B.getInt32(sDest),
                                  numVerts, tags, B.getInt32(resultKind)});
            }

            // Deriv() (spec 017-jit-builtin-function-coverage, US5): 2
            // overloads ("f=ff"/"v=vf") disambiguated by dst stride, same
            // as most other float-vs-vector dispatches above. A plain
            // DEFFUNC -- loops the full numVerts, no numRealVertices
            // replication needed.
            else if (op == "Deriv") {
                if (ins.operands.size() < 2 || !dst)
                    continue;
                auto [num, sNum] = getVar(ins, 0);
                auto [denom, sDenom] = getVar(ins, 1);
                if (!num || !denom)
                    continue;
                bool dstIsVec = (dstDesc.stride == 3);
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy},
                                                   false);
                auto *fn = declareOp(mod, dstIsVec ? "op_deriv_v" : "op_deriv_f", ty);
                B.CreateCall(fn, {dst, dstStride, num, B.getInt32(sNum), denom, B.getInt32(sDenom),
                                  numVerts, tags});
            }

            // shadername() (spec 017-jit-builtin-function-coverage, US5):
            // two overloads distinguished by ARITY (0 vs 1 operands), not
            // result type -- both always return a string. A plain DEFFUNC
            // (shaderFunctions.h), so no numRealVertices-replicate
            // discipline is needed -- loops the full numVerts like any
            // other already-handled simple opcode.
            else if (op == "shadername") {
                if (!dst)
                    continue;
                if (ins.operands.empty()) {
                    auto *ty = llvm::FunctionType::get(voidTy, {ptrTy, i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_shadername", ty);
                    B.CreateCall(fn, {dst, dstStride, numVerts, tags});
                }
                else {
                    auto [typePtr, sType] = getVar(ins, 0);
                    if (!typePtr)
                        continue;
                    auto *ty = llvm::FunctionType::get(voidTy,
                                                       {ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_shadername_s", ty);
                    B.CreateCall(fn, {dst, dstStride, typePtr, B.getInt32(sType), numVerts, tags});
                }
            }

            // clearlighting() (spec 017-jit-builtin-function-coverage,
            // US5): pure side-effect call, no operands, no dst, no result.
            else if (op == "clearlighting") {
                auto *ty = llvm::FunctionType::get(voidTy, {}, false);
                auto *fn = declareOp(mod, "op_clearlighting", ty);
                B.CreateCall(fn, {});
            }

            // debug() (spec 017-jit-builtin-function-coverage, US5): both
            // overloads (float/vector) are a true no-op on shading state
            // (matches debugFunction()'s own body, which only writes to
            // stderr) -- one dispatch branch handles both, no operand
            // resolution needed. Currently uncallable from any RSL shader
            // (GitHub issue #5: "debug" has zero addBuiltInFunction
            // registrations in rslo.cpp), implemented ahead of that fix.
            else if (op == "debug") {
                auto *ty = llvm::FunctionType::get(voidTy, {i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_debug", ty);
                B.CreateCall(fn, {numVerts, tags});
            }

            // ================================================================
            // Layer G — vfrom aliases used by some shader variants
            // vtransform alias 'vfrom' (point transform); 'ntransform' alias 'nfrom'
            // ================================================================
            else if (op == "vfrom" || op == "nfrom") {
                if (ins.operands.size() < 2)
                    continue;
                const std::string &spaceToken = ins.operands[0].token;
                auto [src, ss] = getVar(ins, 1);
                if (!dst || !src)
                    continue;
                std::string spaceName = spaceToken;
                if (spaceName.size() >= 2 && spaceName.front() == '"')
                    spaceName = spaceName.substr(1, spaceName.size() - 2);
                llvm::Value *spacePtr = B.CreateGlobalString(spaceName, "space_str");
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                const char *fnName = (op == "nfrom") ? "op_ntransform" : "op_vtransform";
                auto *fn = declareOp(mod, fnName, ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {ss});
                B.CreateCall(fn, {dst, dstStride, spacePtr, src, B.getInt32(ss), n, tg});
            }

            // ================================================================
            // lightsource — query current light's attribute (inside illuminance)
            // lightsource("f=SF") result attrName outParam
            // ================================================================
            else if (op == "lightsource") {
                if (ins.operands.size() < 2 || !dst) { /* no-op */
                }
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

            // rayinfo()/raylabel()/raydepth() (spec 017-jit-builtin-
            // function-coverage, US3). rayinfo's destination TYPE
            // (string vs. numeric) is looked up separately via
            // resolveVar/VarDesc::isString -- its "f=s." prototype's
            // wildcard gives no static hint the way other functions'
            // per-type DEFFUNC overloads do (VarDesc.stride alone can't
            // distinguish a string local from a plain float, since both
            // collapse to the same "1 item" stride).
            else if (op == "rayinfo") {
                if (ins.operands.size() < 2 || !dst)
                    continue;
                auto [queryPtr, sQuery] = getVar(ins, 0);
                auto [destPtr, sDest] = getVar(ins, 1);
                if (!queryPtr || !destPtr)
                    continue;
                bool isStringDest = false;
                VarDesc destDesc;
                if (resolveVar(ins.operands[1].token, destDesc))
                    isStringDest = destDesc.isString;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy, i32Ty},
                                                   false);
                auto *fn = declareOp(mod, "op_rayinfo", ty);
                B.CreateCall(fn, {dst, dstStride, queryPtr, B.getInt32(sQuery), destPtr, B.getInt32(sDest),
                                  numVerts, tags, B.getInt32(isStringDest ? 1 : 0)});
            }
            else if (op == "raylabel") {
                if (!dst)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy, {ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_raylabel", ty);
                B.CreateCall(fn, {dst, dstStride, numVerts, tags});
            }
            else if (op == "raydepth") {
                if (!dst)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy, {ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_raydepth", ty);
                B.CreateCall(fn, {dst, dstStride, numVerts, tags});
            }

            // photonmap() (spec 017-jit-builtin-function-coverage, US3):
            // both overloads (2 or 3 operands) share op_photonmap -- the
            // 3rd (N) operand is unused even by the interpreter's own
            // macro, so it's simply never read here. Same "no
            // collapseArgs" dispatch shape as occlusion/indirectdiffuse
            // above (D1's numRealVertices-bound-then-replicate discipline
            // is applied inside jitPhotonMap itself, driven by the raw
            // numVerts passed through).
            else if (op == "photonmap") {
                if (ins.operands.size() < 2 || !dst)
                    continue;
                auto [namePtr, sNameUnused] = getVar(ins, 0);
                auto [P, sP] = getVar(ins, 1);
                if (!namePtr || !P)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_photonmap", ty);
                B.CreateCall(fn, {dst, dstStride, namePtr, P, B.getInt32(sP), numVerts, tags});
            }

            // ================================================================
            // spline — Catmull-Rom spline interpolation
            // spline("c=fccc...") dst t knot0 knot1 ... knotN-1
            // ================================================================
            else if (op == "spline") {
                if (!dst || ins.operands.size() < 3) { /* no-op */
                }
                else {
                    auto [t, st] = getVar(ins, 0);
                    if (!t) { /* no-op */
                    }
                    else {
                        int numKnots = (int)ins.operands.size() - 1;
                        // Allocate stack array of float* pointers
                        auto *arrTy = llvm::ArrayType::get(ptrTy, numKnots);
                        auto *arr = B.CreateAlloca(arrTy, nullptr, "spline_knots");
                        for (int k = 0; k < numKnots; ++k) {
                            auto [kp, ks] = getVar(ins, k + 1);
                            if (!kp) {
                                arr = nullptr;
                                break;
                            }
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
                                                                i32Ty, ptrTy, i32Ty, ptrTy},
                                                               false);
                            auto *fn = declareOp(mod, fnName, ty);
                            auto [n, tg] = collapseArgs(dstStrideVal, {st});
                            B.CreateCall(fn, {dst, dstStride, t, B.getInt32(st),
                                              B.getInt32(numKnots), arr,
                                              n, tg});
                        }
                    }
                }
            }

            // ================================================================
            // Array move ops
            // ================================================================
            else if (op == "ffroma")
                emitBin(ins, "op_ffroma", dst, dstStride, dstStrideVal);
            else if (op == "vfroma")
                emitBin(ins, "op_vfroma", dst, dstStride, dstStrideVal);
            else if (op == "mfroma")
                emitBin(ins, "op_mfroma", dst, dstStride, dstStrideVal);
            else if (op == "sfroma")
                emitBin(ins, "op_sfroma", dst, dstStride, dstStrideVal);
            else if (op == "uffroma" || op == "uvfroma" ||
                     op == "umfroma" || op == "usfroma") {
                // Uniform-array read: only the array operand (arr) is
                // uniform for this opcode family — that's what the "u"
                // prefix means, per the interpreter's UARRAY_UPDATE macro
                // (scriptOpcodes.h), which never advances op1 (the array)
                // but does advance op2 (the index) every iteration. The
                // index is a normal expression result and is commonly
                // varying (e.g. `arr[(int)mod(u*3,3)]`). Forcing idx's
                // stride to 0 here (as an earlier version of this code did)
                // read only slot 0 of the index for every vertex, producing
                // coherent-block misclassification instead of a per-vertex
                // lookup — confirmed via sphere-usfroma-reyes-slo mismatch.
                auto [arr, sa] = getVar(ins, 0);
                auto [idx, sidx] = getVar(ins, 1);
                if (!dst || !arr || !idx)
                    continue;
                const char *fnName = (op == "uffroma")   ? "op_ffroma"
                                     : (op == "uvfroma") ? "op_vfroma"
                                     : (op == "umfroma") ? "op_mfroma"
                                                         : "op_sfroma";
                auto *fn = declareOp(mod, fnName, binOpTy);
                auto [n, tg] = collapseArgs(dstStrideVal, {0, sidx});
                B.CreateCall(fn, {dst, dstStride, arr, B.getInt32(0),
                                  idx, B.getInt32(sidx), n, tg});
            }
            else if (op == "ftoa" || op == "vtoa" || op == "mtoa") {
                // Array element write: `ins.result` resolves to the array
                // itself (dst/dstStride == arr/arrStride). idx/val strides
                // must be threaded through (0 for a uniform/literal
                // operand): op_ftoa/vtoa/mtoa always loop numVerts times,
                // unlike the interpreter, which never advances a
                // compile-time-uniform instruction's operands at all (see
                // rslOps.h's array-move-ops comment) — a discarded stride
                // here would walk a single-element idx/val allocation out
                // of bounds.
                auto [idx, sidx] = getVar(ins, 0);
                auto [val, sval] = getVar(ins, 1);
                if (!dst || !idx || !val)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                const char *fnName = (op == "ftoa")   ? "op_ftoa"
                                     : (op == "vtoa") ? "op_vtoa"
                                                      : "op_mtoa";
                auto *fn = declareOp(mod, fnName, ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {sidx, sval});
                B.CreateCall(fn, {dst, dstStride, idx, B.getInt32(sidx),
                                  val, B.getInt32(sval), n, tg});
            }
            else if (op == "stoa") {
                // String array element write: val is char* const* (a
                // string literal or char** local), not float*, so it needs
                // the seql/sneql literal-string embedding pattern rather
                // than getVar (which only resolves numeric literals).
                if (ins.operands.size() < 2 || !dst)
                    continue;
                auto [idx, sidx] = getVar(ins, 0);
                if (!idx)
                    continue;
                const std::string &valTok = ins.operands[1].token;
                VarDesc valDesc;
                llvm::Value *val = nullptr;
                int sval = 0;
                if (resolveVar(valTok, valDesc)) {
                    val = loadVarPtr(valDesc);
                    sval = valDesc.stride;
                }
                else {
                    std::string s = valTok;
                    if (s.size() >= 2 && s.front() == '"')
                        s = s.substr(1, s.size() - 2);
                    llvm::Value *sptr = B.CreateGlobalString(s, "strlit");
                    llvm::Value *alloca = B.CreateAlloca(ptrTy, nullptr, "strlit_pp");
                    B.CreateStore(sptr, alloca);
                    val = alloca;
                    sval = 0;
                }
                if (!val)
                    continue;
                auto *ty = llvm::FunctionType::get(voidTy,
                                                   {ptrTy, i32Ty, ptrTy, i32Ty, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                auto *fn = declareOp(mod, "op_stoa", ty);
                auto [n, tg] = collapseArgs(dstStrideVal, {sidx, sval});
                B.CreateCall(fn, {dst, dstStride, idx, B.getInt32(sidx),
                                  val, B.getInt32(sval), n, tg});
            }

            // printf() (GitHub #11): was silently dropped here -- "covered"
            // by kHandledOpcodes[] with a dispatch case that emitted no IR
            // at all, so it compiled clean and did nothing at runtime.
            // "o=s.*" -- unlike format()'s "s=s.*", printf has no real RSL
            // result, so the bytecode binds its FIRST logical argument (the
            // format string) into ins.result instead of operands[0], the
            // same convention setcomp()'s "o=Vff" uses for its mutated
            // vector (see the setcomp case above). operands[0..N-1] are
            // the trailing values, each resolved via getVar() at its own
            // real RSL type. Confirmed via a throwaway pure-numeric probe's
            // compiled .rslo (`printf ("o=sff") result "..." v_1 v_1`) --
            // getVar(ins,0) on operands[0] was reading the FIRST VALUE as
            // the format string, corrupting every printf/format call with
            // >=1 trailing operand (caught during manual verification, not
            // by any test -- see the regression test added alongside this
            // fix).
            //
            // Unlike format() (whose dst is a real, possibly-uniform RSL
            // variable -- collapsing to n=1 there only skips redundant,
            // identical recomputation), printf's *count* of prints is
            // itself the observable behavior: the interpreter's PRINTFEXPR
            // loops every real vertex regardless of uniformity. So this
            // case never collapses to the uniform fast path -- always pass
            // the real numVerts/tags, exactly like the interpreter's own
            // per-vertex loop.
            else if (op == "printf") {
                if (ins.result.empty())
                    continue;
                llvm::Value *fmt = nullptr;
                int sf = 0;
                VarDesc fmtDesc{};
                if (resolveVar(ins.result, fmtDesc)) {
                    fmt = loadVarPtr(fmtDesc);
                    sf = fmtDesc.stride;
                } else {
                    std::tie(fmt, sf) = allocLiteral(ins.result);
                }
                if (!fmt)
                    continue;
                int numOperands = (int)ins.operands.size();
                int arrLen = numOperands > 0 ? numOperands : 1;
                auto *ptrArrTy = llvm::ArrayType::get(ptrTy, arrLen);
                auto *strideArrTy = llvm::ArrayType::get(i32Ty, arrLen);
                auto *ptrArr = B.CreateAlloca(ptrArrTy, nullptr, "printf_ops");
                auto *strideArr = B.CreateAlloca(strideArrTy, nullptr, "printf_strides");

                bool ok = true;
                for (int k = 0; k < numOperands; ++k) {
                    auto [p, s] = getVar(ins, k);
                    if (!p) {
                        ok = false;
                        break;
                    }
                    auto *pGep = B.CreateGEP(ptrArrTy, ptrArr, {B.getInt32(0), B.getInt32(k)});
                    B.CreateStore(p, pGep);
                    auto *sGep = B.CreateGEP(strideArrTy, strideArr, {B.getInt32(0), B.getInt32(k)});
                    B.CreateStore(B.getInt32(s), sGep);
                }
                if (ok) {
                    // (fmt, sf, operands**, strides*, numOperands, n, tags)
                    auto *ty = llvm::FunctionType::get(
                        voidTy, {ptrTy, i32Ty, ptrTy, ptrTy, i32Ty, i32Ty, ptrTy}, false);
                    auto *fn = declareOp(mod, "op_printf", ty);
                    B.CreateCall(fn, {fmt, B.getInt32(sf), ptrArr, strideArr,
                                      B.getInt32(numOperands), numVerts, tags});
                }
            }

            // Unrecognised opcode — skip silently. ("return"/"jmp" never
            // reach here -- both are already caught and handled at the top
            // of this same instruction loop, before this else-if chain.)
        }
    }

    // Ensure a terminator exists.
    if (!currentBlockHasTerminator(B))
        B.CreateRetVoid();
    return true;
}

// =========================================================================
// Check whether an IRFunction is non-trivial (has instructions besides return).
// =========================================================================
static bool isFnNonTrivial(const IRFunction &fn) {
    for (const IRBlock &blk : fn.blocks)
        for (const IRInstr &ins : blk.instrs)
            if (ins.opcode != "return")
                return true;
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
    auto *i32Ty = llvm::Type::getInt32Ty(ctx);
    auto *ptrTy = llvm::PointerType::getUnqual(ctx);

    auto varTbl = buildVarTable(mod);

    // Helper: build and emit one function with the standard shader signature.
    // Returns false if emitFunction() hit the hardened coverage gate (US2) —
    // the diagnostic naming the unhandled mnemonic is already printed by
    // emitFunction() itself at the point of failure.
    auto buildAndEmit = [&](const std::string &fnName, const IRFunction &irFn) -> bool {
        auto *funcTy = llvm::FunctionType::get(voidTy, {i32Ty, ptrTy, ptrTy}, false);
        auto *func = llvm::Function::Create(
            funcTy, llvm::Function::ExternalLinkage, fnName, llvmMod.get());

        auto args = func->arg_begin();
        llvm::Value *numVerts = &*args++;
        llvm::Value *stuffPtr = &*args++;
        llvm::Value *tags = &*args++;

        // Entry block: extract slot1 and slot2 from stuffPtr.
        // emitFunction() will append its instructions to this same block.
        auto *entry = llvm::BasicBlock::Create(ctx, "entry", func);
        llvm::IRBuilder<> B(entry);
        auto *slot1_pp = B.CreateGEP(ptrTy, stuffPtr, B.getInt32(1), "globals_pp");
        llvm::Value *slot1 = B.CreateLoad(ptrTy, slot1_pp, "globals");
        auto *slot2_pp = B.CreateGEP(ptrTy, stuffPtr, B.getInt32(2), "locals_pp");
        llvm::Value *slot2 = B.CreateLoad(ptrTy, slot2_pp, "locals");

        // Continue appending to entry (after the slot loads).
        return emitFunction(irFn, func, entry, numVerts, slot1, slot2, tags, varTbl, ctx, *llvmMod);
    };

    // -----------------------------------------------------------------------
    // Layer B: Compile #!Init section (if non-trivial) as shadername_init.
    // -----------------------------------------------------------------------
    bool hasInit = isFnNonTrivial(mod.initFn);
    if (hasInit) {
        if (!buildAndEmit(shaderName + "_init", mod.initFn))
            return false;
    }

    // -----------------------------------------------------------------------
    // Compile #!Code section as the main shader entry function.
    // -----------------------------------------------------------------------
    if (!buildAndEmit(shaderName, mod.codeFn))
        return false;

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
