/**
 * Project: openRender
 *
 * File: irBuilder.cpp
 *
 * Description:
 *   CIRBuilder implementation: captures getCode() text output and parses it
 *   into a structured IRModule.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "irBuilder.h"
#include "ir.h"
#include "rslo.h"

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <sstream>

// open_memstream is POSIX (macOS 10.10+, Linux glibc).
// On Windows we fall back to a tmpfile + read approach.
#ifndef _WINDOWS
#  include <stdio.h>
// open_memstream declared in stdio.h on POSIX systems
#else
// Windows fallback: use a temp file
#  include <windows.h>
#endif

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

// Trim leading and trailing ASCII whitespace from a string view (in-place).
static std::string trimWhitespace(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// -------------------------------------------------------------------------
// CIRBuilder
// -------------------------------------------------------------------------

CIRBuilder::CIRBuilder(CScriptContext &ctx)
    : ctx_(ctx)
{}

CIRBuilder::~CIRBuilder() = default;

std::unique_ptr<IRModule> CIRBuilder::build() {
    auto mod = std::make_unique<IRModule>();

    // Version and shader type strings come from ctx_ (caller sets them via
    // the version constants; we reconstruct them from the shader type flag).
    mod->version = "1.0.0"; // matches version constants in the build

    switch (ctx_.shaderType) {
    case SLC_SURFACE:        mod->shaderType = "surface";        break;
    case SLC_LIGHT:          mod->shaderType = "light";          break;
    case SLC_DISPLACEMENT:   mod->shaderType = "displacement";   break;
    case SLC_VOLUME:         mod->shaderType = "volume";         break;
    case SLC_TRANSFORMATION: mod->shaderType = "transformation"; break;
    case SLC_IMAGER:         mod->shaderType = "imager";         break;
    default:                 mod->shaderType = "generic";        break;
    }

    mod->initFn.name = "Init";
    mod->codeFn.name = "Code";

    // Build the typed variable table from ctx_.variables.
    buildVarTable(*mod);

    // Capture and parse the Init section.
    // uniformParameters() must be called first (as in generateCode()) so that
    // parameters are treated as UNIFORM during init code type inference.
    ctx_.uniformParameters();
    {
#ifndef _WINDOWS
        char *buf = nullptr;
        size_t len = 0;
        FILE *mstream = open_memstream(&buf, &len);
        if (mstream) {
            if (ctx_.shaderFunction->initExpression != nullptr)
                ctx_.shaderFunction->initExpression->getCode(mstream, nullptr);
            fclose(mstream);
            parseSection(buf, mod->initFn);
            free(buf);
        }
#else
        // Windows fallback: use a temp file
        FILE *tmp = tmpfile();
        if (tmp) {
            if (ctx_.shaderFunction->initExpression != nullptr)
                ctx_.shaderFunction->initExpression->getCode(tmp, nullptr);
            long size = ftell(tmp);
            rewind(tmp);
            std::string buf(size, '\0');
            fread(&buf[0], 1, size, tmp);
            fclose(tmp);
            parseSection(buf.c_str(), mod->initFn);
        }
#endif
    }
    ctx_.restoreParameters();

    // Capture and parse the Code section (parameters restored to original types).
    {
#ifndef _WINDOWS
        char *buf = nullptr;
        size_t len = 0;
        FILE *mstream = open_memstream(&buf, &len);
        if (mstream) {
            if (ctx_.shaderFunction->code != nullptr)
                ctx_.shaderFunction->code->getCode(mstream, nullptr);
            fclose(mstream);
            parseSection(buf, mod->codeFn);
            free(buf);
        }
#else
        FILE *tmp = tmpfile();
        if (tmp) {
            if (ctx_.shaderFunction->code != nullptr)
                ctx_.shaderFunction->code->getCode(tmp, nullptr);
            long size = ftell(tmp);
            rewind(tmp);
            std::string buf(size, '\0');
            fread(&buf[0], 1, size, tmp);
            fclose(tmp);
            parseSection(buf.c_str(), mod->codeFn);
        }
#endif
    }

    return mod;
}

// -------------------------------------------------------------------------
// buildVarTable: populate mod.vars from:
//   1. RSL global scope (ctx_.variables) — P, N, Ci, etc.  (SLC_GLOBAL bit)
//   2. Shader parameters (ctx_.shaderFunction->parameters) — Ka, Kd, …
//   3. Shader local vars (ctx_.shaderFunction->variables)  — temporaries
//
// Parameters come before locals so that the sequential slot-2 index assigned
// by the LLVM emitter matches what CProgrammableShaderInstance::prepare()
// builds from the #!parameters / #!variables sections in the .rslo file.
// -------------------------------------------------------------------------

static void addVar(IRModule &mod, CVariable *cvar) {
    if (!cvar) return;
    IRVarInfo v;
    v.symbolName = cvar->symbolName ? cvar->symbolName : "";
    // Parameters use symbolName as their code name (no separate cName).
    v.cName      = cvar->cName ? cvar->cName : v.symbolName;
    v.slcType    = cvar->type;
    v.numItems   = cvar->numItems;
    v.defaultValue = "";
    if (cvar->type & SLC_PARAMETER) {
        CParameter *cp = static_cast<CParameter *>(cvar);
        if (cp->defaultValue) v.defaultValue = cp->defaultValue;
    }
    // Include if either name is non-empty.
    if (!v.cName.empty() || !v.symbolName.empty())
        mod.addVar(v);
}

void CIRBuilder::buildVarTable(IRModule &mod) {
    // ctx_.variables is the full scope list: RSL globals AND shader parameters
    // (parameters have cName=null; addVar() falls back to symbolName for them).
    CVariable *cvar = ctx_.variables->first();
    while (cvar != nullptr) { addVar(mod, cvar); cvar = ctx_.variables->next(); }

    // Shader local variables (temporaries) are in shaderFunction->variables
    // and are NOT in ctx_.variables, so we add them separately.
    if (ctx_.shaderFunction) {
        CVariable *cv = ctx_.shaderFunction->variables->first();
        while (cv != nullptr) { addVar(mod, cv); cv = ctx_.shaderFunction->variables->next(); }
    }
}

// -------------------------------------------------------------------------
// parseSection: parse captured text into an IRFunction
// -------------------------------------------------------------------------

void CIRBuilder::parseSection(const char *text, IRFunction &fn) {
    if (!text || *text == '\0') return;

    // Ensure there's at least one entry block.
    fn.blocks.push_back(IRBlock{});

    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        const std::string trimmed = trimWhitespace(line);
        if (trimmed.empty()) continue;

        // Label lines: "#!LabelN:"
        if (trimmed[0] == '#' && trimmed.find(':') != std::string::npos) {
            // This is a label definition; start a new block.
            std::string label = trimmed;
            if (!label.empty() && label.back() == ':')
                label.pop_back(); // strip trailing colon
            fn.beginBlock(label);
            continue;
        }

        IRInstr instr;
        if (parseLine(trimmed.c_str(), instr)) {
            fn.append(instr);
        }
    }

    // Remove trailing empty entry block if nothing was added.
    if (!fn.blocks.empty() && fn.blocks.back().instrs.empty()
        && fn.blocks.back().label.empty() && fn.blocks.size() > 1) {
        fn.blocks.pop_back();
    }
}

// -------------------------------------------------------------------------
// parseLine: parse one instruction line into an IRInstr
// -------------------------------------------------------------------------

// static
bool CIRBuilder::parseLine(const char *line, IRInstr &out) {
    const std::vector<std::string> tokens = tokenize(line);
    if (tokens.empty()) return false;

    size_t idx = 0;
    out.opcode = tokens[idx++];

    // If the next token starts with '(' it's the type signature.
    if (idx < tokens.size() && !tokens[idx].empty() && tokens[idx][0] == '(') {
        std::string sig = tokens[idx++];
        // Strip enclosing parentheses and quotes: ("v=vv") → v=vv
        if (sig.size() >= 4)
            out.proto = sig.substr(2, sig.size() - 4); // remove (" and ")
        else
            out.proto = sig;
    }

    // The first remaining token is the result (destination variable).
    // Opcodes with no result: return, for, forbegin, forend,
    // if, else, endif, illuminance/end, illuminate/end, solar/end,
    // gather*, break, continue.
    static const char *noResultOpcodes[] = {
        "return", "for", "forbegin", "forend",
        "if", "else", "endif",
        "illuminance", "endilluminance",
        "illuminate", "endilluminate",
        "solar", "endsolar",
        "gatherhdr", "gather", "gatherelse", "gatherend",
        "break", "continue",
        nullptr
    };
    bool hasResult = true;
    for (int i = 0; noResultOpcodes[i] != nullptr; ++i) {
        if (out.opcode == noResultOpcodes[i]) { hasResult = false; break; }
    }

    if (hasResult && idx < tokens.size()) {
        out.result = tokens[idx++];
    }

    // Remaining tokens are source operands.
    while (idx < tokens.size()) {
        IROperand op;
        op.token = tokens[idx++];
        out.operands.push_back(op);
    }

    return true;
}

// -------------------------------------------------------------------------
// tokenize: split a line into whitespace-separated tokens,
//           preserving quoted strings as single tokens.
// -------------------------------------------------------------------------

// static
std::vector<std::string> CIRBuilder::tokenize(const char *line) {
    std::vector<std::string> tokens;
    const char *p = line;
    while (*p) {
        // Skip whitespace.
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') break;

        if (*p == '"') {
            // Quoted string — include the surrounding quotes.
            const char *start = p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) ++p; // escaped char
                ++p;
            }
            if (*p == '"') ++p; // consume closing quote
            tokens.emplace_back(start, p);
        } else if (*p == '(') {
            // Parenthesised prototype like ("v=v") — consume until ')'.
            const char *start = p++;
            while (*p && *p != ')') ++p;
            if (*p == ')') ++p;
            tokens.emplace_back(start, p);
        } else {
            // Regular token.
            const char *start = p;
            while (*p && *p != ' ' && *p != '\t') ++p;
            tokens.emplace_back(start, p);
        }
    }
    return tokens;
}
