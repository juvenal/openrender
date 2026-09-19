/**
 * Project: openRender
 *
 * File: subdivisionHierarchical.cpp
 *
 * Description:
 *   This file implements the hierarchical subdivision mesh override list
 *   (RiHierarchicalSubdivisionMesh, FR-007/FR-008/FR-009).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "subdivisionHierarchical.h"

#include <cstdlib>
#include <cstring>

///////////////////////////////////////////////////////////////////////
// Function				:	cloneHierarchicalOverrides
// Description			:	Deep-copies an override list (used by
//                          CSubdivMesh::instantiate(), which owns a fresh
//                          copy of every other per-mesh array as well)
// Return Value			:	The head of the cloned list
// Comments				:
CHierarchicalOverride *cloneHierarchicalOverrides(const CHierarchicalOverride *overrides) {
    CHierarchicalOverride *head = NULL;
    CHierarchicalOverride **tail = &head;

    for (const CHierarchicalOverride *src = overrides; src != NULL; src = src->next) {
        CHierarchicalOverride *dest = new CHierarchicalOverride;

        dest->faceIndex = src->faceIndex;
        dest->level = src->level;
        dest->tagName = strdup(src->tagName);
        dest->value = src->value;
        dest->next = NULL;

        *tail = dest;
        tail = &dest->next;
    }

    return head;
}

///////////////////////////////////////////////////////////////////////
// Function				:	deleteHierarchicalOverrides
// Description			:	Frees an override list previously built by
//                          RiHierarchicalSubdivisionMeshV or cloned via
//                          cloneHierarchicalOverrides()
// Return Value			:	-
// Comments				:
void deleteHierarchicalOverrides(CHierarchicalOverride *overrides) {
    while (overrides != NULL) {
        CHierarchicalOverride *next = overrides->next;

        free(overrides->tagName);
        delete overrides;

        overrides = next;
    }
}
