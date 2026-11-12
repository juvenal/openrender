/**
 * Project: openRender
 *
 * File: photonMapAccess.h
 *
 * Description:
 *   What Shading and the hiders ask of a photon map beyond Shading's own photonmap() lookup (spec 032,
 *   contracts/photon-map.md): the hiders' irradiance estimate (IPhotonEstimator) and the photon hider's writes
 *   (IPhotonStore). CPhotonMapRefTarget is what a Graphics State map slot holds: the photon map, seen through
 *   these interfaces. The storage itself is Texturing's CPhotonStore; the class that composes it with this one
 *   (CPhotonMap) is Core's, header-only (render/photonMap.h).
 *
 *   Introduced in 2.2.0. Version: kRendererServicesVersion (rendererServices.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef PHOTONMAPACCESS_H
#define PHOTONMAPACCESS_H

#include <stdint.h>

#include "common/sampling.h"
#include "photonLookup.h"
#include "stateHandles.h" // CMapRefTarget, MapRef (Shading -> Graphics State)

class CXform; // Graphics State

namespace openrender::shading {

    // The hiders' estimate: the irradiance at P with normal N from the maxFound nearest photons (C: 3 floats). The body is
    // CPhotonStore::lookup(C, P, N, maxFound).
    class IPhotonEstimator {
        public:
            virtual ~IPhotonEstimator() = default;

            virtual void estimate(float *C, const float *P, const float *N, int maxFound) = 0;
    };

    // What the photon hider writes. Not thread-safe beyond what the photon map was: storePhoton() locks, the modifying
    // flag is the same unguarded int as before.
    class IPhotonStore {
        public:
            virtual ~IPhotonStore() = default;

            virtual void storePhoton(const float *P, const float *N, const float *I, const float *C, uint64_t key) = 0;
            virtual void resetPhotons() = 0;
            virtual void balancePhotons() = 0;
            virtual int isModifying() const = 0;
            virtual void setModifying(int modifying) = 0;
            virtual void writePhotons(const CXform *world) = 0; // world's from/to are the matrices written
    };

    // What a Graphics State map slot holds (spec 025): the photon map, seen by Shading and the hiders
    class CPhotonMapRefTarget : public CMapRefTarget, public IPhotonLookup, public IPhotonEstimator, public IPhotonStore {
        protected:
            CPhotonMapRefTarget() = default;
    };

    // The key of a stored photon (spec 040's identity): the point that emitted it (the photon's identity in the frame's sequence: light,
    // index, frame, seed) and the bounce at which it was stored. A pure function: no random value is drawn.
    inline uint64_t photonKey(uint64_t pointKeyFull, uint32_t bounce) {
        return openrender::sampling::detail::mix(0xF07011BEull, pointKeyFull, bounce);
    }

    // The only conversion of a map handle: a map slot only ever holds a CPhotonMap, which derives from this class
    inline CPhotonMapRefTarget *photonMapOf(const MapRef &r) {
        return static_cast<CPhotonMapRefTarget *>(r.get());
    }

} // namespace openrender::shading

#endif
