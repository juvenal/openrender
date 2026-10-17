/**
 * Project: openRender
 *
 * File: rsloFormat.h
 *
 * Description:
 *   The version of the .rslo (interpreter bytecode) text format. The compiler
 *   writes it in the `#!version` line and the loaders check it, so a file from an
 *   incompatible format is refused with a diagnostic and never misread. The
 *   .slo format has its own version (sloFormat.h) and follows the same rule.
 *
 *   The rule (semantic versioning, see formatVersion.h):
 *     MAJOR  a loader of another major cannot read the file: the operand layout,
 *            the JIT ABI or an op_* signature changed. Older loaders cannot read
 *            newer files and newer loaders cannot read older ones.
 *     MINOR  additive: a file may use more than an older one, but everything an
 *            older file uses still means what it did. A loader reads a file when
 *            file.major == supported.major and file.minor <= supported.minor.
 *     PATCH  no effect on the format; never checked.
 *
 *   History:
 *     1.0  the format before spec 039.
 *     2.0  spec 039, Phase 4: every stochastic instruction (random, urandom,
 *          trace, transmission, visibility, gatherHeader, occlusion,
 *          indirectdiffuse, illuminance, texture, environment, shadow) carries
 *          its call-site ordinal as a trailing operand, and the JIT entries
 *          that take one are the `_v2` ones. A MAJOR change.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef RSLO_FORMAT_H
#define RSLO_FORMAT_H

#define RSLO_FORMAT_MAJOR 2
#define RSLO_FORMAT_MINOR 0
#define RSLO_FORMAT_PATCH 0

#include <stdio.h>

#include "common/formatVersion.h"

// The text of the refusal of a file whose `#!version` line says
// fileMajor.fileMinor.filePatch: what the file is, what this loader reads, and
// the remedy.
inline void rsloFormatMismatchText(char *buffer, size_t size, int fileMajor, int fileMinor, int filePatch) {
    snprintf(buffer, size,
             "Version mismatch: this file is .rslo format %d.%d.%d, this renderer reads format %d.%d.%d "
             "(same major, minor up to %d); recompile the shader with oshader",
             fileMajor, fileMinor, filePatch, RSLO_FORMAT_MAJOR, RSLO_FORMAT_MINOR, RSLO_FORMAT_PATCH, RSLO_FORMAT_MINOR);
}

#endif
