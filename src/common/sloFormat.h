/**
 * Project: openRender
 *
 * File: sloFormat.h
 *
 * Description:
 *   The version of the .slo (LLVM bitcode) shader format, separate from the
 *   .rslo one in rsloFormat.h and following the same rule (semantic versioning,
 *   see formatVersion.h): a loader reads a module when its MAJOR equals the
 *   loader's and its MINOR is not greater than the loader's; the PATCH is never
 *   checked. A MAJOR change is one an older or newer loader cannot survive: the
 *   op_* / rsl_* C ABI the module calls, or the layout it expects.
 *
 *   `oshader --jit` records the version in the named metadata node
 *   `openrender.slo.format`, one node of three i32 operands (major, minor,
 *   patch). The JIT loader checks it before it adds or links a module; a module
 *   without the node (written before the format had a version) counts as 1.0.0.
 *   The unresolved-symbol failure at link time stays as a backstop for a module
 *   of the current format whose calls no longer resolve.
 *
 *   History:
 *     1.0  the format before spec 039 (no version record).
 *     2.0  spec 039, Phase 4: the stochastic runtime entries are the `_v2` ones,
 *          which take the call-site ordinal as a last argument. A MAJOR change.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SLO_FORMAT_H
#define SLO_FORMAT_H

#include "common/formatVersion.h"

#define SLO_FORMAT_MAJOR 2
#define SLO_FORMAT_MINOR 0
#define SLO_FORMAT_PATCH 0

// Named metadata node that holds the version.
#define SLO_FORMAT_KEY "openrender.slo.format"

#endif
