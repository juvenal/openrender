/**
 * Project: openRender
 *
 * File: texture3dChannels.cpp
 *
 * Description:
 *   The registration pair of Texturing's channel interface. See texture3dChannels.h. It is in ri_sources
 *   next to texture3d.cpp, so ribRender and ribVector, which both compile CTexture3d, both have the pair.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "texture3dChannels.h"

#include <atomic>

namespace {
    std::atomic<const CTexture3dChannelSource *> g_source{nullptr};
}

void setTexture3dChannelSource(const CTexture3dChannelSource *source) {
    g_source.store(source);
}

const CTexture3dChannelSource *texture3dChannelSource() {
    return g_source.load();
}
