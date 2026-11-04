/**
 * Project: openRender
 *
 * File: pointSource.h
 *
 * Description:
 *   IPointSource: what shade() asks of the primitive it shades (spec 029, contracts/point-source-and-globals.md). Owned
 *   by Shading; implemented in Hiders over CSurface (CSurfacePointSource). CAttributes and CSurface are only named:
 *   Shading never dereferences a CSurface.
 *
 *   Introduced in 2.1.0. Version: kRendererServicesVersion (rendererServices.h).
 *   Thread-safety: one per shading call, on the calling thread's stack. Re-enters Shading: never.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef POINTSOURCE_H
#define POINTSOURCE_H

#include <cstdint>

class CAttributes; // Graphics State (Shading may depend on it)
class CSurface;    // opaque: never dereferenced by Shading

namespace openrender::shading {

    class IPointSource {
        public:
            virtual ~IPointSource() = default; // inline: the vtable and the typeinfo are weak, wherever they are used

            virtual const CAttributes *attributes() const = 0;    // the primitive's attributes, never null
            virtual uint64_t stableId() const = 0;                // spec 039's stable identity (cell keys)
            virtual const CSurface *surface() const = 0;          // opaque, for the observers' reports
            virtual unsigned int requestedParameters() const = 0; // PARAMETER_* the caller needs sampled

            // CSurface::sample(start, count, varying, locals, parameters): fills the varyings of points [start, start + count);
            // reads and may change `parameters`, as before the interface existed.
            virtual void sample(int start, int count, float **varying, float ***locals, unsigned int &parameters) const = 0;
            // CSurface::interpolate(count, varying, locals): the primitive variables into the shader locals
            virtual void interpolate(int count, float **varying, float ***locals) const = 0;
    };

} // namespace openrender::shading

#endif
