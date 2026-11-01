/**
 * Project: openRender
 *
 * File: texture3dChannels.h
 *
 * Description:
 *   The interface Texturing owns for reading display channels (spec 037, FR-025, US7, DB-021;
 *   contracts/texture3d-channels.md). CTexture3d::defineChannels() asks the installed source whether a
 *   name is a known display channel and how big and of what type it is; the renderer core implements
 *   the source over the Graphics State channel registry and installs it for the length of a
 *   RiBegin/RiEnd session. With no source installed (no session) every name is unknown.
 *
 *   Texturing includes nothing of the renderer core or of Graphics State for this.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef TEXTURE3DCHANNELS_H
#define TEXTURE3DCHANNELS_H

struct STexture3dChannelInfo {
        const char *name;  // the channel name as registered
        int numSamples;    // floats in the channel
        int type;          // TYPE_* of common/rendererc.h (TYPE_FLOAT for the standard rgbaz channels)
        const float *fill; // the registry's own default array, or NULL: aliased, not copied
};

class CTexture3dChannelSource {
    public:
        virtual ~CTexture3dChannelSource() = default;

        // True, with `out` filled, if `name` is a known display channel.
        virtual bool find(const char *name, STexture3dChannelInfo &out) const = 0;
};

// Installs the source (nullptr uninstalls). The caller keeps ownership and keeps it alive while installed.
void setTexture3dChannelSource(const CTexture3dChannelSource *source);
const CTexture3dChannelSource *texture3dChannelSource();

#endif
