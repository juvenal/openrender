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
#include "atomic.h"
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

// Below this magnitude a divisor counts as zero. A blobby field is exactly
// zero outside its own support, so this guard has to hold over regions, not
// merely at isolated points.
#define BLOBBY_DIVIDE_EPSILON 1e-6f

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
    smallestField = 0;
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
    smallestField = 0;

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

        // The thinnest direction of this field's own box: for a segment
        // that is its radius, for a squashed ellipsoid its shortest
        // semi-axis. Taking the minimum over fields gives the finest
        // feature the surface can have.
        float thinnest = (bmax[0] - bmin[0]) * 0.5f;

        for (int k = 1; k < 3; k++) {
            const float half = (bmax[k] - bmin[k]) * 0.5f;

            if (half < thinnest)
                thinnest = half;
        }

        if (thinnest > 0 && (smallestField <= 0 || thinnest < smallestField))
            smallestField = thinnest;
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
// Primitive-field evaluation
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Function				:	evaluateEllipsoid
// Description			:	Opcode 1001: the spherical bump taken in the
//							space the instruction's 4x4 carries the unit
//							sphere out of.
// Comments				:	The point is carried back through the inverse
//							and the gradient chains forward through that
//							same inverse's transpose. A singular matrix has
//							no inverse, so the field simply contributes
//							nothing rather than being an error.
///////////////////////////////////////////////////////////////////////
static float evaluateEllipsoid(const float *Minv, const float *P, float *gradient) {
    vector q;

    mulmp(q, Minv, P);

    const float r2 = dotvv(q, q);
    const float value = blobbyBump(r2);

    if (gradient != NULL) {
        const float scale = 2 * blobbyBumpDerivative(r2);
        vector dq;

        initv(dq, scale * q[0], scale * q[1], scale * q[2]);
        chainGradient(Minv, dq, gradient);
    }

    return value;
}

///////////////////////////////////////////////////////////////////////
// Function				:	evaluateSegment
// Description			:	Opcode 1002: the convolution of a segment
//							impulse with the same spherical bump.
// Comments				:	A convolution rather than a bump of the
//							distance to the segment, and that is not a
//							stylistic choice. Segments laid end to end and
//							*summed* must reconstruct exactly the field of
//							the single longer segment, or every joint
//							bulges -- and AppNote #31's own 480-segment
//							spiral does precisely that, sharing endpoints
//							and combining them with opcode 0. A distance
//							bump would give roughly double the field at a
//							joint.
//
//							In the segment's own space, with the point at
//							axial coordinate s and perpendicular distance
//							h, substituting u = (t - s)/r turns the
//							integral into a polynomial in u over the
//							intersection of the segment and the kernel:
//
//							  F = [G(u)]/N, G(u) = k^6 u - k^4 u^3
//							                     + 3/5 k^2 u^5 - u^7/7
//							  k^2 = 1 - (h/r)^2
//
//							Differentiating under the integral with the
//							*t* limits held fixed -- they are 0 and L,
//							independent of the point -- avoids any boundary
//							term and gives the gradient in closed form too.
///////////////////////////////////////////////////////////////////////
static float evaluateSegment(const float *Minv, const float *base, const float *P, float *gradient) {
    const float *a = base;
    const float *b = base + 3;
    const float radius = base[6];
    vector q;

    if (gradient != NULL)
        initv(gradient, 0);

    if (radius <= 0)
        return 0;

    mulmp(q, Minv, P);

    vector axis;
    subvv(axis, b, a);

    const float length = sqrtf(dotvv(axis, axis));
    vector w;

    subvv(w, q, a);

    // A zero-length segment is a deliberate special case, not a limit: the
    // convolution integral vanishes with the length, so the limit would be
    // an empty field, while the useful behaviour -- and the one the spec
    // asks for -- is a sphere of the declared radius (US3 scenario 3). The
    // field is therefore discontinuous in length at exactly zero.
    if (length < C_EPSILON) {
        const float inv2 = 1 / (radius * radius);
        const float r2 = dotvv(w, w) * inv2;
        const float value = blobbyBump(r2);

        if (gradient != NULL) {
            const float scale = 2 * inv2 * blobbyBumpDerivative(r2);
            vector dq;

            initv(dq, scale * w[0], scale * w[1], scale * w[2]);
            chainGradient(Minv, dq, gradient);
        }

        return value;
    }

    vector e;
    const float invLength = 1 / length;

    initv(e, axis[0] * invLength, axis[1] * invLength, axis[2] * invLength);

    const float s = dotvv(w, e);
    float h2 = dotvv(w, w) - s * s;

    if (h2 < 0)
        h2 = 0;

    const float k2 = 1 - h2 / (radius * radius);

    if (k2 <= 0)
        return 0;

    const float k = sqrtf(k2);
    const float invRadius = 1 / radius;
    float u0 = -s * invRadius;
    float u1 = (length - s) * invRadius;

    if (u0 < -k)
        u0 = -k;
    if (u1 > k)
        u1 = k;

    if (u1 <= u0)
        return 0;

    const float k4 = k2 * k2;
    const float k6 = k4 * k2;

#define BLOBBY_SEGMENT_G(__u) (k6 * (__u) - k4 * (__u) * (__u) * (__u) + 0.6f * k2 * powf(__u, 5) - powf(__u, 7) / 7)
#define BLOBBY_SEGMENT_A(__u) (k4 * (__u) - (2.0f / 3.0f) * k2 * (__u) * (__u) * (__u) + powf(__u, 5) / 5)
#define BLOBBY_SEGMENT_B(__u) (-(k2 - (__u) * (__u)) * (k2 - (__u) * (__u)) * (k2 - (__u) * (__u)) / 6)

    const float value = (BLOBBY_SEGMENT_G(u1) - BLOBBY_SEGMENT_G(u0)) / BLOBBY_SEGMENT_NORM;

    if (gradient != NULL) {
        const float dA = BLOBBY_SEGMENT_A(u1) - BLOBBY_SEGMENT_A(u0);
        const float dB = BLOBBY_SEGMENT_B(u1) - BLOBBY_SEGMENT_B(u0);
        const float scale = -6 / (BLOBBY_SEGMENT_NORM * radius * radius);
        vector dq;

        for (int i = 0; i < 3; i++) {
            const float perpendicular = w[i] - s * e[i];

            dq[i] = scale * (perpendicular * dA - radius * e[i] * dB);
        }

        chainGradient(Minv, dq, gradient);
    }

#undef BLOBBY_SEGMENT_G
#undef BLOBBY_SEGMENT_A
#undef BLOBBY_SEGMENT_B

    return value;
}

