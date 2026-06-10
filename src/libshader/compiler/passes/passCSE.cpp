/**
 * Project: openRender
 *
 * File: passes/passCSE.cpp
 *
 * Description:
 *   CCSEPass: Common Subexpression Elimination + Loop-Invariant Code Motion.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "passCSE.h"

// -------------------------------------------------------------------------
// isPure: opcodes safe for both CSE and LICM
// (must match DCE's isSafeToRemove list to maintain consistency)
// -------------------------------------------------------------------------

// static
bool CCSEPass::isPure(const std::string &opcode) {
    static const char *pure[] = {
        // Move / constant load
        "moveff", "movevv", "movemm", "movess",
        "vufloat", "vuvector", "vumatrix", "vustring",
        // Arithmetic — float
        "addf", "subf", "mulf", "divf", "negf",
        "addff", "subff", "mulff", "divff",
        // Arithmetic — vector
        "addvv", "subvv", "mulvv", "divvv",
        "addvf", "subvf", "mulvf", "divvf",
        "negv",
        // Comparisons
        "flt", "fle", "fgt", "fge", "feq", "fne",
        "vlt", "vle", "vgt", "vge", "veq", "vne",
        // Math
        "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
        "exp", "log", "sqrt", "abs", "sign", "floor", "ceil",
        "pow", "mod", "clampf", "clampv", "mixf", "mixv",
        "normalize", "length", "dot", "cross",
        "vfromf", "vfromvff", "vfromfff",
        "xcomp", "ycomp", "zcomp",
        "setxcomp", "setycomp", "setzcomp",
        "matfromf", "matfromv",
        "inversesqrt",
        nullptr
    };
    for (int i = 0; pure[i] != nullptr; ++i)
        if (opcode == pure[i]) return true;
    return false;
}

// -------------------------------------------------------------------------
// isLoopBegin / loopEndOpcode
// -------------------------------------------------------------------------

// static
bool CCSEPass::isLoopBegin(const std::string &opcode) {
    return opcode == "illuminance"
        || opcode == "illuminate"
        || opcode == "solar";
}

// static
std::string CCSEPass::loopEndOpcode(const std::string &begin) {
    if (begin == "illuminance") return "endilluminance";
    if (begin == "illuminate")  return "endilluminate";
    if (begin == "solar")       return "endsolar";
    return "";
}

// -------------------------------------------------------------------------
// isLoopBoundVar: L, Cl, Ol change each illuminance/illuminate iteration
// -------------------------------------------------------------------------

// static
bool CCSEPass::isLoopBoundVar(const std::string &cName, const IRModule &mod) {
    static const char *bound[] = {"L", "Cl", "Ol", nullptr};
    const IRVarInfo *vi = mod.findVar(cName);
    if (!vi) return false;
    for (int i = 0; bound[i] != nullptr; ++i)
        if (vi->symbolName == bound[i]) return true;
    return false;
}

// -------------------------------------------------------------------------
// exprKey: build a canonical identity key for CSE
// -------------------------------------------------------------------------

// static
std::string CCSEPass::exprKey(const IRInstr &instr) {
    if (!instr.hasResult())       return "";
    if (!isPure(instr.opcode))    return "";

    // "vufloat"/"vuvector" load constants — each occurrence loads the same
    // value, so they ARE candidates for CSE.  We include the full opcode and
    // operands in the key, so only identical constant loads are merged.

    std::string key;
    key.reserve(64);
    key += instr.opcode;
    key += '|';
    key += instr.proto;
    for (const IROperand &op : instr.operands) {
        key += '|';
        key += op.token;
    }
    return key;
}

// -------------------------------------------------------------------------
// moveOpcode: pick the right scalar-move opcode for a given result variable
// -------------------------------------------------------------------------

// static
std::string CCSEPass::moveOpcode(const std::string &result,
                                 const IRModule &mod) {
    const IRVarInfo *vi = mod.findVar(result);
    if (vi) {
        if (vi->isVector()) return "movevv";
        if (vi->isMatrix()) return "movemm";
        if (vi->isString()) return "movess";
    }
    return "moveff"; // float or unknown
}

// =========================================================================
// CSE
// =========================================================================

// Invalidate all exprMap entries that are stale after variable `def` is
// redefined.  An entry is stale when:
//   (a) it stores `def` as the canonical first-result, or
//   (b) the expression key references `def` as an operand token.
//
// Keys have the form "opcode|proto|op1|op2|...".  Fencing the key and
// the token with '|' on both sides gives exact token matching without
// false positives from prefix/suffix substrings (e.g. "temporary_4" vs
// "temporary_40").
static void invalidateVar(std::unordered_map<std::string, std::string> &exprMap,
                          const std::string &def) {
    const std::string needle = "|" + def + "|";
    for (auto it = exprMap.begin(); it != exprMap.end(); ) {
        bool stale = (it->second == def);
        if (!stale) {
            // Fence both the key and the needle so the last operand token
            // is also caught: "|op1|op2|" contains "|op2|".
            const std::string fencedKey = "|" + it->first + "|";
            stale = (fencedKey.find(needle) != std::string::npos);
        }
        it = stale ? exprMap.erase(it) : ++it;
    }
}

// static
bool CCSEPass::cseFn(IRFunction &fn, const IRModule &mod) {
    bool changed = false;

    // exprMap: expression key → first result variable that computed it.
    std::unordered_map<std::string, std::string> exprMap;

    // Track every variable that has been assigned at least once.
    // When a variable is assigned a second time its old value is gone, so
    // any cached expression that depended on it must be invalidated.
    std::unordered_set<std::string> defsSeen;

    for (IRBlock &blk : fn.blocks) {
        // Local (intra-block) CSE only: cross-block substitution requires
        // dominator analysis that the IR does not currently encode.  Without
        // it, an entry added inside a conditional branch would be applied to
        // code on paths that never executed that branch, corrupting results.
        exprMap.clear();
        for (IRInstr &instr : blk.instrs) {
            // Invalidate stale entries before inspecting this instruction.
            // Always call invalidateVar — not just on re-definitions — because
            // a variable may be written for the first time by a non-pure
            // instruction (e.g. calculatenormal redefines N) and the old cached
            // expression that used it (e.g. normalize(N)) would otherwise be
            // kept alive, causing subsequent normalize(N) to reuse the pre-update
            // cached result.
            if (instr.hasResult())
                invalidateVar(exprMap, instr.result);

            const std::string key = exprKey(instr);
            if (key.empty()) {
                if (instr.hasResult()) defsSeen.insert(instr.result);
                continue;
            }

            auto it = exprMap.find(key);
            if (it != exprMap.end()) {
                // Duplicate expression: replace with a move from the first result.
                const std::string &prev = it->second;
                const std::string mov   = moveOpcode(instr.result, mod);

                instr.opcode = mov;
                instr.proto.clear();
                instr.operands.clear();

                IROperand src;
                src.token = prev;
                instr.operands.push_back(src);

                changed = true;
            } else {
                exprMap[key] = instr.result;
            }

            if (instr.hasResult()) defsSeen.insert(instr.result);
        }
    }
    return changed;
}

// =========================================================================
// LICM
// =========================================================================

// A flat position within an IRFunction's instruction stream.
struct FlatPos { int b; int i; };

// Build a flat ordered index of all instruction positions across all blocks.
static std::vector<FlatPos> buildFlat(const IRFunction &fn) {
    std::vector<FlatPos> flat;
    for (int b = 0; b < static_cast<int>(fn.blocks.size()); ++b)
        for (int i = 0; i < static_cast<int>(fn.blocks[b].instrs.size()); ++i)
            flat.push_back({b, i});
    return flat;
}

// static
bool CCSEPass::licmFn(IRFunction &fn, const IRModule &mod) {
    bool anyHoisted = false;

    // Repeat: hoist one instruction at a time then rebuild the flat index.
    // This is O(n²) in the number of hoistable instructions, which is
    // acceptable given typical RSL shader sizes.
    bool hoisted;
    do {
        hoisted = false;

        const std::vector<FlatPos> flat = buildFlat(fn);
        const int N = static_cast<int>(flat.size());

        for (int f = 0; f < N; ++f) {
            const IRInstr &loopBeginInstr =
                fn.blocks[flat[f].b].instrs[flat[f].i];
            if (!isLoopBegin(loopBeginInstr.opcode)) continue;

            const std::string endOp = loopEndOpcode(loopBeginInstr.opcode);

            // Find matching loop-end, tracking nesting depth.
            int depth = 1;
            int g = f + 1;
            while (g < N) {
                const IRInstr &cur =
                    fn.blocks[flat[g].b].instrs[flat[g].i];
                if (isLoopBegin(cur.opcode)) ++depth;
                if (cur.opcode == endOp && --depth == 0) break;
                ++g;
            }
            if (g >= N) continue; // unmatched loop — leave alone

            // Collect the set of variables defined inside the loop body
            // (flat[f+1] to flat[g-1]).
            std::unordered_set<std::string> loopDefs;
            for (int h = f + 1; h < g; ++h) {
                const IRInstr &instr =
                    fn.blocks[flat[h].b].instrs[flat[h].i];
                if (instr.hasResult())
                    loopDefs.insert(instr.result);
            }

            // Find the first invariant instruction in the loop body.
            // (We hoist one per outer iteration, then rebuild.)
            for (int h = f + 1; h < g; ++h) {
                const FlatPos &pos = flat[h];
                IRInstr &instr = fn.blocks[pos.b].instrs[pos.i];

                if (!instr.hasResult())    continue;
                if (!isPure(instr.opcode)) continue;

                // Do not hoist an instruction whose result variable is also
                // defined (re-assigned) elsewhere inside the loop body.
                // Example: in a light shader "vuvector Cl tmp" is invariant
                // w.r.t. its operands, but Cl IS the loop output and must stay
                // inside the loop body.  Similarly, a temporary that is reused
                // as a loop-varying result (e.g. temporary_7 = length(L)) must
                // not have its pre-loop definition hoisted since the loop would
                // overwrite it on the first iteration and subsequent consumers
                // would read the wrong value.
                if (loopDefs.count(instr.result)) continue;

                // Also skip known light-loop output variables (Cl, L, Ol)
                // even if they are not yet in loopDefs (they may only appear
                // as results of the loop-end machinery outside the body range).
                if (isLoopBoundVar(instr.result, mod)) continue;

                // Check operand invariance.
                bool invariant = true;
                for (const IROperand &op : instr.operands) {
                    if (op.isLiteral() || op.isQuoted() || op.isLabel())
                        continue;
                    // Defined inside the loop → varying.
                    if (loopDefs.count(op.token)) {
                        invariant = false;
                        break;
                    }
                    // Known light-loop-bound built-in → varying.
                    if (isLoopBoundVar(op.token, mod)) {
                        invariant = false;
                        break;
                    }
                }
                if (!invariant) continue;

                // Hoist: copy the instruction, then erase from loop body,
                // then insert before the loop-begin instruction.
                IRInstr copy = instr;

                // Erase from current position.
                fn.blocks[pos.b].instrs.erase(
                    fn.blocks[pos.b].instrs.begin() + pos.i);

                // Insert before the loop-begin instruction.
                // After the erase above, if the loop-begin is in the same
                // block and at a higher index, its index shifts down by one.
                int lb = flat[f].b;
                int li = flat[f].i;
                if (pos.b == lb && pos.i < li) --li;

                fn.blocks[lb].instrs.insert(
                    fn.blocks[lb].instrs.begin() + li, copy);

                // The result is now defined before the loop, so remove it
                // from loopDefs so subsequent iterations can see it as
                // invariant for instructions that depend on it.
                loopDefs.erase(copy.result);

                hoisted     = true;
                anyHoisted  = true;
                break; // rebuild flat and rescan
            }

            if (hoisted) break; // restart the outer loop-scan from the top
        }
    } while (hoisted);

    return anyHoisted;
}

// =========================================================================
// CCSEPass::run
// =========================================================================

bool CCSEPass::run(IRModule &mod) {
    bool anyChanged = false;

    // LICM first: runs to its own internal fixpoint, hoisting all
    // invariant instructions out of illuminance/illuminate/solar loops.
    anyChanged |= licmFn(mod.initFn, mod);
    anyChanged |= licmFn(mod.codeFn, mod);

    // CSE once after LICM: hoisting may have placed identical subexpressions
    // adjacent, making them visible to a single linear scan.
    anyChanged |= cseFn(mod.initFn, mod);
    anyChanged |= cseFn(mod.codeFn, mod);

    return anyChanged;
}
