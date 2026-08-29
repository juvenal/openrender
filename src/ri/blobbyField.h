/**
 * Project: openRender
 *
 * File: blobbyField.h
 *
 * Description:
 *   This file defines the interface for blobbyField.
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
//  File				:	blobbyField.h
//  Classes				:	CBlobbyInstruction, CBlobbyProgram
//  Description			:	RiBlobby code-array validation and field
//							evaluation (spec 015-blobby-implicit-surfaces).
//
//							Pure: no renderer state is read or written by
//							the evaluator, which is what makes the field
//							unit-testable without a rendering context
//							(plan.md, Constitution Principle III).
//
////////////////////////////////////////////////////////////////////////
#ifndef BLOBBYFIELD_H
#define BLOBBYFIELD_H

#include "common/algebra.h"
#include "common/containers.h"
#include "common/global.h"

class CBlobbyRepeller;

///////////////////////////////////////////////////////////////////////
// The surface threshold (FR-015).
//
// Neither RISpec 3.2 5.6 nor PRMan Application Note #31 states the level
// at which the combined field defines the surface, so the value below is
// *derived* from two published figures rather than adopted from folklore.
// The commonly cited 0.5 is ruled out by the first constraint.
//
//  1. Application Note #31's coloured octahedron declares six unit-sphere
//     ellipsoid fields at +-0.89 on each axis, summed, and its figure
//     (vert.jpg) shows one merged blob whose per-blob colours blend across
//     every join. Two adjacent centres are 0.89*sqrt(2) = 1.2586 apart, so
//     the saddle between them sits 0.6293 from each and the summed field
//     there is exactly
//
//         2 * (1 - 0.6293^2)^3 = 2 * (1 - 0.396050)^3 = 0.4405883
//
//     The six lobes are one connected component if and only if the
//     threshold does not exceed that saddle value.  =>  T <= 0.4405883.
//
//  2. Application Note #31's figures.31/pairs.rib places nine pairs of
//     radius-2.5 ellipsoid fields, each pair summed, at decreasing
//     separations. In its figure (pairs.jpg) the pair separated by 3.24
//     renders as two distinct lobes while the pair separated by 3.00
//     renders joined. The summed field at each pair's midpoint is
//
//         d = 3.24  ->  2 * (1 - (1.62/2.5)^2)^3 = 0.3904523   (separate)
//         d = 3.00  ->  2 * (1 - (1.50/2.5)^2)^3 = 0.5242880   (joined)
//
//     =>  0.3904523 < T <= 0.5242880.
//
// Intersecting the two brackets gives 0.3904523 < T <= 0.4405883, and 0.4
// is the one round value inside it.
//
// Two independent photogrammetric checks against the note's own figures
// corroborate it rather than merely permitting it:
//
//  - figures.31/dent.rib's upper-left object is a lone unit-sphere
//    ellipsoid field at a known camera transform, so its rendered radius
//    is directly r = sqrt(1 - T^(1/3)). Sub-pixel coverage measurement of
//    dent.jpg gives a silhouette radius of 29.09 px against 29.37 px
//    predicted at T = 0.4 (30.06 at 0.38, 28.68 at 0.42) -- and the
//    plastic highlight biases that measurement small, i.e. towards a
//    slightly lower T.
//  - A silhouette fit of figures.31/blend.rib against blend.jpg peaks at
//    intersection-over-union 0.988 at 0.38 and 0.986 at 0.40, falling to
//    0.936 at 0.44.
//
// tests/unit/blobby/test_threshold_calibration.cpp asserts both brackets
// directly, so this constant cannot be changed without the derivation
// being re-checked. Not author-configurable (FR-015).
///////////////////////////////////////////////////////////////////////
#define BLOBBY_THRESHOLD 0.4f

///////////////////////////////////////////////////////////////////////
// Opcodes (RISpec 3.2 Tables 5.2 and 5.3)
///////////////////////////////////////////////////////////////////////
#define BLOBBY_OP_ADD 0
#define BLOBBY_OP_MULTIPLY 1
#define BLOBBY_OP_MAXIMUM 2
#define BLOBBY_OP_MINIMUM 3
#define BLOBBY_OP_45A 4 // subtract under "rispec", divide under "appnote"
#define BLOBBY_OP_45B 5 // divide under "rispec", subtract under "appnote"

// Both primary sources name subtract's two operands "subtrahend, minuend",
// which reads as though the *second* operand were the one subtracted from.
// AppNote #31's own dent.rib refutes that: it subtracts a small sphere
// (operand 1) from a large one (operand 0) and the figure shows the large
// sphere with a crater in it. Taking the documented naming literally would
// instead evaluate "small minus large", which barely crosses the threshold
// anywhere and could not produce that image. So subtract computes
//     operand0 - operand1
// and divide, by the same reading of its "dividend, divisor" naming,
// computes operand0 / operand1. The shared naming quirk is a documentation
// slip in both sources, not a behavioural difference between them.
#define BLOBBY_OP_NEGATE 6
#define BLOBBY_OP_IDENTITY 7

#define BLOBBY_OP_CONSTANT 1000
#define BLOBBY_OP_ELLIPSOID 1001
#define BLOBBY_OP_SEGMENT 1002
#define BLOBBY_OP_REPELLER 1003

///////////////////////////////////////////////////////////////////////
// Enum					:	EBlobbyOpcodeOrder
// Description			:	Which primary source's assignment of opcodes 4
//							and 5 to subtract/divide is in force.
// Comments				:	RISpec 3.2 Table 5.3 and AppNote #31 assign
//							them in opposite orders; both were read
//							verbatim, so this is a genuine contradiction
//							between the sources rather than a
//							transcription slip (FR-013, research-inputs.md).
//
//							AppNote #31's own example scene settles which
//							one PhotoRealistic RenderMan actually
//							implements, against its own table.
//							figures.31/dent.rib builds four blobbies from
//							the same two ellipsoid fields: the lower pair
//							combine them with opcode 2 and the upper pair
//							with opcode 4. In dent.jpg the lower pair are a
//							sphere with a bump and a sphere with a spike --
//							the unblended union `max` gives -- while the
//							upper pair are a sphere with a crater and a
//							sphere with a tunnel bored through it. Only
//							subtraction produces those. So opcode 4 is
//							subtract in the shipping renderer, matching
//							RISpec rather than the note's own Table.
//
//							Consequence for authors, which inverts the
//							obvious reading: RIB written against
//							PhotoRealistic RenderMan renders correctly
//							under BLOBBY_ORDER_RISPEC, the default.
//							BLOBBY_ORDER_APPNOTE exists for the narrower
//							case of RIB generated from the note's *table*
//							rather than from its examples.
///////////////////////////////////////////////////////////////////////
typedef enum {
    BLOBBY_ORDER_RISPEC = 0, // 4 = subtract, 5 = divide  (default)
    BLOBBY_ORDER_APPNOTE     // 4 = divide,   5 = subtract
} EBlobbyOpcodeOrder;

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyInstruction
// Description			:	One decoded instruction of the code array
// Comments				:	The instruction's ordinal position in the
//							program is the result reference later
//							instructions use (field-semantics.md 1).
///////////////////////////////////////////////////////////////////////
class CBlobbyInstruction {
    public:
        int opcode;       // As written in the code array
        int leafIndex;    // Ordinal among opcode >= 1000 instructions, or -1
        int numOperands;  // Number of entries in operands
        int *operands;    // floats/strings indices, or earlier instruction indices
        int resolvedOp;   // opcode with 4/5 mapped through the opcode order
};

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyProgram
// Description			:	Validated code array plus its float/string
//							pools; evaluates the combined field.
// Comments				:	Build-time only. Nothing here survives into
//							rendering (data-model.md "Lifetime overview").
///////////////////////////////////////////////////////////////////////
class CBlobbyProgram {
    public:
        CBlobbyProgram(int nleaf, int ncode, const int *code, int nfloats, const float *floats, int nstrings, const char *const *strings, EBlobbyOpcodeOrder order = BLOBBY_ORDER_RISPEC);
        ~CBlobbyProgram();

        // TRUE if the declaration validated well enough to render. A
        // program that validated but declares no primitive fields is
        // valid and simply produces no geometry (FR-030).
        int isValid() const { return valid; }

        int getNumLeaves() const { return numLeaves; }

        // The nleaf the author wrote, which need not agree with the
        // count above: Pixar's own hand example declares 21 and emits
        // 22. Per-blob parameter reads clamp to the shorter of the two,
        // so a mismatch cannot read past the end of a primvar array
        // (FR-017).
        int getDeclaredLeaves() const { return declaredLeaves; }
        int getNumInstructions() const { return numInstructions; }
        const CBlobbyInstruction *getInstruction(int i) const { return instructions + i; }

        // Cheap entry point: field value and analytic gradient only. The
        // continuation walk calls this millions of times, so it must not
        // pay the O(numLeaves) weight write (data-model.md 2).
        float evaluate(const float *P, float *gradient = NULL) const;

        // Expensive entry point: additionally fills leafWeights[numLeaves]
        // with each primitive field's normalized share of the result.
        // Called only where a vertex is actually emitted.
        float evaluateWeights(const float *P, float *gradient, float *leafWeights) const;

        // Union of every primitive field's bounded support, in the
        // primitive's object space (FR-028). Consumed by the extraction
        // walk and by the default cell size.
        void getExtent(float *bmin, float *bmax) const;

        // TRUE when some field has no bounded support (a repeller), so the
        // extent above is not a containment guarantee.
        int hasUnboundedField() const { return unbounded; }

        // TRUE when at least one primitive field bounds the extent.
        int hasBoundedExtent() const { return extentValid; }

        // The smallest half-extent of any single bounded primitive field,
        // or zero when there is none. A bounding box says nothing about how
        // thin the surface inside it is, so this is what the default cell
        // size uses to avoid under-resolving a long thin shape (FR-025).
        float getSmallestFieldSize() const { return smallestField; }

        // Object-space centre of primitive field `leaf`, used to seed the
        // extraction walk. Returns FALSE for fields with no natural centre
        // (constant, repeller), which contribute no seed.
        int getLeafSeed(int leaf, float *P) const;

    private:
        void decode(int ncode, const int *code);
        void resolveOpcodeOrder();
        void computeExtent();

        float evaluateInternal(const float *P, float *gradient, float *leafWeights) const;

        CBlobbyInstruction *instructions;
        int *operandPool;
        int numInstructions;
        int numLeaves;
        int declaredLeaves;
        float *floats;
        int nfloats;
        char **strings;
        int nstrings;
        EBlobbyOpcodeOrder opcodeOrder;
        int valid;
        int unbounded;
        int extentValid;
        float extentMin[3];
        float extentMax[3];
        float smallestField;

        // One per opcode-1003 instruction, indexed by instruction number
        // (NULL for every other instruction).
        CBlobbyRepeller **repellers;

        // Per-instruction inverse of the 4x4 an ellipsoid or segment field
        // carries, and a flag for the ones whose matrix is singular and so
        // contribute no field. Both are indexed by instruction number and
        // computed once at construction, so evaluation does no work that
        // depends only on the declaration.
        float *inverses;
        int *singular;

        // Scratch buffers sized at construction so evaluation allocates
        // nothing per call.
        mutable float *scratchValue;
        mutable float *scratchGradient;
        mutable float *scratchWeights;
};

// The spherical bump F(R) = (1-R^2)^3 shared by the ellipsoid and segment
// fields, expressed in terms of R^2 so no square root is needed.
float blobbyBump(float r2);

// Normalisation of the segment field's convolution integral: the integral
// of the bump along an infinite line through its centre,
//     N = integral of (1-u^2)^3 du over [-1,1] = 32/35,
// so that a long segment's on-axis field is exactly 1, matching an
// ellipsoid's value at its centre. Dividing by the segment radius as well
// makes the field scale-invariant, so a segment behaves like an ellipsoid
// under a uniform scale.
//
// Verified against AppNote #31's own 480-segment toroidal spiral: a
// forward render of figures.31/segspiral.rib under this field at
// BLOBBY_THRESHOLD reproduces segspiral.jpg's silhouette with an
// intersection-over-union of 0.981 and within 0.4% of its covered area --
// a thin-tube figure, where tube radius dominates coverage, so the
// normalisation is what that agreement is testing.
#define BLOBBY_SEGMENT_NORM (32.0f / 35.0f)

// d/d(R^2) of the above: -3*(1-R^2)^2, zero outside the unit radius.
float blobbyBumpDerivative(float r2);

#endif
