/**
 * Project: openRender
 *
 * File: displayV1Adapter.h
 *
 * Description:
 *   The confined version 1 display adapter (spec 037, FR-006, FR-019, FR-020, FR-039, FR-043, FR-046;
 *   contracts/display-v1-adapter.md): wraps a version 1 driver module, or the three callbacks of
 *   RiCustomDisplay, as a display 2.0.0 plug-in table. The display host selects it in one place
 *   (CDisplayHost::bindPlugin); nothing else in the host knows a display MAJOR.
 *
 *   Through the adapter a version 1 driver is given values from per-display, host-owned mutable copies
 *   of the display's parameter snapshot, never pointers into renderer state; a lookup made outside the
 *   driver's displayStart answers "not found"; its displayData calls are serialized per display; no
 *   displayFinish follows a displayData that failed.
 *
 *   Each table returned is the caller's to give back with displayV1ReleasePlugin().
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef DISPLAYV1ADAPTER_H
#define DISPLAYV1ADAPTER_H

#include "dsply.h"
#include "ri/parse/ri.h" // RtDisplayStartFunction, RtDisplayDataFunction, RtDisplayFinishFunction

// A version 2 table over the version 1 module `osHandle` loaded from `path`; NULL if the module lacks
// displayStart, displayData or displayFinish.
const OrDisplayPlugin *displayV1PluginForModule(void *osHandle, const char *path);

// A version 2 table over the RiCustomDisplay callbacks; NULL if one is missing.
const OrDisplayPlugin *displayV1PluginForCustom(RtDisplayStartFunction start, RtDisplayDataFunction data, RtDisplayFinishFunction finish);

// Gives back a table either function returned.
void displayV1ReleasePlugin(const OrDisplayPlugin *plugin);

#endif
