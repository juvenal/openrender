/**
 * Project: openRender
 *
 * File: csgDefinition.h
 *
 * Description:
 *   Immutable, shared CSG definition: the template tree a solid
 *   keeps for the whole frame (spec 021).
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
//  File				:	csgDefinition.h
//  Classes				:	CCSGDefinition, CCSGResolvedOperand
//  Description			:	The persistent template of one solid: its boolean
//							tree over operands, shared by every instance
//							(spec 021, data-model.md)
//
////////////////////////////////////////////////////////////////////////
#ifndef CSGDEFINITION_H
#define CSGDEFINITION_H

#include <mutex>
#include <vector>

#include "common/algebra.h"
#include "common/containers.h"
#include "common/global.h"
#include "csgTree.h"
#include "refCounter.h"

class CObject;
class CSurface;
class CShadingContext;
class CXform;

// What an operand contributes, decided per instance after procedural
// expansion and mesh materialization (FR-021, FR-025a)
enum ECSGOperandKind {
    CSG_OPERAND_EMPTY = 0,      // Encloses no volume: the empty set in its position
    CSG_OPERAND_SINGLE_QUADRIC, // One quadric: analytic, implied-closed classification
    CSG_OPERAND_MULTI_SURFACE,  // Several members, or one closed non-quadric: crossing parity
    CSG_OPERAND_OPEN            // One non-quadric that is not closed: empty, plus a warning
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGResolvedOperand
// Description			:	One operand as resolved for an instance: its
//							declared primitives (members), every surface they
//							consist of, and the kind derived from them
// Comments				:	Closedness is judged per member, not per surface:
//							a mesh splits into many surfaces (one per patch,
//							span or face), so each surface records its member.
class CCSGResolvedOperand {
    public:
        CCSGResolvedOperand();

        void addMember(CObject *member);                 // A declared primitive (not owned)
        void addSurface(CSurface *surface, int member);  // One of member's surfaces (not owned)

        std::vector<CObject *> members;
        std::vector<CSurface *> surfaces;
        std::vector<int> surfaceMember; // Index into members, per surface
        int kind;                       // ECSGOperandKind
        vector bmin, bmax;              // Camera-space bound of the surfaces

        // Every surface whose bound passes `meets` (a test on a camera-space
        // box), and every unbounded surface. Uses the bound hierarchy that
        // buildIndex() makes, or visits all surfaces before that.
        template <class Meets, class Visit> void forEachSurface(Meets meets, Visit visit) const {
            if (index.empty()) {
                for (const CSurface *surface : surfaces)
                    visit(surface);
                return;
            }
            for (int s : unbounded)
                visit(surfaces[s]);
            int stack[64];
            int top = 0;
            stack[top++] = 0;
            while (top > 0) {
                const CIndexNode &node = index[stack[--top]];
                if (!meets(node.bmin, node.bmax))
                    continue;
                if (node.count > 0) {
                    for (int k = node.first; k < node.first + node.count; k++) {
                        const CSurface *surface = surfaces[order[k]];
                        if (meets(surfaceBound[order[k]].bmin, surfaceBound[order[k]].bmax))
                            visit(surface);
                    }
                }
                else { // buildNodes() caps the depth, so the stack cannot overflow
                    stack[top++] = node.child;
                    stack[top++] = node.child + 1;
                }
            }
        }

        // The bound hierarchy over the bounded surfaces (called by deriveOperandKind)
        void buildIndex();

    private:
        struct CBound {
                vector bmin, bmax;
        };
        struct CIndexNode {
                vector bmin, bmax;
                int child;        // Interior: the first of two adjacent children
                int first, count; // Leaf: a range of `order` (count > 0)
        };
        void buildNodes();

        std::vector<CIndexNode> index;
        std::vector<CBound> surfaceBound; // Per surface, copied at buildIndex()
        std::vector<int> order;           // Bounded surfaces, grouped by leaf
        std::vector<int> unbounded;       // Surfaces with an empty (unbounded) box
};

// Derive operand.kind (and its bound) from its members and surfaces: the
// FR-021 closedness rules. Pure: no shading context, no side effects beyond
// the operand itself.
void deriveOperandKind(CCSGResolvedOperand &operand);

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGDefinition
// Description			:	Immutable, shared template of one solid: a
//							flattened boolean tree whose leaves reference
//							operands (lists of template members)
// Comments				:	Built once at the outermost RiSolidEnd (or directly
//							by tests through the same builder calls), then
//							finalize()d and never modified.
class CCSGDefinition : public CRefCounter {
    public:
        CCSGDefinition();
        ~CCSGDefinition();

        // Building
        int addOperand();                                  // New empty operand; returns its index
        void addOperandMember(int operand, CObject *member); // Template member (attached)
        int addLeaf(int operand);                          // Tree node standing for an operand
        int addNode(ECSGOperation operation);              // Interior tree node
        void addChild(int node, int child);                // Append a child, in declaration order
        void setRoot(int node);
        void finalize(CXform *outerXform);                 // Flips, bound, rebasing; outerXform may be NULL

        // Queries
        int numOperands() const { return (int)operandMembers.size(); }
        int operandFlip(int operand) const;   // Static orientation parity of an operand's leaves
        int isEmpty() const;                  // The whole tree yields no solid (spec 013 FR-016)
        int passthroughOperand() const;       // The single operand a collapsed tree yields, or -1

        // Evaluate the tree given each operand's membership (1 = IN, 0 = OUT).
        // membership(operand) is called lazily, only for operands that can
        // still change the result (short-circuit evaluation).
        template <class F> int evaluate(F membership) const { return evaluateNode(root, membership); }

        struct Node {
                int operation;             // ECSGOperation (interior nodes)
                int operand;               // Operand index for a leaf, -1 otherwise
                std::vector<int> children; // Child node indices, in declaration order
        };

        std::vector<Node> nodes;
        std::vector<std::vector<CObject *>> operandMembers;
        std::vector<int> leafFlip; // Per operand
        int root;
        vector bmin, bmax;         // Template bound, in the outer frame

        std::vector<float> operandBmin, operandBmax; // Per operand template bound (3 floats each), in the outer frame

        // TRUE the first time a given warning is due for operand in frame
        // (frame serial, CStats::csgFrame), so every instance of the solid
        // shares one warning per offending operand per frame
        enum EWarning { WARNING_UNSUPPORTED = 0, WARNING_NOT_CLOSED, NUM_WARNINGS };
        int firstWarning(EWarning warning, int operand, int frame);

    private:
        std::mutex warningMutex;
        std::vector<int> warnedFrame[NUM_WARNINGS]; // Per operand: the last frame warned, + 1

        template <class F> int evaluateNode(int node, F &membership) const;
        void markFlips(int node, int parity);
        void nodeBound(int node, float *lo, float *hi) const;
        int hasLeaves(int node) const;
};

template <class F> int CCSGDefinition::evaluateNode(int node, F &membership) const {
    const Node &n = nodes[node];
    if (n.operand >= 0)
        return membership(n.operand) ? 1 : 0;

    const int count = (int)n.children.size();
    switch (n.operation) {
    case CSG_UNION:
        for (int i = 0; i < count; i++)
            if (evaluateNode(n.children[i], membership))
                return 1;
        return 0;
    case CSG_INTERSECTION:
        for (int i = 0; i < count; i++)
            if (!evaluateNode(n.children[i], membership))
                return 0;
        return count > 0;
    case CSG_DIFFERENCE:
        if (count == 0 || !evaluateNode(n.children[0], membership))
            return 0;
        for (int i = 1; i < count; i++)
            if (evaluateNode(n.children[i], membership))
                return 0;
        return 1;
    default: // CSG_PRIMITIVE: a single operand wrapped in a node
        return count > 0 ? evaluateNode(n.children[0], membership) : 0;
    }
}

#endif // CSGDEFINITION_H
