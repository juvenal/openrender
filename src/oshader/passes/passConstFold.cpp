/**
 * Project: openRender
 *
 * File: passes/passConstFold.cpp
 *
 * Description:
 *   CConstFoldPass implementation.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "passConstFold.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_set>

// -------------------------------------------------------------------------
// isFoldable: scalar float opcodes we know how to evaluate
// -------------------------------------------------------------------------

// static
bool CConstFoldPass::isFoldable(const std::string &opcode) {
    static const char *foldableOps[] = {
        // Unary
        "negf", "sin", "cos", "tan",
        "asin", "acos", "atan",
        "exp", "log",
        "sqrt", "inversesqrt",
        "abs", "sign", "floor", "ceil",
        // Binary
        "addf", "addff",
        "subf", "subff",
        "mulf", "mulff",
        "divf", "divff",
        "pow", "mod", "atan2",
        // Comparisons → float 0/1
        "flt", "fle", "fgt", "fge", "feq", "fne",
        // Ternary
        "clampf", "mixf",
        nullptr
    };
    for (int i = 0; foldableOps[i] != nullptr; ++i)
        if (opcode == foldableOps[i]) return true;
    return false;
}

// -------------------------------------------------------------------------
// evalFloatOp: compute the result of a scalar float operation
// -------------------------------------------------------------------------

// static
bool CConstFoldPass::evalFloatOp(const std::string &op,
                                 const std::vector<double> &a,
                                 double &result) {
    const int n = static_cast<int>(a.size());

    // Unary
    if (op == "negf")  { if (n!=1) return false; result = -a[0]; }
    else if (op == "sin")  { if (n!=1) return false; result = std::sin(a[0]); }
    else if (op == "cos")  { if (n!=1) return false; result = std::cos(a[0]); }
    else if (op == "tan")  { if (n!=1) return false; result = std::tan(a[0]); }
    else if (op == "asin") { if (n!=1) return false; result = std::asin(a[0]); }
    else if (op == "acos") { if (n!=1) return false; result = std::acos(a[0]); }
    else if (op == "atan") { if (n!=1) return false; result = std::atan(a[0]); }
    else if (op == "exp")  { if (n!=1) return false; result = std::exp(a[0]); }
    else if (op == "log") {
        if (n!=1 || a[0] <= 0.0) return false;
        result = std::log(a[0]);
    }
    else if (op == "sqrt") {
        if (n!=1 || a[0] < 0.0) return false;
        result = std::sqrt(a[0]);
    }
    else if (op == "inversesqrt") {
        if (n!=1 || a[0] <= 0.0) return false;
        result = 1.0 / std::sqrt(a[0]);
    }
    else if (op == "abs")   { if (n!=1) return false; result = std::fabs(a[0]); }
    else if (op == "sign")  { if (n!=1) return false; result = (a[0]>0.0)?1.0:(a[0]<0.0)?-1.0:0.0; }
    else if (op == "floor") { if (n!=1) return false; result = std::floor(a[0]); }
    else if (op == "ceil")  { if (n!=1) return false; result = std::ceil(a[0]); }

    // Binary
    else if (op == "addf" || op == "addff") { if (n!=2) return false; result = a[0]+a[1]; }
    else if (op == "subf" || op == "subff") { if (n!=2) return false; result = a[0]-a[1]; }
    else if (op == "mulf" || op == "mulff") { if (n!=2) return false; result = a[0]*a[1]; }
    else if (op == "divf" || op == "divff") {
        if (n!=2 || a[1] == 0.0) return false;
        result = a[0] / a[1];
    }
    else if (op == "pow") {
        if (n!=2) return false;
        // Avoid domain error: base negative with non-integer exponent
        if (a[0] < 0.0 && std::fmod(a[1], 1.0) != 0.0) return false;
        result = std::pow(a[0], a[1]);
    }
    else if (op == "mod") {
        if (n!=2 || a[1] == 0.0) return false;
        result = std::fmod(a[0], a[1]);
    }
    else if (op == "atan2") { if (n!=2) return false; result = std::atan2(a[0], a[1]); }

    // Comparisons
    else if (op == "flt") { if (n!=2) return false; result = a[0] <  a[1] ? 1.0 : 0.0; }
    else if (op == "fle") { if (n!=2) return false; result = a[0] <= a[1] ? 1.0 : 0.0; }
    else if (op == "fgt") { if (n!=2) return false; result = a[0] >  a[1] ? 1.0 : 0.0; }
    else if (op == "fge") { if (n!=2) return false; result = a[0] >= a[1] ? 1.0 : 0.0; }
    else if (op == "feq") { if (n!=2) return false; result = a[0] == a[1] ? 1.0 : 0.0; }
    else if (op == "fne") { if (n!=2) return false; result = a[0] != a[1] ? 1.0 : 0.0; }

    // Ternary
    else if (op == "clampf") {
        // clampf result value min max
        if (n!=3) return false;
        result = std::min(std::max(a[0], a[1]), a[2]);
    }
    else if (op == "mixf") {
        // mixf result a b t  → a*(1-t) + b*t
        if (n!=3) return false;
        result = a[0]*(1.0-a[2]) + a[1]*a[2];
    }
    else {
        return false; // unknown / unsupported
    }

    // Reject non-finite results.
    if (!std::isfinite(result)) return false;

    return true;
}

// -------------------------------------------------------------------------
// resolveOperand: get numeric value of a token
// -------------------------------------------------------------------------

// static
bool CConstFoldPass::resolveOperand(
        const std::string &token,
        const std::unordered_map<std::string, double> &constMap,
        double &out) {
    if (token.empty()) return false;

    // Numeric literal: starts with digit, '-', or '.'
    const char c0 = token[0];
    if ((c0 >= '0' && c0 <= '9') || c0 == '-' || c0 == '.') {
        char *end = nullptr;
        out = std::strtod(token.c_str(), &end);
        return end != token.c_str();
    }

    // Named constant
    auto it = constMap.find(token);
    if (it != constMap.end()) {
        out = it->second;
        return true;
    }
    return false;
}

// -------------------------------------------------------------------------
// fmtDouble: format a double for the .sdr token stream
// -------------------------------------------------------------------------

// static
std::string CConstFoldPass::fmtDouble(double v) {
    // Use %.17g so the round-trip through strtod is exact.
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

// -------------------------------------------------------------------------
// foldFn: one forward sweep over an IRFunction
// -------------------------------------------------------------------------

// static
bool CConstFoldPass::foldFn(IRFunction &fn,
                            std::unordered_map<std::string, double> &constMap) {
    bool changed = false;

    // Pre-scan: collect every variable assigned inside any loop body or
    // conditional block.  These cannot be treated as compile-time constants
    // across control-flow boundaries:
    //   - Loop variables: change on each iteration (e.g. loop counter i).
    //   - Conditional variables: may or may not have been assigned depending
    //     on which branch was taken (e.g. yfract=0 inside an if-branch must
    //     not be assumed 0 after the endif when the else-branch may set a
    //     different value).
    std::unordered_set<std::string> cfAssignedVars; // loop + conditional
    {
        int depth = 0; // counts both loop and conditional nesting
        for (const IRBlock &blk : fn.blocks) {
            for (const IRInstr &instr : blk.instrs) {
                if (instr.opcode == "forbegin"
                    || instr.opcode == "if"
                    || instr.opcode == "else") {
                    ++depth;
                } else if (instr.opcode == "forend"
                           || instr.opcode == "endif") {
                    if (depth > 0) --depth;
                } else if (depth > 0 && instr.hasResult()) {
                    cfAssignedVars.insert(instr.result);
                }
            }
        }
    }

    for (IRBlock &blk : fn.blocks) {
        for (IRInstr &instr : blk.instrs) {
            // At any control-flow boundary (loop or conditional), flush all
            // variables assigned inside control-flow blocks from the constant
            // map.  This prevents incorrect propagation of a value set in one
            // branch across the if/else/endif or loop boundary.
            if (instr.opcode == "forbegin" || instr.opcode == "forend"
                || instr.opcode == "if"    || instr.opcode == "else"
                || instr.opcode == "endif") {
                for (const std::string &v : cfAssignedVars)
                    constMap.erase(v);
                continue;
            }

            if (!instr.hasResult()) continue;

            // vufloat result literal → seed the constant map, no rewrite needed.
            // If the operand is a named variable that is not a known constant,
            // erase any stale entry for result so it is not treated as constant.
            if (instr.opcode == "vufloat"
                && instr.operands.size() == 1) {
                double val;
                if (resolveOperand(instr.operands[0].token, constMap, val))
                    constMap[instr.result] = val;
                else
                    constMap.erase(instr.result);
                continue;
            }

            if (!isFoldable(instr.opcode)) {
                // Unknown / side-effecting op assigns a runtime value — invalidate.
                constMap.erase(instr.result);
                continue;
            }

            // Self-reference guard: "x = f(x, ...)" cannot be folded using the
            // pre-existing value of x (e.g., loop update "scale /= 2").
            bool selfRef = false;
            for (const IROperand &op : instr.operands) {
                if (!op.isLiteral() && !op.isQuoted() && op.token == instr.result) {
                    selfRef = true;
                    break;
                }
            }
            if (selfRef) {
                constMap.erase(instr.result);
                continue;
            }

            // Attempt to resolve all operands to constant values.
            std::vector<double> args;
            args.reserve(instr.operands.size());
            bool allConst = true;
            for (const IROperand &op : instr.operands) {
                double v;
                if (!resolveOperand(op.token, constMap, v)) {
                    allConst = false;
                    break;
                }
                args.push_back(v);
            }
            if (!allConst) {
                // At least one operand is runtime — result is no longer a known constant.
                constMap.erase(instr.result);
                continue;
            }

            // Evaluate the operation.
            double result;
            if (!evalFloatOp(instr.opcode, args, result)) continue;

            // Replace instruction in place with "vufloat result <value>".
            instr.opcode = "vufloat";
            instr.proto.clear();
            instr.operands.clear();
            IROperand litOp;
            litOp.token = fmtDouble(result);
            instr.operands.push_back(litOp);

            // Record the folded result for downstream instructions.
            constMap[instr.result] = result;
            changed = true;
        }
    }
    return changed;
}

// -------------------------------------------------------------------------
// CConstFoldPass::run
// -------------------------------------------------------------------------

bool CConstFoldPass::run(IRModule &mod) {
    bool anyChanged = false;

    // Use separate constant maps for initFn and codeFn.
    // Init sets parameter defaults (e.g., intensity=1) which must NOT be
    // treated as compile-time constants in Code — at runtime the caller
    // overwrites parameters with actual values before running Code.
    std::unordered_map<std::string, double> initMap;
    anyChanged |= foldFn(mod.initFn, initMap);

    std::unordered_map<std::string, double> codeMap;
    anyChanged |= foldFn(mod.codeFn, codeMap);

    return anyChanged;
}
