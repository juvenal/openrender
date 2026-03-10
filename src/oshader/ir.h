/**
 * Project: openRender
 *
 * File: ir.h
 *
 * Description:
 *   Compiler IR (Intermediate Representation) data structures for the RSL
 *   shader compiler.  The IR sits between the CExpression AST and the final
 *   .rslo/.sdr text output.  It is a flat, instruction-list based
 *   representation (analogous to LLVM's module/function/basicblock structure)
 *   that enables analysis and optimization passes before text emission.
 *
 *   Design principles:
 *   - One IRInstr per emitted opcode token (mirrors the .sdr text format).
 *   - IRVarInfo carries the full SLC_ type bits so passes can query
 *     container (UNIFORM/VARYING), base type, and array-ness without
 *     accessing the compiler's CVariable list.
 *   - IRFunction owns all storage: vector<IRBlock> and vector<IRVarInfo>.
 *   - Passes operate on IRFunction in-place.  The SdrTextEmitter converts
 *     the (possibly transformed) IRFunction back to .sdr text.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OSHADER_IR_H
#define OSHADER_IR_H

#include <string>
#include <vector>
#include <unordered_map>

// -------------------------------------------------------------------------
// IRVarInfo — type and container information for one shader variable.
//
// This mirrors the relevant fields of CVariable / CParameter without
// coupling the IR to the compiler's live object graph.
// -------------------------------------------------------------------------
struct IRVarInfo {
    std::string cName;        // unique code name (e.g. "temporary_0")
    std::string symbolName;   // RSL source name (e.g. "Ks")
    int         slcType;      // SLC_xxx bit flags from sdr.h (type + qualifiers)
    int         numItems;     // 1 for scalars; >1 for arrays
    std::string defaultValue; // parameter default, or "" if none

    // Convenience predicates
    bool isUniform()   const;
    bool isVarying()   const;
    bool isParameter() const;
    bool isGlobal()    const;
    bool isOutput()    const;
    bool isArray()     const;
    bool isFloat()     const;
    bool isVector()    const;
    bool isMatrix()    const;
    bool isString()    const;
};

// -------------------------------------------------------------------------
// IROperand — one operand token inside an IRInstr.
//
// Operands are either a named variable reference or a literal token
// (number, quoted string, "#!LabelN", etc.).
// -------------------------------------------------------------------------
struct IROperand {
    std::string token; // raw token as it appears in the .sdr stream

    bool isLabel()   const { return !token.empty() && token[0] == '#'; }
    bool isQuoted()  const { return !token.empty() && token[0] == '"'; }
    bool isLiteral() const; // true if the token parses as a number
};

// -------------------------------------------------------------------------
// IRInstr — one instruction in the IR.
//
// Corresponds to one non-blank, non-label line from the .sdr code section.
// Format: [opcode] [("prototype")] [result] [operand ...]
//
// Some opcodes (e.g. normalize, faceforward) carry a parenthesised type
// signature; that is stored separately in `proto` so passes can inspect it.
// The `result` field is the first non-proto operand for most opcodes (the
// destination variable).  For opcodes with no result (e.g. return, for,
// forbegin) it is empty.
// -------------------------------------------------------------------------
struct IRInstr {
    std::string              opcode;    // opcode token
    std::string              proto;     // type signature e.g. "v=v", or ""
    std::string              result;    // destination variable name, or ""
    std::vector<IROperand>   operands;  // remaining operand tokens

    bool hasResult() const { return !result.empty(); }
};

// -------------------------------------------------------------------------
// IRBlock — a labelled sequence of instructions (basic block).
//
// A block starts at `label` (which may be "" for the entry block) and ends
// at the first branch/return or at the start of the next block.
// -------------------------------------------------------------------------
struct IRBlock {
    std::string          label;   // "#!LabelN" or "" for the entry block
    std::vector<IRInstr> instrs;
};

// -------------------------------------------------------------------------
// IRFunction — one shader function (init section or code section).
// -------------------------------------------------------------------------
struct IRFunction {
    std::string           name;     // "Init" or "Code"
    std::vector<IRBlock>  blocks;   // basic blocks in order

    // Convenience: append an instruction to the last block.
    void append(const IRInstr &instr);

    // Convenience: start a new labelled block.
    void beginBlock(const std::string &label);

    // Iteration helpers.
    IRBlock &entryBlock() { return blocks.front(); }
};

// -------------------------------------------------------------------------
// IRModule — the complete compiled shader.
//
// An IRModule holds all metadata plus two IRFunctions (init and code).
// The variable table is shared across both functions.
// -------------------------------------------------------------------------
struct IRModule {
    std::string shaderType;   // "surface", "light", "displacement", etc.
    std::string version;      // e.g. "1.0.0"

    // Variable table: ordered list (preserves original declaration order).
    std::vector<IRVarInfo>   vars;

    // Fast name lookup (name → index into vars).
    std::unordered_map<std::string, int> varIndex;

    IRFunction initFn;  // #!Init: section
    IRFunction codeFn;  // #!Code: section

    // Add a variable to the table; returns its index.
    int addVar(const IRVarInfo &v);

    // Look up a variable by cName; returns nullptr if not found.
    const IRVarInfo *findVar(const std::string &name) const;
    IRVarInfo       *findVar(const std::string &name);
};

#endif // OSHADER_IR_H
