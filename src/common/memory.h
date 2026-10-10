/**
 * Project: openRender
 *
 * File: memory.h
 *
 * Description:
 *   This file defines the interface for memory.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	memory.h
//  Classes				:	-
//  Description			:	This file defines the misc. memory interface routines
//
////////////////////////////////////////////////////////////////////////
#ifndef MEMORY_H
#define MEMORY_H

#include "global.h"
#include "os.h"

#include <string.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CMemPage
// Description			:	This class coltains memory that's allocated on the fly
// Comments				:
class CMemPage {
    public:
        char *memory;      // Points to the current free memory
        char *base;        // Points to the base memory
        int availableSize; // The available number of bytes
        int totalSize;     // The total size of the block
        CMemPage *next;    // Points to the next free memory block
        CMemPage *prev;    // points to the previous valid memory block
};

void memoryInit(CMemPage *&);      // Init named memory stack
void memoryTini(CMemPage *&);      // Destroy the named allocated memory
CMemPage *memoryNewPage(int);      // Allocate a new memory page
void memoryDeletePage(CMemPage *); // Allocate a new memory page

// This macro allocates memory in the named stack
inline void *ralloc(int size, CMemPage *&stack) {

    // Align the size with 8 byte boundaries
    size = (size + 7) & (~7);

    while (stack->availableSize < size) {
        if (stack->next == NULL) {
            CMemPage *cPage = memoryNewPage(size);
            cPage->prev = stack;
            stack->next = cPage;
        }

        stack = stack->next;
        stack->availableSize = stack->totalSize;
        stack->memory = stack->base;
    }

    void *ptr = stack->memory;
    stack->memory = stack->memory + size;
    stack->availableSize -= size;
    return ptr;
}

// This macro duplicates a string in the stack
inline char *rstrdup(const char *string, CMemPage *&stack) {
    char *dest = (char *)ralloc((int)strlen(string) + 1, stack);

    strcpy(dest, string);

    return dest;
}

// This macro places a checkpoint
#define memBegin(__page)                            \
    {                                               \
        char *savedMem = __page->memory;            \
        int savedAvailable = __page->availableSize; \
        CMemPage *savedPage = __page;

// This macro restores the memory to the last checkpoint
// It is important that the scope between the matching begin-end
// pairs mist not be exitted
#define memEnd(__page)                      \
    __page = savedPage;                     \
    __page->availableSize = savedAvailable; \
    __page->memory = savedMem;              \
    }

// This structure can be used to put a checkpoint
typedef struct {
        CMemPage *stack;
        int availableSize;
        char *memory;
} TMemCheckpoint;

// Mem save and mem restore does the same thing, but they explicitly store the checkpoint
#define memSave(__data, __stack)                   \
    __data.memory = __stack->memory;               \
    __data.availableSize = __stack->availableSize; \
    __data.stack = __stack;

#define memRestore(__data, __stack)                \
    __stack = __data.stack;                        \
    __stack->availableSize = __data.availableSize; \
    __stack->memory = __data.memory;

///////////////////////////////////////////////////////////////////////
// Class				:	CArena
// Description			:	A stack-like scratch-memory arena: a chain of
//							CMemPage pages with nested begin/end regions
//							(spec 023). globalArena, below, is the one
//							process-wide instance.
// Comments				:	constexpr-constructed, so a global CArena is
//							constant-initialized (no static-init ordering).
class CArena {
    public:
        constexpr CArena() : top(nullptr) {}

        CMemPage *top; // The current page of the stack

        void init() { memoryInit(top); }
        void tini() { memoryTini(top); }
};

inline void *ralloc(int size, CArena &arena) { return ralloc(size, arena.top); }
inline char *rstrdup(const char *string, CArena &arena) { return rstrdup(string, arena.top); }

#define arenaBegin(__arena) memBegin((__arena).top)
#define arenaEnd(__arena) memEnd((__arena).top)
#define arenaSave(__data, __arena) memSave(__data, (__arena).top)
#define arenaRestore(__data, __arena) memRestore(__data, (__arena).top)

// The process-wide scratch arena (replaces CRenderer::globalMemory). It has a
// single definition, in libRIBCommon; init()/tini() run where the renderer
// and the RI front ends begin and end a session.
extern CArena globalArena;

#endif
