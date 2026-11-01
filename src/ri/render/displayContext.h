/**
 * Project: openRender
 *
 * File: displayContext.h
 *
 * Description:
 *   The host's definition of the opaque OrDisplayContext handle a display driver is given (spec 037,
 *   data-model 4.2). Core-private: the display host (displayHost.cpp) and the version 1 adapter
 *   (displayV1Adapter.cpp) include it; a driver sees only the incomplete type of dsply.h.
 *
 *   `plugin` is the table this display was bound to, so the adapter's entry points (which receive
 *   only the context) find the version 1 entry points they forward to.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef DISPLAYCONTEXT_H
#define DISPLAYCONTEXT_H

#include "dsply.h"

struct OrDisplayContext {
        const OrDisplayPlugin *plugin;
};

#endif
