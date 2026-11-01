/**
 * Project: openRender
 *
 * File: orplugin.hpp
 *
 * Description:
 *   C++20 convenience over orplugin.h (spec 037, FR-010). Not required: a plug-in can be written in
 *   C17 from orplugin.h alone.
 *
 *   orplugin::makeTable<Table>(abiVersion) returns a function table with its header filled in and
 *   every entry point null. The table is byte-identical to a hand-written C initializer of the same
 *   table (US6-3; PluginAbi_Convention compares the two with memcmp), so the table types declare the
 *   header as their first member, named `header`.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef ORPLUGIN_HPP
#define ORPLUGIN_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "orplugin.h"

namespace orplugin {

    enum class Status : int {
        Ok = OR_STATUS_OK,
        Error = OR_STATUS_ERROR,
        Unsupported = OR_STATUS_UNSUPPORTED,
        BadArgument = OR_STATUS_BAD_ARGUMENT,
        OutOfMemory = OR_STATUS_OUT_OF_MEMORY,
        Version = OR_STATUS_VERSION,
        TableSize = OR_STATUS_TABLE_SIZE
    };

    template <class Table>
    constexpr Table makeTable(uint32_t abiVersion) noexcept {
        static_assert(std::is_standard_layout_v<Table> && std::is_trivially_copyable_v<Table>, "a plug-in table is a plain C struct");
        static_assert(offsetof(Table, header) == 0, "a plug-in table starts with OrTableHeader");
        Table t{}; // every entry point null until set
        t.header.structSize = static_cast<uint32_t>(sizeof(Table));
        t.header.abiVersion = abiVersion;
        return t;
    }

} // namespace orplugin

#endif
