/**
 * Project: openRender
 *
 * File: photonMap.h
 *
 * Description:
 *   CPhotonMap (spec 032): the photon map the renderer core hands out, composed from the Texturing store
 *   (CPhotonStore: kd-tree, file format, estimates), the Graphics State handle target and Shading's access interfaces
 *   (CPhotonMapRefTarget) and the data viewer's adapter (CPhotonMapView). Header-only on purpose: libRIBVector (the data
 *   viewer) constructs the map and has no render/photonMap.cpp to link.
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
//  File				:	photonMap.h
//  Classes				:	CPhotonMap
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef PHOTONMAP_H
#define PHOTONMAP_H

#include "photonMapAccess.h"
#include "photonMapView.h"
#include "photonStore.h"
#include "renderer.h"
#include "xform.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonMap
// Description			:	A Photon map
// Comments				:	One CRefCounter base (through CMapRefTarget), so refCount, attach() and detach() are unambiguous
class CPhotonMap : public CPhotonStore, public openrender::shading::CPhotonMapRefTarget, public CPhotonMapView {
    public:
        CPhotonMap(const char *name, FILE *in) : CPhotonStore(name, in, CRenderer::fromWorld, CRenderer::toWorld), CPhotonMapView(this) {
            attach(); // Count the fileResource reference
        }

        // IPhotonLookup, IPhotonEstimator and IPhotonStore: forwards to the store
        void lookup(float *C, const float *P, int maxFound) override { CPhotonStore::lookup(C, P, maxFound); }
        void estimate(float *C, const float *P, const float *N, int maxFound) override { CPhotonStore::lookup(C, P, N, maxFound); }
        void storePhoton(const float *P, const float *N, const float *I, const float *C, uint64_t key) override { CPhotonStore::store(P, N, I, C, key); }
        void resetPhotons() override { CPhotonStore::reset(); }
        void balancePhotons() override { CPhotonStore::balance(); }
        int isModifying() const override { return modifying; }
        void setModifying(int m) override { modifying = m; }
        void writePhotons(const CXform *world) override { write(world); }

        // The matrices written are the world's, or the renderer's when there is none (the data viewer's tests write(NULL))
        void write(const CXform *world) {
            CPhotonStore::write(world != NULL ? world->from : CRenderer::fromWorld, world != NULL ? world->to : CRenderer::toWorld);
        }

        // The members both the store and the viewer declare: one final overrider each
        void bound(float *bmin, float *bmax) override { CPhotonStore::bound(bmin, bmax); }
        const char *typeName() const override { return CPhotonStore::typeName(); }
};

#endif
