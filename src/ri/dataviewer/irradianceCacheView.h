/**
 * Project: openRender
 *
 * File: irradianceCacheView.h
 *
 * Description:
 *   The data viewer's adapter of the irradiance cache (spec 032): draw(), the keys and the viewer state that CIrradianceCache used to carry as a CDataView.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	irradianceCacheView.h
//  Classes				:	CIrradianceCacheView, CIrradianceCacheDocument
//  Description			:	The irradiance cache no longer derives from the data viewer; this adapter draws it through the
//							cache's read interface (forEachRecord). The draw and keyDown bodies are the ones the cache had.
//
////////////////////////////////////////////////////////////////////////
#ifndef IRRADIANCECACHEVIEW_H
#define IRRADIANCECACHEVIEW_H

#include "dataView.h"
#include "irradiance.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCacheView
// Description			:	Draws an irradiance cache
// Comments				:	The draw mode is class-static, as it was in CIrradianceCache. Like the cache did, the view reports no
//							current channel, no detail level and the default draw mode (0) to the scene snapshot
class CIrradianceCacheView : public CDataView {
    public:
        explicit CIrradianceCacheView(const CIrradianceCache *cache) : cache(cache) {}

        void draw() override;
        int keyDown(int key) override;
        int numChannels() const override { return cache->numChannels(); }
        const char *channelName(int index) const override { return cache->channelName(index); }

    protected:
        const CIrradianceCache *cache;

        static int drawDiscs; // Which type to draw
};

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCacheDocument
// Description			:	An irradiance cache that can be drawn: what CDataDocument::open() builds
// Comments				:	One final overrider for each member both bases declare (the view answers the channels)
class CIrradianceCacheDocument final : public CIrradianceCache, public CIrradianceCacheView {
    public:
        CIrradianceCacheDocument(const char *name, unsigned int flags, FILE *in, const float *from, const float *to, const float *tondc = NULL)
            : CIrradianceCache(name, flags, in, from, to, tondc), CIrradianceCacheView(this) {}

        void bound(float *bmin, float *bmax) override { CIrradianceCache::bound(bmin, bmax); }
        const char *typeName() const override { return CIrradianceCache::typeName(); }
        int numChannels() const override { return CIrradianceCache::numChannels(); }
        const char *channelName(int index) const override { return CIrradianceCache::channelName(index); }
};

#endif