///////////////////////////////////////////////////////////////////////
// Value blending
//
// Weights propagate up the code array alongside the field, in the same
// walk, because that is the only reading under which value blending and
// shape blending agree everywhere. A flat average weighted by field
// strength would bleed one finger's colour onto its neighbour in the
// reference hand, where two fingers overlap in *field* while the max that
// combines them means they do not overlap in *surface* (research
// Decision 6, US4 scenario 4).
//
// The contribution measure is max(value, 0) rather than the raw value: a
// negated or subtracted operand can be negative, and a raw-value
// apportionment would then produce weights outside [0,1] or blow up where
// the operands cancel. Clamping keeps the weights a partition of unity and
// stays continuous, since max(v,0) is.
///////////////////////////////////////////////////////////////////////
#define BLOBBY_WEIGHT_EPSILON 1e-12f

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Method				:	evaluateInternal
// Description			:	One walk of the code array
// Comments				:	Not re-entrant: the per-instruction scratch
//							buffers are members, sized once at
//							construction, so evaluation allocates nothing
//							per call. Extraction is single-threaded by
//							design (research Decision 3), which is what
//							makes that safe -- and the same decision is
//							what makes extraction deterministic, so the two
//							are not independent choices.
///////////////////////////////////////////////////////////////////////
float CBlobbyProgram::evaluateInternal(const float *P, float *gradient, float *leafWeights) const {
    if (gradient != NULL)
        initv(gradient, 0);

    if (!valid || numInstructions == 0) {
        if (leafWeights != NULL) {
            for (int i = 0; i < numLeaves; i++)
                leafWeights[i] = 0;
        }
        return 0;
    }

    atomicIncrement(&stats.numBlobbyFieldEvals);

    if (leafWeights != NULL)
        atomicIncrement(&stats.numBlobbyWeightedEvals);

    if (leafWeights != NULL && scratchWeights == NULL && numLeaves > 0)
        scratchWeights = new float[numInstructions * numLeaves];

    const int wantWeights = (leafWeights != NULL) && (scratchWeights != NULL) && (numLeaves > 0);

#define BLOBBY_VALUE(__i) scratchValue[__i]
#define BLOBBY_GRADIENT(__i) (scratchGradient + 3 * (__i))
#define BLOBBY_WEIGHTS(__i) (scratchWeights + numLeaves * (__i))

    for (int i = 0; i < numInstructions; i++) {
        const CBlobbyInstruction *instruction = instructions + i;
        float *value = scratchValue + i;
        float *grad = BLOBBY_GRADIENT(i);
        float *weights = wantWeights ? BLOBBY_WEIGHTS(i) : NULL;

        if (weights != NULL) {
            for (int k = 0; k < numLeaves; k++)
                weights[k] = 0;
        }

        if (instruction->opcode >= 1000) {
            switch (instruction->opcode) {
                case BLOBBY_OP_CONSTANT:
                    *value = floats[instruction->operands[0]];
                    initv(grad, 0);
                    break;

                case BLOBBY_OP_ELLIPSOID:
                    if (singular[i]) {
                        *value = 0;
                        initv(grad, 0);
                    }
                    else {
                        *value = evaluateEllipsoid(inverses + 16 * i, P, grad);
                    }
                    break;

                case BLOBBY_OP_SEGMENT:
                    if (singular[i]) {
                        *value = 0;
                        initv(grad, 0);
                    }
                    else {
                        *value = evaluateSegment(inverses + 16 * i, floats + instruction->operands[0], P, grad);
                    }
                    break;

                default: // BLOBBY_OP_REPELLER
                    if (repellers[i] != NULL) {
                        *value = repellers[i]->evaluate(P, grad);
                    }
                    else {
                        *value = 0;
                        initv(grad, 0);
                    }
                    break;
            }

            if (weights != NULL && instruction->leafIndex >= 0)
                weights[instruction->leafIndex] = 1;

            continue;
        }

        const int *operands = instruction->operands;
        const int count = instruction->numOperands;

        switch (instruction->resolvedOp) {
            case BLOBBY_OP_ADD:
            case BLOBBY_OP_MULTIPLY: {
                const int isAdd = (instruction->resolvedOp == BLOBBY_OP_ADD);
                float total = isAdd ? 0.0f : 1.0f;

                initv(grad, 0);

                if (isAdd) {
                    for (int k = 0; k < count; k++) {
                        const float *g = BLOBBY_GRADIENT(operands[k]);

                        total += BLOBBY_VALUE(operands[k]);

                        for (int c = 0; c < 3; c++)
                            grad[c] += g[c];
                    }
                }
                else {
                    // Product rule: the k-th term is that operand's own
                    // gradient scaled by the product of all the others.
                    for (int k = 0; k < count; k++)
                        total *= BLOBBY_VALUE(operands[k]);

                    for (int k = 0; k < count; k++) {
                        const float *g = BLOBBY_GRADIENT(operands[k]);
                        float others = 1;

                        for (int j = 0; j < count; j++) {
                            if (j != k)
                                others *= BLOBBY_VALUE(operands[j]);
                        }

                        for (int c = 0; c < 3; c++)
                            grad[c] += others * g[c];
                    }
                }

                *value = total;

                if (weights != NULL) {
                    float denominator = 0;

                    for (int k = 0; k < count; k++) {
                        const float contribution = BLOBBY_VALUE(operands[k]);

                        if (contribution > 0)
                            denominator += contribution;
                    }

                    if (denominator > BLOBBY_WEIGHT_EPSILON) {
                        for (int k = 0; k < count; k++) {
                            const float contribution = BLOBBY_VALUE(operands[k]);

                            if (contribution <= 0)
                                continue;

                            const float share = contribution / denominator;
                            const float *source = BLOBBY_WEIGHTS(operands[k]);

                            for (int c = 0; c < numLeaves; c++)
                                weights[c] += share * source[c];
                        }
                    }
                    else {
                        // FR-019a: where every contributing operand
                        // evaluates to zero there is no meaningful
                        // proportion, so split equally. Continuous, because
                        // it is only reached as every contribution goes to
                        // zero together.
                        const float share = 1.0f / (float)count;

                        for (int k = 0; k < count; k++) {
                            const float *source = BLOBBY_WEIGHTS(operands[k]);

                            for (int c = 0; c < numLeaves; c++)
                                weights[c] += share * source[c];
                        }
                    }
                }
                break;
            }

            case BLOBBY_OP_MAXIMUM:
            case BLOBBY_OP_MINIMUM: {
                const int wantMax = (instruction->resolvedOp == BLOBBY_OP_MAXIMUM);
                int winner = operands[0];

                // Strict comparison, so the first operand wins a tie. The
                // gradient is genuinely discontinuous along the seam where
                // two operands are equal -- that crease is what makes an
                // unblended union look unblended -- so what matters is not
                // smoothness but that the value and the gradient come from
                // the *same* operand (research Decision 4).
                for (int k = 1; k < count; k++) {
                    const float candidate = BLOBBY_VALUE(operands[k]);
                    const float current = BLOBBY_VALUE(winner);

                    if (wantMax ? (candidate > current) : (candidate < current))
                        winner = operands[k];
                }

                *value = BLOBBY_VALUE(winner);
                movvv(grad, BLOBBY_GRADIENT(winner));

                if (weights != NULL) {
                    const float *source = BLOBBY_WEIGHTS(winner);

                    for (int c = 0; c < numLeaves; c++)
                        weights[c] = source[c];
                }
                break;
            }

            case BLOBBY_RESOLVED_SUBTRACT: {
                // operand0 - operand1. Both primary sources name these
                // "subtrahend, minuend", which reads the other way round;
                // AppNote #31's own dent.rib subtracts a small blob
                // (operand 1) from a large one (operand 0) and its figure
                // shows the large blob cratered, so the naming is a shared
                // documentation slip (blobbyField.h).
                const float *ga = BLOBBY_GRADIENT(operands[0]);
                const float *gb = BLOBBY_GRADIENT(operands[1]);

                *value = BLOBBY_VALUE(operands[0]) - BLOBBY_VALUE(operands[1]);

                for (int c = 0; c < 3; c++)
                    grad[c] = ga[c] - gb[c];

                // The subtrahend carves the surface away; it contributes no
                // shading values to what remains (US4 scenario 5).
                if (weights != NULL) {
                    const float *source = BLOBBY_WEIGHTS(operands[0]);

                    for (int c = 0; c < numLeaves; c++)
                        weights[c] = source[c];
                }
                break;
            }

            case BLOBBY_RESOLVED_DIVIDE: {
                const float numerator = BLOBBY_VALUE(operands[0]);
                const float divisor = BLOBBY_VALUE(operands[1]);

                if (divisor > -BLOBBY_DIVIDE_EPSILON && divisor < BLOBBY_DIVIDE_EPSILON) {
                    // A blobby field is exactly zero outside its own
                    // support, so a divisor vanishes over whole regions
                    // rather than at isolated points. Returning zero there
                    // keeps the result defined and finite (FR-029); it is a
                    // discontinuity, but the alternative is an infinity
                    // that would propagate into every vertex position
                    // downstream of it.
                    *value = 0;
                    initv(grad, 0);
                }
                else {
                    const float *ga = BLOBBY_GRADIENT(operands[0]);
                    const float *gb = BLOBBY_GRADIENT(operands[1]);
                    const float inv = 1 / divisor;

                    *value = numerator * inv;

                    for (int c = 0; c < 3; c++)
                        grad[c] = (ga[c] * divisor - numerator * gb[c]) * inv * inv;
                }

                if (weights != NULL) {
                    const float *source = BLOBBY_WEIGHTS(operands[0]);

                    for (int c = 0; c < numLeaves; c++)
                        weights[c] = source[c];
                }
                break;
            }

            case BLOBBY_OP_NEGATE: {
                const float *ga = BLOBBY_GRADIENT(operands[0]);

                *value = -BLOBBY_VALUE(operands[0]);

                for (int c = 0; c < 3; c++)
                    grad[c] = -ga[c];

                // A negated operand contributes no values: it exists to
                // remove field, not to colour what is left.
                break;
            }

            default: { // BLOBBY_OP_IDENTITY
                *value = BLOBBY_VALUE(operands[0]);
                movvv(grad, BLOBBY_GRADIENT(operands[0]));

                if (weights != NULL) {
                    const float *source = BLOBBY_WEIGHTS(operands[0]);

                    for (int c = 0; c < numLeaves; c++)
                        weights[c] = source[c];
                }
                break;
            }
        }
    }

    // The last instruction's result is the primitive's field.
    const int last = numInstructions - 1;

    if (gradient != NULL)
        movvv(gradient, BLOBBY_GRADIENT(last));

    if (leafWeights != NULL) {
        if (wantWeights) {
            const float *source = BLOBBY_WEIGHTS(last);

            for (int c = 0; c < numLeaves; c++)
                leafWeights[c] = source[c];
        }
        else {
            for (int c = 0; c < numLeaves; c++)
                leafWeights[c] = 0;
        }
    }

    return BLOBBY_VALUE(last);

#undef BLOBBY_VALUE
#undef BLOBBY_GRADIENT
#undef BLOBBY_WEIGHTS
}
