/**
 * Project: openRender
 *
 * File: irBuilder.h
 *
 * Description:
 *   CIRBuilder: builds an IRModule from the CScriptContext AST.
 *
 *   Strategy: the existing CExpression::getCode() methods emit text opcodes
 *   to a FILE*.  CIRBuilder captures this output via an in-memory stream,
 *   then parses the captured text into structured IRInstr / IRBlock objects.
 *   This approach lets the existing compiler emit code without modification
 *   while the IR layer provides the typed, analysable representation needed
 *   by optimization passes.
 *
 *   For a future visitor-based approach, replace build() with a recursive
 *   visit() over the AST that emits IRInstrs directly.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmailal.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OSHADER_IRBUILDER_H
#define OSHADER_IRBUILDER_H

#include "ir.h"
#include <memory>

// Forward declarations
class CScriptContext;

// -------------------------------------------------------------------------
// CIRBuilder
//
// Usage in generateCode():
//
//   CIRBuilder builder(*rslo);
//   IRModule *mod = builder.build(out);   // captures getCode() output
//
// After build() returns, the caller runs passes on *mod, then calls
// CRSLObjectEmitter::emit(*mod, out) to write the final .sdr file.
// -------------------------------------------------------------------------
class CIRBuilder {
public:
    explicit CIRBuilder(CScriptContext &ctx);
    ~CIRBuilder();

    // Build the IRModule for the shader in ctx.
    // This calls shaderFunction->initExpression->getCode() and
    // shaderFunction->code->getCode() with in-memory FILE* captures,
    // then parses the captured text into IRFunction blocks.
    // Returns ownership of the newly-allocated IRModule.
    std::unique_ptr<IRModule> build();

private:
    CScriptContext &ctx_;

    // Populate mod.vars from ctx_.variables list.
    void buildVarTable(IRModule &mod);

    // Parse one captured function section (init or code) into fn.
    // text is the null-terminated text captured from getCode().
    void parseSection(const char *text, IRFunction &fn);

    // Parse one instruction line into an IRInstr.
    // Returns false if the line is empty or a comment.
    static bool parseLine(const char *line, IRInstr &out);

    // Tokenize a whitespace-delimited line, respecting quoted strings.
    static std::vector<std::string> tokenize(const char *line);
};

#endif // OSHADER_IRBUILDER_H
