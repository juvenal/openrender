/**
 * Project: openRender
 *
 * File: brickMapView.h
 *
 * Description:
 *   The data viewer's adapter of the brick map (spec 032): draw(), the keys and the viewer state that CBrickMap used to carry as a CDataView.
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
//  File				:	brickMapView.h
//  Classes				:	CBrickMapView, CBrickMapDocument
//  Description			:	The brick map no longer derives from the data viewer (it is Texturing); this adapter draws it
//							through CBrickMap::visitVoxels(), which loads bricks on demand exactly as the old draw did.
//							The draw and keyDown bodies are the ones CBrickMap had, moved.
//
////////////////////////////////////////////////////////////////////////
#ifndef BRICKMAPVIEW_H
#define BRICKMAPVIEW_H

#include "brickmap.h"
#include "dataView.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CBrickMapView
// Description			:	Draws a brick map
// Comments				:	The viewer's state (detail level, draw type, channel) is class-static, as it was in CBrickMap
class CBrickMapView : public CDataView {
    public:
        explicit CBrickMapView(CBrickMap *brick) : brick(brick) {} // not const: drawing loads bricks

        void draw() override;
        int keyDown(int key) override;
        int currentChannel() const override { return drawChannel; }
        int detailLevel() const override { return detail; }
        int drawMode() const override { return drawType; } // 0 = boxes, 1 = discs, 2 = points

    protected:
        CBrickMap *brick;

        static int detail;      // The brickmap detail level for visualization
        static int drawType;    // Which type to draw
        static int drawChannel; // Which channel to draw
};

///////////////////////////////////////////////////////////////////////
// Class				:	CBrickMapDocument
// Description			:	A brick map that can be drawn: what CDataDocument::open() builds
// Comments				:	One final overrider for each member both bases declare
class CBrickMapDocument final : public CBrickMap, public CBrickMapView {
    public:
        CBrickMapDocument(FILE *in, const char *name, const float *from, const float *to) : CBrickMap(in, name, from, to), CBrickMapView(this) {}

        void bound(float *bmin, float *bmax) override { CBrickMap::bound(bmin, bmax); }
        const char *typeName() const override { return CBrickMap::typeName(); }
        int numChannels() const override { return CBrickMap::numChannels(); }
        const char *channelName(int index) const override { return CBrickMap::channelName(index); }
};

#endif
