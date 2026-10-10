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

#include <algorithm>
#include <vector>

#include "csgDefinition.h"
#include "common/diagnostics.h"
#include "object.h"
#include "xform.h"

#include <assert.h>
#include <math.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CSGTreeNode
// Method				:	CSGTreeNode
// Description			:	Ctor
// Return Value			:
// Comments				:
CSGTreeNode::CSGTreeNode(ECSGOperation operation, CSGTreeNode *parent) {
    this->operation = operation;
    this->parent = parent;

    operands = new CArray<CSGTreeNode *>;
    leafObjects = NULL;
    outerXform = NULL;
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
    if (parent->operation == CSG_PRIMITIVE)
        csgReportNestedSolidBegin();
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgReportNestedSolidBegin
// Description			:	FR-019, and a SolidBegin inside a procedural
//							operand's expansion (spec 021)
// Return Value			:
// Comments				:
void csgReportNestedSolidBegin() {
    error(CODE_BADTOKEN, "SolidBegin/SolidEnd cannot be nested inside a \"primitive\" solid block\n");
}

// A node's captured leaves, in declaration order (addObject() prepends)
static std::vector<CObject *> capturedLeaves(const CSGTreeNode *node) {
    std::vector<CObject *> leaves;
    for (CObject *o = node->leafObjects; o != NULL; o = o->sibling)
        leaves.push_back(o);
    std::reverse(leaves.begin(), leaves.end());
    return leaves;
}

// One operand holding leaves (template members; the definition attaches them)
static int addLeafOperand(CCSGDefinition *def, const std::vector<CObject *> &leaves) {
    const int operand = def->addOperand();
    for (CObject *leaf : leaves)
        def->addOperandMember(operand, leaf);
    return def->addLeaf(operand);
}

// Everything captured under a node that contributes no geometry: a block
// nested inside "primitive", diagnosed when it opened
static void discardSubtree(const CSGTreeNode *node) {
    CObject *next;
    for (CObject *o = node->leafObjects; o != NULL; o = next) {
        next = o->sibling;
        o->attach(); // captured unattached: this frees it
        o->detach();
    }
    for (int i = 0; i < node->operands->numItems; i++)
        discardSubtree(node->operands->array[i]);
}

static int buildNode(CCSGDefinition *def, const CSGTreeNode *node) {
    if (node->operation == CSG_PRIMITIVE) {
        for (int i = 0; i < node->operands->numItems; i++)
            discardSubtree(node->operands->array[i]);
        return addLeafOperand(def, capturedLeaves(node)); // An empty block is an empty operand (FR-025a)
    }

    const int n = def->addNode(node->operation);
    if (node->leafObjects != NULL) // Direct leaves form one operand, positioned first
        def->addChild(n, addLeafOperand(def, capturedLeaves(node)));
    for (int i = 0; i < node->operands->numItems; i++)
        def->addChild(n, buildNode(def, node->operands->array[i]));
    return n;
}

///////////////////////////////////////////////////////////////////////
// Function				:	buildCSGDefinition
// Description			:	See csgTree.h
// Return Value			:	The definition (unattached)
// Comments				:	The captured leaves move into the definition, which
//							rebases them into the outer frame (finalize)
CCSGDefinition *buildCSGDefinition(const CSGTreeNode *node) {
    assert(node != NULL);
    CCSGDefinition *def = new CCSGDefinition();
    def->setRoot(buildNode(def, node));
    def->finalize(node->outerXform);
    return def;
}

///////////////////////////////////////////////////////////////////////
// Function				:	freeCSGTree
// Description			:	See csgTree.h
// Return Value			:
// Comments				:
void freeCSGTree(CSGTreeNode *node) {
    for (int i = 0; i < node->operands->numItems; i++)
        freeCSGTree(node->operands->array[i]);
    if (node->outerXform != NULL)
        node->outerXform->detach();
    delete node;
}
