/**
 * Project: openRender
 *
 * File: geometryHost.cpp
 *
 * Description:
 *   Registration hook for the Geometry host (spec 021).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "geometryHost.h"

#include <atomic>

static std::atomic<CGeometryHost *> installedHost{nullptr};

void setGeometryHost(CGeometryHost *host) {
    installedHost.store(host, std::memory_order_release);
}

CGeometryHost *geometryHost() {
    return installedHost.load(std::memory_order_acquire);
}
