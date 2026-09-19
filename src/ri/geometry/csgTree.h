/**
 * Project: openRender
 *
 * File: csgTree.h
 *
 * Description:
 *   This file defines the interface for csgTree.
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
//  File				:	csgTree.h
//  Classes				:	CSGTreeNode
//  Description			:	Build-time RiSolidBegin/RiSolidEnd capture tree
//
////////////////////////////////////////////////////////////////////////
#ifndef CSGTREE_H
#define CSGTREE_H

#include "common/containers.h"

class CObject;
class CXform;
class CRendererContext;

// The four recognized SolidBegin operation-type strings (FR-001)
enum ECSGOperation {
    CSG_PRIMITIVE = 0,
    CSG_UNION,
    CSG_INTERSECTION,
    CSG_DIFFERENCE
};

///////////////////////////////////////////////////////////////////////
// Class				:	CSGTreeNode
// Description			:	Exists only while a solid block is open;
//							discarded once resolved into a Resolved
//							Solid Boundary (CSolidObject) at the
//							outermost RiSolidEnd.
// Comments				:
class CSGTreeNode {
    public:
        CSGTreeNode(ECSGOperation operation, CSGTreeNode *parent);
        ~CSGTreeNode();

        ECSGOperation operation;         // Set from the SolidBegin operation-type string
        CArray<CSGTreeNode *> *operands; // Boolean-node children, in declaration order (unused on Primitive nodes)
        CObject *leafObjects;            // Primitive-node captured objects, chained via CObject::sibling
        CXform *outerXform;               // Root-node only: the outermost SolidBegin's local frame
        CSGTreeNode *parent;              // Enables nested-block validity checks; NULL on the root
};

// FR-019: report a clear diagnostic when opening a nested SolidBegin/SolidEnd
// block directly inside a "primitive" leaf (a CSG leaf cannot itself hold a
// sub-tree). Always safe to call; only a "primitive" parent produces an error.
void csgValidateNestedSolidBegin(CSGTreeNode *parent);

// FR-021: report a clear diagnostic that an RiProcedural (or other
// delayed/procedural primitive) cannot be captured directly inside an open
// solid block, since it does not exist yet at SolidEnd time and cannot be
// tessellated for CSG boundary resolution. `node` must be non-NULL (callers
// only invoke this while a solid block is actually open). Always returns
// FALSE.
int csgValidateProceduralCapture(CSGTreeNode *node);

// Resolves a just-closed root CSGTreeNode (research.md Decision 1) into a
// Resolved Solid Boundary and adds it to the scene via context->addObject(),
// or adds nothing for an empty solid block (FR-016). Consumes and deletes
// `node` (and its whole still-open sub-tree, if any) unconditionally.
void resolveCSGTree(CRendererContext *context, CSGTreeNode *node);

#endif
