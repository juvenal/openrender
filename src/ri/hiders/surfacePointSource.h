/**
 * Project: openRender
 *
 * File: surfacePointSource.h
 *
 * Description:
 *   CSurfacePointSource: shade()'s view of a CSurface (spec 029, contracts/point-source-and-globals.md). Each member
 *   forwards to the unchanged CSurface virtual or field, so the sampling and the interpolation are exactly the
 *   ones Geometry has always provided.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SURFACEPOINTSOURCE_H
#define SURFACEPOINTSOURCE_H

#include "libshader/shading/pointSource.h"
#include "object.h"

class CSurfacePointSource final : public openrender::shading::IPointSource {
    public:
        CSurfacePointSource(const CSurface *surface, unsigned int parameters) : object(surface), requested(parameters) {}

        const CAttributes *attributes() const override { return object->attributes; }
        uint64_t stableId() const override { return object->stableId; }
        const CSurface *surface() const override { return object; }
        unsigned int requestedParameters() const override { return requested; }

        void sample(int start, int count, float **varying, float ***locals, unsigned int &parameters) const override {
            object->sample(start, count, varying, locals, parameters);
        }
        void interpolate(int count, float **varying, float ***locals) const override { object->interpolate(count, varying, locals); }

    private:
        const CSurface *object;
        unsigned int requested;
};

#endif
