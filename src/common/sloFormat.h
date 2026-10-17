/**
 * Project: openRender
 *
 * File: sloFormat.h
 *
 * Description:
 *   The version of the .slo (LLVM bitcode) shader format, separate from the
 *   .rslo one in rsloFormat.h. `oshader --jit` records it in the named metadata
 *   node `openrender.slo.format` (three integer operands) and the JIT loader
 *   checks it before it adds or links a module. The rule is the one documented
 *   in rsloFormat.h.
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

#define SLO_FORMAT_MAJOR 2
#define SLO_FORMAT_MINOR 0
#define SLO_FORMAT_PATCH 0

#endif
