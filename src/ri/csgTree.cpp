/**
 * Project: openRender
 *
 * File: csgTree.cpp
 *
 * Description:
 *   This file contains the implementation of csgTree.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	csgTree.cpp
//  Classes				:	CSGTreeNode
//  Description			:	Build-time RiSolidBegin/RiSolidEnd capture tree
//
////////////////////////////////////////////////////////////////////////
#include "csgTree.h"
#include "error.h"
#include "object.h"
#include "rendererContext.h"
#include "solidObject.h"
#include "xform.h"

#include <assert.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CSGTreeNode
// Method				:	CSGTreeNode
// Description			:	Ctor
// Return Value			:
// Comments				:
CSGTreeNode::CSGTreeNode(ECSGOperation operation, CSGTreeNode *parent) {
    this->operation = operation;
    this->parent    = parent;

    operands    = new CArray<CSGTreeNode *>;
    leafObjects = NULL;
    outerXform  = NULL;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSGTreeNode
// Method				:	~CSGTreeNode
// Description			:	Dtor
// Return Value			:
// Comments				:
CSGTreeNode::~CSGTreeNode() {
    delete operands;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgValidateNestedSolidBegin
// Description			:	FR-019
// Return Value			:
// Comments				:
void csgValidateNestedSolidBegin(CSGTreeNode *parent) {
    if (parent->operation == CSG_PRIMITIVE) {
        error(CODE_BADTOKEN, "SolidBegin/SolidEnd cannot be nested inside a \"primitive\" solid block\n");
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgValidateProceduralCapture
// Description			:	FR-021
// Return Value			:
// Comments				:
int csgValidateProceduralCapture(CSGTreeNode *node) {
    assert(node != NULL);

    error(CODE_BADTOKEN, "RiProcedural cannot be declared inside a SolidBegin block\n");
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgResolveFragments
// Description			:	Recursively resolves `node` to its flat chain of
//							Boundary Fragments (CObject, chained via
//							CObject::sibling), or NULL for an empty node.
//							Phase 2 (T010) only implements the two trivial
//							shortcuts required for a single-primitive round
//							trip; real multi-operand boolean combination is
//							Phase 3 (US1, T017-T019).
// Return Value			:	The resolved fragment chain, or NULL if empty
// Comments				:
static CObject *csgResolveFragments(CSGTreeNode *node) {
    if (node->operation == CSG_PRIMITIVE)
        return node->leafObjects; // FR-002: raw geometry is already one opaque leaf operand

    // Boolean node ("union"/"intersection"/"difference"): collect this
    // node's own directly-captured leaf operand (if any) together with
    // each nested operand's already-resolved fragment chain, in
    // declaration order.
    CArray<CObject *> operandFragments;

    if (node->leafObjects != NULL)
        operandFragments.push(node->leafObjects);

    int i;
    for (i = 0; i < node->operands->numItems; i++)
        operandFragments.push(csgResolveFragments(node->operands->array[i]));

    if (operandFragments.numItems == 0)
        return NULL; // FR-016: empty solid block -> no geometry

    if (operandFragments.numItems == 1)
        return operandFragments.array[0]; // FR-017: single operand -> passthrough unchanged

    // Phase 3 (US1) implements the actual boolean kernel behind approved,
    // failing tests written first (T013-T016) per the TDD gate; it is not
    // reachable yet from any Phase 2 scenario.
    assert(FALSE && "CSG boolean combination (2+ operands) is not yet implemented");
    return NULL;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgRebaseFragments
// Description			:	Each captured leaf's `xform` is absolute (the
//							full local-to-world transform active when it
//							was declared). CSolidObject::intersect()/dice()
//							unconditionally re-composes every fragment's
//							xform with the container's own `outerXform` on
//							first use (processDelayedSolid), so a fragment
//							declared under a transform different from the
//							one snapshotted at SolidBegin time must first
//							be rebased into that outer frame here, or the
//							outer transform is applied twice.
// Return Value			:
// Comments				:	outerXform is shared (also held as the future
//							CSolidObject's own xform), so it is copied
//							before being inverted rather than inverted in
//							place.
static void csgRebaseFragments(CObject *fragments, CXform *outerXform) {
    if (fragments == NULL)
        return;

    CXform *outerInverse = new CXform(outerXform);
    outerInverse->invert();

    CObject *cObject;
    for (cObject = fragments; cObject != NULL; cObject = cObject->sibling) {
        CXform *rebased = new CXform(outerInverse);
        rebased->concat(cObject->xform);

        rebased->attach();
        cObject->xform->detach();
        cObject->xform = rebased;
    }

    delete outerInverse;
}

///////////////////////////////////////////////////////////////////////
// Function				:	resolveCSGTree
// Description			:	See csgTree.h
// Return Value			:
// Comments				:
void resolveCSGTree(CRendererContext *context, CSGTreeNode *node) {
    assert(node != NULL);
    assert(node->outerXform != NULL);

    CObject *fragments = csgResolveFragments(node);

    if (fragments != NULL) {
        csgRebaseFragments(fragments, node->outerXform);
        context->addObject(new CSolidObject(context->getAttributes(FALSE), node->outerXform, fragments));
    }

    node->outerXform->detach();
    delete node;
}
