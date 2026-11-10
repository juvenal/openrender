/**
 * Project: openRender
 *
 * File: photonLookup.h
 *
 * Description:
 *   IPhotonLookup: what Shading asks of a photon map (spec 029, contracts/photon-lookup.md). Owned by Shading;
 *   implemented by the photon map (CPhotonMap, Core, render/photonMap.h), whose storage, file handling and balancing
 *   are Texturing's (CPhotonStore, spec 032). Returned by CRendererServices::getPhotonMap().
 *
 *   Introduced in 2.1.0. Version: kRendererServicesVersion (rendererServices.h).
 *   Thread-safety: as CPhotonMap::lookup today (concurrent lookups on a balanced map).
 *   Re-enters Shading: never.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef PHOTONLOOKUP_H
#define PHOTONLOOKUP_H

namespace openrender::shading {

    class IPhotonLookup {
        public:
            virtual ~IPhotonLookup() = default; // inline: the vtable and the typeinfo are weak, wherever they are used

            // The irradiance estimate at P from the maxFound nearest photons, written to C (3 floats); P in the
            // map's space as the caller passes it. The body is CPhotonMap::lookup(float *, const float *, int).
            virtual void lookup(float *C, const float *P, int maxFound) = 0;
    };

} // namespace openrender::shading

#endif
