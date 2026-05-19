/**
 * Project: openRender
 *
 * File: ir.cpp
 *
 * Description:
 *   Implementation of the IR (Intermediate Representation) data structures.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "ir.h"
#include "rslo.h" // for SLC_xxx constants

#include <cstdlib>
#include <cassert>

// -------------------------------------------------------------------------
// IRVarInfo
// -------------------------------------------------------------------------

bool IRVarInfo::isUniform()   const { return (slcType & SLC_UNIFORM)    != 0; }
bool IRVarInfo::isVarying()   const { return (slcType & SLC_VARYING)    != 0; }
bool IRVarInfo::isParameter() const { return (slcType & SLC_PARAMETER)  != 0; }
bool IRVarInfo::isGlobal()    const { return (slcType & SLC_GLOBAL)     != 0; }
bool IRVarInfo::isOutput()    const { return (slcType & SLC_OUTPUT)     != 0; }
bool IRVarInfo::isArray()     const { return (slcType & SLC_ARRAY)      != 0; }
bool IRVarInfo::isFloat()     const { return (slcType & SLC_FLOAT)      != 0; }
bool IRVarInfo::isVector()    const { return (slcType & SLC_VECTOR)     != 0; }
bool IRVarInfo::isMatrix()    const { return (slcType & SLC_MATRIX)     != 0; }
bool IRVarInfo::isString()    const { return (slcType & SLC_STRING)     != 0; }

// -------------------------------------------------------------------------
// IROperand
// -------------------------------------------------------------------------

bool IROperand::isLiteral() const {
    if (token.empty()) return false;
    // A token is a numeric literal if it begins with a digit, '-', or '.'
    const char c = token[0];
    return (c >= '0' && c <= '9') || c == '-' || c == '.';
}

// -------------------------------------------------------------------------
// IRFunction
// -------------------------------------------------------------------------

void IRFunction::append(const IRInstr &instr) {
    if (blocks.empty())
        blocks.push_back(IRBlock{});
    blocks.back().instrs.push_back(instr);
}

void IRFunction::beginBlock(const std::string &label) {
    IRBlock blk;
    blk.label = label;
    blocks.push_back(blk);
}

// -------------------------------------------------------------------------
// IRModule
// -------------------------------------------------------------------------

int IRModule::addVar(const IRVarInfo &v) {
    const int idx = static_cast<int>(vars.size());
    vars.push_back(v);
    varIndex[v.cName] = idx;
    return idx;
}

const IRVarInfo *IRModule::findVar(const std::string &name) const {
    auto it = varIndex.find(name);
    if (it == varIndex.end()) return nullptr;
    return &vars[it->second];
}

IRVarInfo *IRModule::findVar(const std::string &name) {
    auto it = varIndex.find(name);
    if (it == varIndex.end()) return nullptr;
    return &vars[it->second];
}
