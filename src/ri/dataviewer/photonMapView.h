/**
 * Project: openRender
 *
 * File: photonMapView.h
 *
 * Description:
 *   The data viewer's adapter of the photon map (spec 032): the photon store is Texturing and no longer derives from the
 *   viewer, so this class draws it through the store's items. The draw body is the one the photon map had, moved.
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
//  File				:	photonMapView.h
//  Classes				:	CPhotonMapView
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef PHOTONMAPVIEW_H
#define PHOTONMAPVIEW_H

#include "dataView.h"

class CPhotonStore;

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonMapView
// Description			:	Draws a photon map
// Comments				:	No channels, no detail level, no keys: the defaults of CDataView. bound() and typeName() are the
//							store's, answered by the class that composes the two (CPhotonMap, render/photonMap.h)
class CPhotonMapView : public CDataView {
    public:
        explicit CPhotonMapView(const CPhotonStore *photons) : photons(photons) {}

        void draw() override;

    protected:
        const CPhotonStore *photons;
};

#endif
