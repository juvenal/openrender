/**
 * Project: openRender
 *
 * File: subdivisionHierarchical.h
 *
 * Description:
 *   This file defines the interface for the hierarchical subdivision mesh
 *   override list (RiHierarchicalSubdivisionMesh, FR-007/FR-008/FR-009).
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
//  File				:	subdivisionHierarchical.h
//  Classes				:	CHierarchicalOverride
//  Description			:	One (face, level, tag, value) override layered on top of a
//                          base CSubdivMesh's default tags, never replacing the base
//                          mesh's own tag storage (data-model.md, Hierarchical
//                          Subdivision Edit entity).
//
////////////////////////////////////////////////////////////////////////
#ifndef SUBDIVHIERARCHICAL_H
#define SUBDIVHIERARCHICAL_H

///////////////////////////////////////////////////////////////////////
// Class				:	CHierarchicalOverride
// Description			:	Singly-linked list node; one override = one
//                          (faceIndex, level, tagName, value) tuple
// Comments				:
struct CHierarchicalOverride {
    int faceIndex;
    int level;
    char *tagName;
    float value;
    CHierarchicalOverride *next;
};

CHierarchicalOverride *cloneHierarchicalOverrides(const CHierarchicalOverride *overrides);
void deleteHierarchicalOverrides(CHierarchicalOverride *overrides);

#endif
