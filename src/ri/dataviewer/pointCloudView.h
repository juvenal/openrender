/**
 * Project: openRender
 *
 * File: pointCloudView.h
 *
 * Description:
 *   The data viewer's adapter of the point cloud (spec 032): draw(), the keys and the viewer state that CPointCloud used to carry as a CDataView.
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
//  File				:	pointCloudView.h
//  Classes				:	CPointCloudView, CPointCloudDocument
//  Description			:	The point cloud no longer derives from the data viewer (it is Texturing); this adapter
//							draws it through its read interface (points(), pointData(), the channel accessors).
//							The draw and keyDown bodies are the ones CPointCloud had, moved.
//
////////////////////////////////////////////////////////////////////////
#ifndef POINTCLOUDVIEW_H
#define POINTCLOUDVIEW_H

#include "dataView.h"
#include "pointCloud.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPointCloudView
// Description			:	Draws a point cloud
// Comments				:	The viewer's state (draw mode, channel) is class-static, as it was in CPointCloud, so a key
//							pressed in one document still applies to the next one of the same type in the process
class CPointCloudView : public CDataView {
    public:
        explicit CPointCloudView(const CPointCloud *cloud) : cloud(cloud) {}

        void draw() override;
        int keyDown(int key) override;
        int currentChannel() const override { return drawChannel; }
        int drawMode() const override { return drawDiscs; } // 0 = points, 1 = discs

    protected:
        const CPointCloud *cloud;

        static int drawDiscs;   // Which type to draw
        static int drawChannel; // Which channel to draw
};

///////////////////////////////////////////////////////////////////////
// Class				:	CPointCloudDocument
// Description			:	A point cloud that can be drawn: what CDataDocument::open() builds
// Comments				:	One final overrider for each member both bases declare
class CPointCloudDocument final : public CPointCloud, public CPointCloudView {
    public:
        CPointCloudDocument(const char *name, const float *from, const float *to, FILE *in) : CPointCloud(name, from, to, in), CPointCloudView(this) {}

        void bound(float *bmin, float *bmax) override { CPointCloud::bound(bmin, bmax); }
        const char *typeName() const override { return CPointCloud::typeName(); }
        int numChannels() const override { return CPointCloud::numChannels(); }
        const char *channelName(int index) const override { return CPointCloud::channelName(index); }
};

#endif
