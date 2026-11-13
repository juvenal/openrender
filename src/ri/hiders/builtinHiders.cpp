/**
 * Project: openRender
 *
 * File: builtinHiders.cpp
 *
 * Description:
 *   The built-in hiders the process registry starts with, in the fixed order raytrace, reyes, zbuffer, photon. They are reached
 *   by the call graph (the renderer core -> hiderRegistry() -> registerBuiltinHiders() -> each getter), which is what keeps
 *   them in a plain static link (research R1, FR-006): no linker anchor is needed.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "hiderRegistry.h"

void registerBuiltinHiders(CHiderRegistry &registry) {
    registry.add(orHiderTableRaytrace(), "built-in raytrace");
    registry.add(orHiderTableReyes(), "built-in reyes");
    registry.add(orHiderTableZbuffer(), "built-in zbuffer");
    registry.add(orHiderTablePhoton(), "built-in photon");
}
