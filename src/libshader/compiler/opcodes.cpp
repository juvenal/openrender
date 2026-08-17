/**
 * Project: openRender
 *
 * File: opcodes.cpp
 *
 * Description:
 *   This file implements the functionality for opcodes.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	opcodes.cpp
//  Classes				:
//  Description			:	All the string definitions
//
////////////////////////////////////////////////////////////////////////
#include "opcodes.h"

////////////////////////////////////////////////////////////////////////
//	Conditionals
const char *opcodeIf = "\tif                ";
const char *opcodeElse = "\telse              ";
const char *opcodeEndif = "\tendif             ";
const char *opcodeGatherHeader = "\tgatherHeader      ";
const char *opcodeGather = "\tgather            ";
const char *opcodeGatherElse = "\tgatherElse        ";
const char *opcodeGatherEnd = "\tgatherEnd         ";
const char *opcodeFor = "\tfor               ";
const char *opcodeBeginfor = "\tforbegin          ";
const char *opcodeEndfor = "\tforend            ";
const char *opcodeIlluminance = "\tilluminance       ";
const char *opcodeBeginIlluminance = "\tbeginilluminance  ";
const char *opcodeEndIlluminance = "\tendilluminance    ";
const char *opcodeSolar = "\tsolar             ";
const char *opcodeEndSolar = "\tendsolar          ";
const char *opcodeIlluminate = "\tilluminate        ";
const char *opcodeEndIlluminate = "\tendilluminate     ";
const char *opcodeBreak = "\tbreak             ";
const char *opcodeContinue = "\tcontinue          ";
const char *opcodeReturn = "\treturn            ";

////////////////////////////////////////////////////////////////////////
//	Relations

// Equal
const char *opcodeFloatEqual = "\tfeql              ";
const char *opcodeVectorEqual = "\tveql              ";
const char *opcodeStringEqual = "\tseql              ";
const char *opcodeMatrixEqual = "\tmeql              ";

// Not Equal
const char *opcodeFloatNotEqual = "\tfneql             ";
const char *opcodeVectorNotEqual = "\tvneql             ";
const char *opcodeStringNotEqual = "\tsneql             ";
const char *opcodeMatrixNotEqual = "\tmneql             ";

// Less than or equal
const char *opcodeFloatELess = "\tfelt              ";
const char *opcodeVectorELess = "\tvelt              ";

// Less than
const char *opcodeFloatLess = "\tflt               ";
const char *opcodeVectorLess = "\tvlt               ";

// Greater than or equal
const char *opcodeFloatEGreater = "\tfegt              ";
const char *opcodeVectorEGreater = "\tvegt              ";

// Greater than
const char *opcodeFloatGreater = "\tfgt               ";
const char *opcodeVectorGreater = "\tvgt               ";

////////////////////////////////////////////////////////////////////////
//	Conversions
const char *opcodeMatrixFromFloat = "\tmfromf            ";
const char *opcodeVectorFromFloat = "\tvfromf            ";
const char *opcodeMatrixFromVector = "\tmfromv            ";

// System conversion
const char *opcodeVectorFrom = "\tvfrom             ";
const char *opcodeColorFrom = "\tcfrom             ";
const char *opcodePointFrom = "\tpfrom             ";
const char *opcodeMatrixFrom = "\tmfrom             ";

////////////////////////////////////////////////////////////////////////
//	Boolean operations
const char *opcodeAnd = "\tand               ";
const char *opcodeOr = "\tor                ";
const char *opcodeXor = "\txor               ";
const char *opcodeNXor = "\tnxor              ";
const char *opcodeNot = "\tnot               ";

////////////////////////////////////////////////////////////////////////
//	Unary operations

//	Negetion
const char *opcodeNegFloat = "\tnegf              ";
const char *opcodeNegVector = "\tnegv              ";
const char *opcodeNegMatrix = "\tnegm              ";

////////////////////////////////////////////////////////////////////////
//	Binary operations

// Dot and cross products
const char *opcodeDotProduct = "\tdot               ";
const char *opcodeCrossProduct = "\tcross             ";

// Division
const char *opcodeDivFloatFloat = "\tdivff             ";
const char *opcodeDivVectorVector = "\tdivvv             ";
const char *opcodeDivMatrixMatrix = "\tdivmm             ";

// Multipication
const char *opcodeMulFloatFloat = "\tmulff             ";
const char *opcodeMulVectorVector = "\tmulvv             ";
const char *opcodeMulMatrixMatrix = "\tmulmm             ";
const char *opcodeMulMatrixPoint = "\tmulmp             ";
const char *opcodeMulMatrixNormal = "\tmulmn             ";
const char *opcodeMulMatrixVector = "\tmulmv             ";
const char *opcodeMulPointMatrix = "\tmulpm             ";
const char *opcodeMulNormalMatrix = "\tmulnm             ";
const char *opcodeMulVectorMatrix = "\tmulvm             ";

// Addition
const char *opcodeAddFloatFloat = "\taddff             ";
const char *opcodeAddVectorVector = "\taddvv             ";
const char *opcodeAddMatrixMatrix = "\taddmm             ";

// Subtraction
const char *opcodeSubFloatFloat = "\tsubff             ";
const char *opcodeSubVectorVector = "\tsubvv             ";
const char *opcodeSubMatrixMatrix = "\tsubmm             ";

////////////////////////////////////////////////////////////////////////
//	Data movement operations

// Uniform to varying assignment
const char *opcodeVUFloat = "\tvufloat           ";
const char *opcodeVUVector = "\tvuvector          ";
const char *opcodeVUMatrix = "\tvumatrix          ";
const char *opcodeVUString = "\tvustring          ";

//	Move
const char *opcodeMoveFloatFloat = "\tmoveff            ";
const char *opcodeMoveVectorVector = "\tmovevv            ";
const char *opcodeMoveStringString = "\tmovess            ";
const char *opcodeMoveMatrixMatrix = "\tmovemm            ";
const char *opcodeMoveAFloatFloat = "\tmoveaff           ";
const char *opcodeMoveAVectorVector = "\tmoveavv           ";
const char *opcodeMoveAStringString = "\tmoveass           ";
const char *opcodeMoveAMatrixMatrix = "\tmoveamm           ";
const char *opcodeFFromArray = "\tffroma            ";
const char *opcodeFToArray = "\tftoa              ";
const char *opcodeVFromArray = "\tvfroma            ";
const char *opcodeVToArray = "\tvtoa              ";
const char *opcodeMFromArray = "\tmfroma            ";
const char *opcodeMToArray = "\tmtoa              ";
const char *opcodeSFromArray = "\tsfroma            ";
const char *opcodeSToArray = "\tstoa              ";
const char *opcodeUFFromArray = "\tuffroma           ";
const char *opcodeUVFromArray = "\tuvfroma           ";
const char *opcodeUMFromArray = "\tumfroma           ";
const char *opcodeUSFromArray = "\tusfroma           ";

////////////////////////////////////////////////////////////////////////
//	Constants

const char *constantLoopName = "1__$$__$$__$$__loop";
const char *constantBlockName = "1__$$__$$__$$__block";
const char *constantShaderMain = "1__$$__$$__$$__main";
const char *constantReturnValue = "__ReturnValue__";
const char *constantBug = "Compiler bug, please report";

////////////////////////////////////////////////////////////////////////
//	Coverage-guard aggregates (spec 011-jit-opcode-parity, FR-006)

// All 95 canonical opcode mnemonics, in definition order above. Entries
// are pointer-aliases to the opcodeXxx constants themselves, not re-typed
// literals, so this array is structurally incapable of drifting from
// their text.
const char *const kAllOpcodeMnemonics[] = {
    opcodeIf, opcodeElse, opcodeEndif, opcodeGatherHeader, opcodeGather,
    opcodeGatherElse, opcodeGatherEnd, opcodeFor, opcodeBeginfor, opcodeEndfor,
    opcodeIlluminance, opcodeBeginIlluminance, opcodeEndIlluminance,
    opcodeSolar, opcodeEndSolar, opcodeIlluminate, opcodeEndIlluminate,
    opcodeBreak, opcodeContinue, opcodeReturn,
    opcodeFloatEqual, opcodeVectorEqual, opcodeStringEqual, opcodeMatrixEqual,
    opcodeFloatNotEqual, opcodeVectorNotEqual, opcodeStringNotEqual, opcodeMatrixNotEqual,
    opcodeFloatELess, opcodeVectorELess,
    opcodeFloatLess, opcodeVectorLess,
    opcodeFloatEGreater, opcodeVectorEGreater,
    opcodeFloatGreater, opcodeVectorGreater,
    opcodeMatrixFromFloat, opcodeVectorFromFloat, opcodeMatrixFromVector,
    opcodeVectorFrom, opcodeColorFrom, opcodePointFrom, opcodeMatrixFrom,
    opcodeAnd, opcodeOr, opcodeXor, opcodeNXor, opcodeNot,
    opcodeNegFloat, opcodeNegVector, opcodeNegMatrix,
    opcodeDotProduct, opcodeCrossProduct,
    opcodeDivFloatFloat, opcodeDivVectorVector, opcodeDivMatrixMatrix,
    opcodeMulFloatFloat, opcodeMulVectorVector, opcodeMulMatrixMatrix,
    opcodeMulMatrixPoint, opcodeMulMatrixNormal, opcodeMulMatrixVector,
    opcodeMulPointMatrix, opcodeMulNormalMatrix, opcodeMulVectorMatrix,
    opcodeAddFloatFloat, opcodeAddVectorVector, opcodeAddMatrixMatrix,
    opcodeSubFloatFloat, opcodeSubVectorVector, opcodeSubMatrixMatrix,
    opcodeVUFloat, opcodeVUVector, opcodeVUMatrix, opcodeVUString,
    opcodeMoveFloatFloat, opcodeMoveVectorVector, opcodeMoveStringString, opcodeMoveMatrixMatrix,
    opcodeMoveAFloatFloat, opcodeMoveAVectorVector, opcodeMoveAStringString, opcodeMoveAMatrixMatrix,
    opcodeFFromArray, opcodeFToArray, opcodeVFromArray, opcodeVToArray,
    opcodeMFromArray, opcodeMToArray, opcodeSFromArray, opcodeSToArray,
    opcodeUFFromArray, opcodeUVFromArray, opcodeUMFromArray, opcodeUSFromArray,
    nullptr
};

void stripOpcodeMnemonic(const char *raw, char *out, int outSize) {
    while (*raw == '\t' || *raw == ' ') ++raw;
    int i = 0;
    while (*raw && *raw != '\t' && *raw != ' ' && i < outSize - 1) {
        out[i++] = *raw++;
    }
    out[i] = '\0';
}

// Dead-opcode evidence, mirroring specs/011-jit-opcode-parity/triage-results.md
// (the original 11) plus 4 found during Phase 4's post-checkpoint revision
// (see tasks.md's revision note under T012/T013).
const char *const kDeadOpcodes[] = {
    // Matrix arithmetic -- type checker rejects matrix*point/normal/vector,
    // zero opcodeMul* emission sites in expression.cpp.
    "mulmp", "mulpm", "mulmn", "mulnm", "mulmv", "mulvm",
    // Comparison -- rslo.y hardcodes nullptr for the matrix comparison
    // opcode parameter on == and !=.
    "meql", "mneql",
    // Boolean -- zero call sites in src/libshader/compiler/; no xor/nxor
    // token in the lexer or grammar.
    "xor", "nxor",
    // GI -- zero call sites anywhere in the compiler, no grammar token
    // reaches it.
    "beginilluminance",
    // Data movement (array-element variants) -- zero call sites anywhere
    // in src/libshader/compiler/ for opcodeMoveAFloatFloat/AVectorVector/
    // AStringString/AMatrixMatrix; found during Phase 4's full opcodes.cpp
    // accounting, not in the original triage.
    "moveaff", "moveavv", "moveass", "moveamm",
    nullptr
};
