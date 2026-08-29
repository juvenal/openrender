/**
 * Project: openRender
 *
 * File: blobbyField.cpp
 *
 * Description:
 *   This file implements the functionality for blobbyField.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	blobbyField.cpp
//  Classes				:	CBlobbyProgram
//  Description			:	Code-array validation and field evaluation
//
////////////////////////////////////////////////////////////////////////
#include "blobbyField.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "blobbyRepeller.h"
#include "error.h"
#include "stats.h"

///////////////////////////////////////////////////////////////////////
// How many floats each primitive-field opcode consumes at its float
// index, and how many operands the instruction itself carries.
///////////////////////////////////////////////////////////////////////
#define BLOBBY_WIDTH_CONSTANT 1
#define BLOBBY_WIDTH_ELLIPSOID 16
#define BLOBBY_WIDTH_SEGMENT 23
#define BLOBBY_WIDTH_REPELLER 4

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyBump
// Description			:	The spherical bump F(R) = (1-R^2)^3
// Comments				:	Takes R^2 so callers never need a square root.
///////////////////////////////////////////////////////////////////////
float blobbyBump(float r2) {
    if (r2 >= 1)
        return 0;

    const float t = 1 - r2;

    return t * t * t;
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyBumpDerivative
// Description			:	dF/d(R^2) = -3 (1-R^2)^2
///////////////////////////////////////////////////////////////////////
float blobbyBumpDerivative(float r2) {
    if (r2 >= 1)
        return 0;

    const float t = 1 - r2;

    return -3 * t * t;
}

///////////////////////////////////////////////////////////////////////
// Function				:	chainGradient
// Description			:	Carry a gradient taken in a primitive field's
//							own space back to object space through the
//							transpose of the inverse that took the point
//							the other way.
// Comments				:	dF/dP_j = sum_i Minv(i,j) dF/dq_i
///////////////////////////////////////////////////////////////////////
static void chainGradient(const float *Minv, const float *dq, float *dP) {
    for (int j = 0; j < 3; j++) {
        dP[j] = Minv[element(0, j)] * dq[0] + Minv[element(1, j)] * dq[1] + Minv[element(2, j)] * dq[2];
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	rowLength
// Description			:	Length of row `row` of a 4x4's linear part.
// Comments				:	This is the exact half-extent of the
//							transformed unit sphere along axis `row`.
///////////////////////////////////////////////////////////////////////
static float rowLength(const float *m, int row) {
    const float a = m[element(row, 0)];
    const float b = m[element(row, 1)];
    const float c = m[element(row, 2)];

    return sqrtf(a * a + b * b + c * c);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	CBlobbyProgram
// Description			:	Ctor -- copies the pools, decodes and validates
//							the code array, then computes the field extent.
///////////////////////////////////////////////////////////////////////
CBlobbyProgram::CBlobbyProgram(int nleaf, int ncode, const int *code, int nf, const float *f, int ns, const char *const *s, EBlobbyOpcodeOrder order) {
    instructions = NULL;
    operandPool = NULL;
    numInstructions = 0;
    numLeaves = 0;
    declaredLeaves = nleaf;
    floats = NULL;
    nfloats = 0;
    strings = NULL;
    nstrings = 0;
    opcodeOrder = order;
    valid = TRUE;
    unbounded = FALSE;
    extentValid = FALSE;
    repellers = NULL;
    inverses = NULL;
    singular = NULL;
    scratchValue = NULL;
    scratchGradient = NULL;
    scratchWeights = NULL;

    initv(extentMin, 0);
    initv(extentMax, 0);

    // Defensive against a caller that hands us negative lengths: the
    // arrays are author data arriving through the RIB parser, and a
    // malformed declaration must produce a diagnostic, never a wild read.
    if (ncode < 0 || nf < 0 || ns < 0) {
        error(CODE_CONSISTENCY, "Blobby: negative array length (ncode %d, nfloats %d, nstrings %d)\n", ncode, nf, ns);
        valid = FALSE;
        return;
    }

    if (nf > 0 && f != NULL) {
        nfloats = nf;
        floats = new float[nf];
        memcpy(floats, f, sizeof(float) * nf);
    }

    if (ns > 0 && s != NULL) {
        nstrings = ns;
        strings = new char *[ns];
        for (int i = 0; i < ns; i++)
            strings[i] = strdup(s[i] == NULL ? "" : s[i]);
    }

    decode(ncode, code);

    if (valid) {
        resolveOpcodeOrder();

        // FR-017: Pixar's own published hand example declares 21 leaves
        // while emitting 22, so real RIB contains this. Diagnose it and
        // keep going with the count the code array actually shows; per-blob
        // parameter reads clamp to the shorter of the two arrays.
        if (declaredLeaves != numLeaves) {
            warning(CODE_CONSISTENCY, "Blobby: nleaf declares %d primitive field(s) but the code array contains %d; using %d\n", declaredLeaves, numLeaves, numLeaves);
        }

        computeExtent();

        scratchValue = new float[numInstructions > 0 ? numInstructions : 1];
        scratchGradient = new float[(numInstructions > 0 ? numInstructions : 1) * 3];
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	~CBlobbyProgram
// Description			:	Dtor
///////////////////////////////////////////////////////////////////////
CBlobbyProgram::~CBlobbyProgram() {
    if (repellers != NULL) {
        for (int i = 0; i < numInstructions; i++) {
            if (repellers[i] != NULL)
                delete repellers[i];
        }
        delete[] repellers;
    }

    if (strings != NULL) {
        for (int i = 0; i < nstrings; i++)
            free(strings[i]);
        delete[] strings;
    }

    if (floats != NULL)
        delete[] floats;
    if (instructions != NULL)
        delete[] instructions;
    if (operandPool != NULL)
        delete[] operandPool;
    if (inverses != NULL)
        delete[] inverses;
    if (singular != NULL)
        delete[] singular;
    if (scratchValue != NULL)
        delete[] scratchValue;
    if (scratchGradient != NULL)
        delete[] scratchGradient;
    if (scratchWeights != NULL)
        delete[] scratchWeights;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	decode
// Description			:	Walk the code array once, decoding and
//							validating each instruction in turn.
// Comments				:	One forward pass suffices because a combining
//							instruction may only reference results that
//							come before it (FR-006), so every reference is
//							already decoded by the time it is checked.
//							Any rejection stops decoding: past the first
//							malformed instruction the instruction
//							boundaries are no longer known, so continuing
//							would report invented problems.
///////////////////////////////////////////////////////////////////////
void CBlobbyProgram::decode(int ncode, const int *code) {
    if (ncode == 0 || code == NULL) {
        // Not an error: an empty declaration yields no geometry (FR-030).
        numInstructions = 0;
        numLeaves = 0;
        return;
    }

    // Every instruction occupies at least two ints, and the operand pool
    // can never be larger than the code array itself.
    instructions = new CBlobbyInstruction[ncode / 2 + 1];
    operandPool = new int[ncode];
    repellers = new CBlobbyRepeller *[ncode / 2 + 1];
    inverses = new float[16 * (ncode / 2 + 1)];
    singular = new int[ncode / 2 + 1];

    for (int i = 0; i < ncode / 2 + 1; i++) {
        repellers[i] = NULL;
        singular[i] = FALSE;
    }

    int pos = 0;
    int poolUsed = 0;

    while (pos < ncode) {
        const int opcode = code[pos];
        const int index = numInstructions;
        int numOperands;
        int firstOperand = pos + 1;

        // ---- how long is this instruction? --------------------------
        switch (opcode) {
            case BLOBBY_OP_ADD:
            case BLOBBY_OP_MULTIPLY:
            case BLOBBY_OP_MAXIMUM:
            case BLOBBY_OP_MINIMUM:
                if (pos + 1 >= ncode) {
                    error(CODE_CONSISTENCY, "Blobby: instruction %d (opcode %d) is truncated: its operand count is missing\n", index, opcode);
                    valid = FALSE;
                    return;
                }

                numOperands = code[pos + 1];
                firstOperand = pos + 2;

                if (numOperands <= 0) {
                    error(CODE_CONSISTENCY, "Blobby: instruction %d (opcode %d) declares an operand count of %d; it must be greater than zero\n", index, opcode, numOperands);
                    valid = FALSE;
                    return;
                }

                if (numOperands > ncode - firstOperand) {
                    error(CODE_CONSISTENCY, "Blobby: instruction %d (opcode %d) declares %d operands but only %d remain in the code array\n", index, opcode, numOperands, ncode - firstOperand);
                    valid = FALSE;
                    return;
                }
                break;

            case BLOBBY_OP_45A:
            case BLOBBY_OP_45B:
                numOperands = 2;
                break;

            case BLOBBY_OP_NEGATE:
            case BLOBBY_OP_IDENTITY:
                numOperands = 1;
                break;

            case BLOBBY_OP_CONSTANT:
            case BLOBBY_OP_ELLIPSOID:
            case BLOBBY_OP_SEGMENT:
                numOperands = 1;
                break;

            case BLOBBY_OP_REPELLER:
                numOperands = 2;
                break;

            default:
                // FR-014: everything else, including the ranges both
                // tables reserve for future use (8..99 and 1004..1099).
                error(CODE_BADTOKEN, "Blobby: unknown opcode %d at instruction %d\n", opcode, index);
                valid = FALSE;
                return;
        }

        if (firstOperand + numOperands > ncode) {
            error(CODE_CONSISTENCY, "Blobby: instruction %d (opcode %d) is truncated: it needs %d operand(s) but only %d remain in the code array\n", index, opcode, numOperands, ncode - firstOperand);
            valid = FALSE;
            return;
        }

        // ---- record it ----------------------------------------------
        CBlobbyInstruction *instruction = instructions + index;

        instruction->opcode = opcode;
        instruction->resolvedOp = opcode;
        instruction->leafIndex = -1;
        instruction->numOperands = numOperands;
        instruction->operands = operandPool + poolUsed;

        for (int k = 0; k < numOperands; k++)
            operandPool[poolUsed + k] = code[firstOperand + k];

        poolUsed += numOperands;
        numInstructions++;
        pos = firstOperand + numOperands;

        // ---- validate its operands ----------------------------------
        if (opcode >= 1000) {
            // FR-016: the leaf index is the ordinal among primitive-field
            // instructions, counting all four types.
            instruction->leafIndex = numLeaves++;

            int floatIndex = instruction->operands[0];
            int width = BLOBBY_WIDTH_CONSTANT;

            if (opcode == BLOBBY_OP_ELLIPSOID)
                width = BLOBBY_WIDTH_ELLIPSOID;
            else if (opcode == BLOBBY_OP_SEGMENT)
                width = BLOBBY_WIDTH_SEGMENT;

            if (opcode == BLOBBY_OP_REPELLER) {
                const int stringIndex = instruction->operands[0];

                floatIndex = instruction->operands[1];
                width = BLOBBY_WIDTH_REPELLER;

                if (stringIndex < 0 || stringIndex >= nstrings) {
                    error(CODE_CONSISTENCY, "Blobby: instruction %d (opcode 1003) names string %d, but the strings array holds %d\n", index, stringIndex, nstrings);
                    valid = FALSE;
                    return;
                }
            }

            if (floatIndex < 0 || floatIndex > nfloats - width) {
                error(CODE_CONSISTENCY, "Blobby: instruction %d (opcode %d) reads floats %d..%d, past the end of the %d-float array\n", index, opcode, floatIndex, floatIndex + width - 1, nfloats);
                valid = FALSE;
                return;
            }

            // Cache what evaluation needs so the hot path does no work
            // that depends only on the declaration.
            if (opcode == BLOBBY_OP_ELLIPSOID || opcode == BLOBBY_OP_SEGMENT) {
                const float *m = floats + floatIndex + (opcode == BLOBBY_OP_SEGMENT ? 7 : 0);

                // invertm() returns TRUE when the matrix is singular. A
                // singular ellipsoid contributes no field rather than
                // being an error (edge case, data-model.md 2).
                singular[index] = invertm(inverses + 16 * index, m);
            }

            if (opcode == BLOBBY_OP_REPELLER) {
                const float *shape = floats + floatIndex;

                repellers[index] = new CBlobbyRepeller(strings[instruction->operands[0]], shape[0], shape[1], shape[2], shape[3]);
            }
        }
        else {
            for (int k = 0; k < numOperands; k++) {
                const int reference = instruction->operands[k];

                if (reference < 0 || reference >= index) {
                    error(CODE_CONSISTENCY, "Blobby: instruction %d (opcode %d) references result %d, which is not an earlier instruction\n", index, opcode, reference);
                    valid = FALSE;
                    return;
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	resolveOpcodeOrder
// Description			:	Map opcodes 4 and 5 onto subtract/divide once,
//							here, rather than branching per evaluation
//							point (research Decision 10).
///////////////////////////////////////////////////////////////////////
#define BLOBBY_RESOLVED_SUBTRACT (-2)
#define BLOBBY_RESOLVED_DIVIDE (-3)

void CBlobbyProgram::resolveOpcodeOrder() {
    for (int i = 0; i < numInstructions; i++) {
        CBlobbyInstruction *instruction = instructions + i;

        if (instruction->opcode == BLOBBY_OP_45A) {
            instruction->resolvedOp = (opcodeOrder == BLOBBY_ORDER_RISPEC) ? BLOBBY_RESOLVED_SUBTRACT : BLOBBY_RESOLVED_DIVIDE;
        }
        else if (instruction->opcode == BLOBBY_OP_45B) {
            instruction->resolvedOp = (opcodeOrder == BLOBBY_ORDER_RISPEC) ? BLOBBY_RESOLVED_DIVIDE : BLOBBY_RESOLVED_SUBTRACT;
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	computeExtent
// Description			:	Union of every primitive field's bounded
//							support, in the primitive's object space.
// Comments				:	Two fields have no bounded support and the rule
//							for each is decided here once, because this
//							value is consumed in two places downstream and
//							produced nowhere else:
//
//							  - a constant field has no spatial support at
//							    all, so it contributes nothing;
//							  - a repelling ground plane is unbounded
//							    across its plane, so it contributes nothing
//							    either but raises `unbounded`, which tells
//							    the extraction walk the extent is not a
//							    containment guarantee.
//
//							A singular ellipsoid likewise contributes no
//							field and so no extent.
///////////////////////////////////////////////////////////////////////
void CBlobbyProgram::computeExtent() {
    extentValid = FALSE;
    unbounded = FALSE;

    for (int i = 0; i < numInstructions; i++) {
        const CBlobbyInstruction *instruction = instructions + i;

        if (instruction->opcode < 1000)
            continue;

        float bmin[3], bmax[3];

        if (instruction->opcode == BLOBBY_OP_CONSTANT) {
            continue;
        }
        else if (instruction->opcode == BLOBBY_OP_REPELLER) {
            unbounded = TRUE;
            continue;
        }
        else if (instruction->opcode == BLOBBY_OP_ELLIPSOID) {
            if (singular[i])
                continue;

            const float *m = floats + instruction->operands[0];

            for (int k = 0; k < 3; k++) {
                const float half = rowLength(m, k);

                bmin[k] = m[element(k, 3)] - half;
                bmax[k] = m[element(k, 3)] + half;
            }
        }
        else { // BLOBBY_OP_SEGMENT
            if (singular[i])
                continue;

            const float *base = floats + instruction->operands[0];
            const float *m = base + 7;
            const float radius = base[6];
            vector a, b;

            mulmp(a, m, base);
            mulmp(b, m, base + 3);

            for (int k = 0; k < 3; k++) {
                // The transformed ball of radius `radius` is an ellipsoid
                // whose half-extent along axis k is radius * |row k|.
                const float half = radius * rowLength(m, k);

                bmin[k] = (a[k] < b[k] ? a[k] : b[k]) - half;
                bmax[k] = (a[k] > b[k] ? a[k] : b[k]) + half;
            }
        }

        if (!extentValid) {
            movvv(extentMin, bmin);
            movvv(extentMax, bmax);
            extentValid = TRUE;
        }
        else {
            addBox(extentMin, extentMax, bmin);
            addBox(extentMin, extentMax, bmax);
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	getExtent
// Description			:	The field extent (FR-028)
///////////////////////////////////////////////////////////////////////
void CBlobbyProgram::getExtent(float *bmin, float *bmax) const {
    movvv(bmin, extentMin);
    movvv(bmax, extentMax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	getLeafSeed
// Description			:	Object-space centre of a primitive field, used
//							to seed the extraction walk.
// Return Value			:	FALSE for fields with no natural centre.
// Comments				:	A constant has no position, and a repeller's
//							surface does not enclose a point of its own --
//							both are reached by continuation from a
//							neighbouring field's seed instead.
///////////////////////////////////////////////////////////////////////
int CBlobbyProgram::getLeafSeed(int leaf, float *P) const {
    for (int i = 0; i < numInstructions; i++) {
        const CBlobbyInstruction *instruction = instructions + i;

        if (instruction->leafIndex != leaf)
            continue;

        if (instruction->opcode == BLOBBY_OP_ELLIPSOID) {
            if (singular[i])
                return FALSE;

            const float *m = floats + instruction->operands[0];

            initv(P, m[element(0, 3)], m[element(1, 3)], m[element(2, 3)]);
            return TRUE;
        }

        if (instruction->opcode == BLOBBY_OP_SEGMENT) {
            if (singular[i])
                return FALSE;

            const float *base = floats + instruction->operands[0];
            const float *m = base + 7;
            vector a, b;

            mulmp(a, m, base);
            mulmp(b, m, base + 3);

            initv(P, (a[0] + b[0]) * 0.5f, (a[1] + b[1]) * 0.5f, (a[2] + b[2]) * 0.5f);
            return TRUE;
        }

        return FALSE;
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	evaluate
// Description			:	Field value and analytic gradient
///////////////////////////////////////////////////////////////////////
float CBlobbyProgram::evaluate(const float *P, float *gradient) const {
    return evaluateInternal(P, gradient, NULL);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	evaluateWeights
// Description			:	As evaluate(), plus each leaf's normalized share
///////////////////////////////////////////////////////////////////////
float CBlobbyProgram::evaluateWeights(const float *P, float *gradient, float *leafWeights) const {
    return evaluateInternal(P, gradient, leafWeights);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	evaluateInternal
// Description			:	One walk of the code array (T029-T030, T044-T046,
//							T052-T053, T059)
///////////////////////////////////////////////////////////////////////
float CBlobbyProgram::evaluateInternal(const float *, float *gradient, float *) const {
    if (gradient != NULL)
        initv(gradient, 0);

    return 0;
}
