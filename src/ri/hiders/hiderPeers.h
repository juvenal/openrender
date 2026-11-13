/**
 * Project: openRender
 *
 * File: hiderPeers.h
 *
 * Description:
 *   CHiderPeers (spec 030, contracts/hider-registration.md): the frame's per-thread hiders, which a hider may reach through the
 *   frame it is created with (CReyes keeps the pointer for its sibling loop from Phase 8). Header-only.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef HIDERPEERS_H
#define HIDERPEERS_H

class CHider;

struct CHiderPeers {
        CHider *const *members; // the per-thread hiders, filled as they are constructed
        int count;
};

#endif
