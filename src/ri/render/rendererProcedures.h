/**
 * Project: openRender
 *
 * File: rendererProcedures.h
 *
 * Description:
 *   The renderer core's side of the DynamicLoad hook (spec 037, FR-022, FR-024): the expansion of a
 *   Procedural "DynamicLoad" through the Procedures module. rendererProceduresInit() installs it in
 *   the Scene Interface (procDynamicLoadHook.h) and is called from CRenderer::beginRenderer();
 *   rendererProceduresShutdown() removes it and drops any frame reference still held, from endRenderer().
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef RENDERERPROCEDURES_H
#define RENDERERPROCEDURES_H

void rendererProceduresInit();
void rendererProceduresShutdown();

#endif
