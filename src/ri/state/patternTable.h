/**
 * Project: openRender
 *
 * File: patternTable.h
 *
 * Description:
 *   The scoped pattern-node table of an attribute block (spec 025, FR-013 to
 *   FR-015): a map from a handle string to a ShaderGroupRef, so that a later
 *   spec can name a node once and refer to it from a slot.
 *
 *   Scope comes from value semantics. AttributeBegin clones the block, the
 *   clone defines into its own copy of the table, and AttributeEnd drops the
 *   clone, so a definition is visible in the block that made it and in every
 *   block cloned from it later, and is gone with the scope that made it. A
 *   node bound into a slot stays alive through that slot's own reference.
 *
 *   Copying the table shares its storage (O(1)); define() copies the storage
 *   first when it is shared (copy-on-write). The check relies on attribute
 *   blocks being modified only on the single RIB-stream thread, which is how
 *   the Ri calls drive them.
 *
 *   State never interprets a node (FR-015) and writes no diagnostic: lookup()
 *   of an undefined handle simply returns nullptr. Nothing in spec 025
 *   consumes the table.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef PATTERNTABLE_H
#define PATTERNTABLE_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "stateHandles.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPatternTable
// Description			:	Handle -> node, in first-definition order, with unique handles
// Comments				:
class CPatternTable {
    public:
        // Defines handle. An existing entry with the same handle is replaced in place (its
        // position is kept); otherwise the entry is appended.
        void define(const char *handle, ShaderGroupRef node) {
            if (!entries)
                entries = std::make_shared<std::vector<SEntry>>();
            else if (entries.use_count() > 1)
                entries = std::make_shared<std::vector<SEntry>>(*entries); // copy on write

            for (SEntry &entry : *entries) {
                if (entry.handle == handle) {
                    entry.node = std::move(node);
                    return;
                }
            }
            entries->push_back(SEntry{handle, std::move(node)});
        }

        // The node defined under handle, or nullptr. No diagnostic.
        const ShaderGroupRef *lookup(const char *handle) const {
            if (!entries)
                return nullptr;
            for (const SEntry &entry : *entries) {
                if (entry.handle == handle)
                    return &entry.node;
            }
            return nullptr;
        }

        // Calls fn(handle, node) for each entry, in definition order.
        template <class F>
        void forEach(F &&fn) const {
            if (!entries)
                return;
            for (const SEntry &entry : *entries)
                fn(entry.handle, entry.node);
        }

        int size() const { return entries ? static_cast<int>(entries->size()) : 0; }

    private:
        struct SEntry {
                std::string handle;
                ShaderGroupRef node;
        };

        std::shared_ptr<std::vector<SEntry>> entries; // shared between copies until one defines
};

#endif
