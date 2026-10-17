/**
 * Project: openRender
 *
 * File: rsloFormat.h
 *
 * Description:
 *   The version of the .rslo (interpreter bytecode) text format. The compiler
 *   writes it in the `#!version` line and the loaders require the major and
 *   minor numbers to match, so a file from an older format is refused with a
 *   diagnostic and never misread.
 *
 *   Format 1.1 (spec 039, Phase 4): every stochastic instruction (random,
 *   urandom, trace, transmission, visibility, gatherHeader, occlusion,
 *   indirectdiffuse, illuminance, texture, environment, shadow) carries its
 *   call-site ordinal as a trailing operand. Format 1.0 had no such operand.
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

#define RSLO_FORMAT_MAJOR 1
#define RSLO_FORMAT_MINOR 1
#define RSLO_FORMAT_PATCH 0

#endif
