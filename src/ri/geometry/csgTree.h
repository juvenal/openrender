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
class CCSGDefinition;

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
//							discarded once the outermost RiSolidEnd has
//							built its CCSGDefinition (spec 021).
// Comments				:
class CSGTreeNode {
    public:
        CSGTreeNode(ECSGOperation operation, CSGTreeNode *parent);
        ~CSGTreeNode();

        ECSGOperation operation;         // Set from the SolidBegin operation-type string
        CArray<CSGTreeNode *> *operands; // Boolean-node children, in declaration order (unused on Primitive nodes)
        CObject *leafObjects;            // Primitive-node captured objects, chained via CObject::sibling
        CXform *outerXform;              // Root-node only: the outermost SolidBegin's local frame
        CSGTreeNode *parent;             // Enables nested-block validity checks; NULL on the root
};

// FR-019: report a clear diagnostic when opening a nested SolidBegin/SolidEnd
// block directly inside a "primitive" leaf (a CSG leaf cannot itself hold a
// sub-tree). Always safe to call; only a "primitive" parent produces an error.
void csgValidateNestedSolidBegin(CSGTreeNode *parent);

// The nested-in-primitive diagnostic itself, for a caller with no parent node
// (a SolidBegin emitted while a procedural operand expands, spec 021)
void csgReportNestedSolidBegin();

///////////////////////////////////////////////////////////////////////
// Function				:	buildCSGDefinition
// Description			:	The persistent definition of the finished tree
//							rooted at node (spec 021): one operand per
//							"primitive" block, a boolean block's direct
//							leaves as its first operand, template leaves
//							rebased into node's outer frame. Blocks nested
//							inside "primitive" contribute nothing.
// Return Value			:	A new, unattached definition (isEmpty() when the
//							tree holds no operand)
// Comments				:	Pure geometry: the caller (the renderer core)
//							places the solid in the scene
CCSGDefinition *buildCSGDefinition(const CSGTreeNode *node);

// Free a capture tree (its nodes and the outer transform's reference); the
// leaves were moved into a definition or discarded by buildCSGDefinition()
void freeCSGTree(CSGTreeNode *node);

#endif
