/**
 * Project: openRender
 *
 * File: csgDefinition.cpp
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
#include "csgDefinition.h"

#include "object.h"

// Spec 021 Phase 2 skeleton: storage and builder calls only. Kind
// derivation, flips, bounds, collapse and rebasing arrive with T030.

CCSGResolvedOperand::CCSGResolvedOperand() {
    kind = CSG_OPERAND_EMPTY;
    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);
}

void CCSGResolvedOperand::addMember(CObject *member) {
    members.push_back(member);
}

void CCSGResolvedOperand::addSurface(CSurface *surface, int member) {
    surfaces.push_back(surface);
    surfaceMember.push_back(member);
}

void deriveOperandKind(CCSGResolvedOperand &operand) {
    operand.kind = CSG_OPERAND_EMPTY;
}

CCSGDefinition::CCSGDefinition() {
    root = -1;
    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);
}

CCSGDefinition::~CCSGDefinition() {
    for (std::vector<CObject *> &members : operandMembers)
        for (CObject *member : members)
            member->detach();
}

int CCSGDefinition::addOperand() {
    operandMembers.emplace_back();
    leafFlip.push_back(0);
    return (int)operandMembers.size() - 1;
}

void CCSGDefinition::addOperandMember(int operand, CObject *member) {
    member->attach();
    operandMembers[operand].push_back(member);
}

int CCSGDefinition::addLeaf(int operand) {
    Node n;
    n.operation = CSG_PRIMITIVE;
    n.operand = operand;
    nodes.push_back(n);
    return (int)nodes.size() - 1;
}

int CCSGDefinition::addNode(ECSGOperation operation) {
    Node n;
    n.operation = operation;
    n.operand = -1;
    nodes.push_back(n);
    return (int)nodes.size() - 1;
}

void CCSGDefinition::addChild(int node, int child) {
    nodes[node].children.push_back(child);
}

void CCSGDefinition::setRoot(int node) {
    root = node;
}

void CCSGDefinition::finalize(CXform *) {
}

int CCSGDefinition::operandFlip(int operand) const {
    return leafFlip[operand];
}

int CCSGDefinition::isEmpty() const {
    return FALSE;
}

int CCSGDefinition::passthroughOperand() const {
    return -1;
}
