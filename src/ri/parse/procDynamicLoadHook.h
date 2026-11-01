/**
 * Project: openRender
 *
 * File: procDynamicLoadHook.h
 *
 * Description:
 *   The Scene Interface's hook for expanding a RenderMan DynamicLoad procedural (spec 037, FR-022).
 *   RiProcDynamicLoad (parse/ri.cpp) loads nothing itself: it calls the hook the renderer core
 *   installed, so the Scene Interface depends on no module loader (Principle VIII). The core
 *   installs the hook in CRenderer::beginRenderer() and removes it in endRenderer() (the services
 *   pattern); with none installed RiProcDynamicLoad reports that there is no renderer.
 *
 *   `data` is the CDelayedData * the Procedural statement created; the hook reads its generator
 *   and helper strings. Defined in ri.cpp, which is compiled into ribRender, ribVector and ribOut.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef PROCDYNAMICLOADHOOK_H
#define PROCDYNAMICLOADHOOK_H

typedef void (*TProcDynamicLoadHook)(void *data, float detail);

void setProcDynamicLoadHook(TProcDynamicLoadHook hook); // nullptr uninstalls
TProcDynamicLoadHook procDynamicLoadHook();

#endif
