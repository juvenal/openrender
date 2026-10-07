/**
 * Project: openRender
 *
 * File: metrics.h
 *
 * Description:
 *   Renderer-core usage metric (FR-012, research R8): per class, the number
 *   of `CRenderer::<member>` / `CRendererContext::<member>` references and the
 *   number of distinct members referenced, counted over source text with
 *   comments and string/character literals removed. Definitions count.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef ARCHCHECK_METRICS_H
#define ARCHCHECK_METRICS_H

#include <set>
#include <string>
#include <string_view>

namespace archcheck {

    // The source with comments and the contents of string and character literals
    // (raw strings included) replaced by spaces; newlines are kept.
    std::string stripCommentsAndLiterals(std::string_view source);

    struct CoreUsage {
            long refs = 0;
            std::set<std::string> members; // "CRenderer::x", "CRendererContext::y"
    };

    // Adds the renderer-core references found in already-stripped text.
    void countCoreUsage(std::string_view stripped, CoreUsage &usage);

} // namespace archcheck

#endif // ARCHCHECK_METRICS_H
