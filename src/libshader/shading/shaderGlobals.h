/**
 * Project: openRender
 *
 * File: shaderGlobals.h
 *
 * Description:
 *   ShaderGlobals: the description of a batch of shading points (spec 026, contracts/shader-globals.md,
 *   data-model.md). A structure of arrays with a runtime point count and no fixed batch width: a grid
 *   (u fastest), a span of independent points, or one point. It owns nothing.
 *
 *   Spec 026 produced it only in test doubles. Since spec 029 shade() takes it (with an IPointSource) as its input:
 *   the hider that owns the context describes the batch, and spec 049 adapts the structure to OSL's.
 *
 *   Spaces and units: unless a field says otherwise, P, N, Ng, I, dPdu, dPdv and dPdtime are in the
 *   current (camera) coordinate system, the space shade() works in (getMatrix("current") is the identity).
 *   The imager is the exception: its P is the pixel coordinate. Times (shutterOpen, dtime, time) are in
 *   the time units of RiShutter, the values the shader reads as `dtime` and `time`.
 *
 *   Presence rules (data-model.md, "Per-point fields and presence rules"):
 *     - Always present: P, time, and every per-batch field. "Always present" is a rule for a producer: it
 *       must set P and time (their present() is true) on every batch, whatever the situation. The default
 *       constructed structure has every field absent, and a consumer may assume nothing else is set.
 *     - Every other per-point field is optional: test present() before reading it.
 *     - As the input of shade() (spec 029) every per-batch field is set and every per-point field is absent: the per-point
 *       values are in the context's varyings (written by the hider or Geometry before the call: u, v, time, and for ray
 *       hits P, Ng, I, du) or are sampled by the point source inside shade(). A full description (what the test doubles and
 *       ShadingContract_ShaderGlobalsDouble build, and spec 049) follows the rules above.
 *     - Array lengths: every per-point array with a non-zero stride has numPoints + numDerivativeTail
 *       elements (times its stride): the real points first, then the tail. A field with stride 0 holds
 *       one value for every point, tail included, and has one element (times its width).
 *       Which field a producer fills in which shading situation (grid, ray hit, photon emission,
 *       displacement pass, imager) is the table of data-model.md, "Per-point fields and presence rules";
 *       the situations are those of the table in contracts/shader-globals.md, "Situation table". activeTags has numPoints entries, indexed by real
 *       point; the tail has no entry and is never active.
 *     - The derivative tail (numDerivativeTail > 0) follows the real points, holds the du-offset then the
 *       dv-offset copies of them, is never counted in numPoints, and is never an active lane. Its
 *       sampleKey is the real point's key.
 *     - An inactive lane (activeTags[i] != 0) holds unspecified values.
 *
 *   Version: kRendererServicesVersion (rendererServices.h). A field appended at the end of ShaderGlobals,
 *   or an enumerator appended to a vocabulary, is MINOR; removing a field or changing a presence rule is
 *   MAJOR.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADERGLOBALS_H
#define SHADERGLOBALS_H

#include <cstdint>

#include "common/sampling.h" // openrender::sampling::SampleKey
#include "serviceTypes.h"    // ObjectHandle, ShaderHandle

namespace openrender::shading {

    // The vocabularies below are append only.

    // Grid is gridU x gridV points, u fastest (numUvertices/numVvertices); Span is independent 2-D ray hits
    // (SHADING_2D); Point is a batch of one; PointSet (2.1.0) is a 0-D point set (SHADING_0D: points, curves' vertices,
    // implicit hits, a phony light batch). The producer of shade()'s input sets gridU and gridV for every shape (the
    // number of u and v vertices of the call, numPoints = gridU * gridV).
    enum class BatchShape : uint8_t { Grid, Span, Point, PointSet };

    // None covers a rasterised grid or point, a displacement pass, the imager and a bare execution.
    enum class RayType : uint8_t { None, Camera, Secondary, Shadow, Photon };

    // From camerahitmode, specularhitmode and transmissionhitmode ('s' shades, 'p' reads the primitive).
    enum class HitMode : uint8_t { Shade, Primitive };

    enum class TransportMode : uint8_t { Radiance, Importance };

    // shade()'s displaceOnly argument: FALSE, TRUE, 3.
    enum class ShadingPass : uint8_t { Full, DisplaceOnly, DisplaceEstimate };

    // One per-point field: element i of point k is data[k * stride + i]. stride 0: one value for every
    // point. data == nullptr: the field is absent; test present() before reading.
    template <class T> struct Strided {
            const T *data = nullptr;
            int stride = 0;
            bool present() const { return data != nullptr; }
            const T *at(int point) const { return data + point * stride; }
    };

    // Introduced in 2.0.0. Version: kRendererServicesVersion (rendererServices.h).
    struct ShaderGlobals {
            // ---- shape
            int numPoints = 0;               // real points
            int numDerivativeTail = 0;       // 0, or 2 * numPoints: the du-offset then dv-offset copies
            BatchShape shape = BatchShape::Point;
            int gridU = 0, gridV = 0;        // gridU * gridV == numPoints; a Grid's dimensions (shade()'s input sets them for every shape)
            const int *activeTags = nullptr; // numPoints entries, 0 = active (the shading tags[]); nullptr: every real point is active

            // ---- per batch (always present)
            ObjectHandle object;
            ShaderHandle shader;
            RayType rayType = RayType::None;
            HitMode hitMode = HitMode::Shade;
            TransportMode transport = TransportMode::Radiance;
            ShadingPass pass = ShadingPass::Full;
            int pathDepth = 0;              // ray levels below the camera (CShadingContext::currentRayDepth)
            float shadingCellFootprint = 1; // pixels: CAttributes::shadingRate, already sqrt(area), never square-rooted again
            float shutterOpen = 0, dtime = 0; // the shutter's open time and its length (close - open), in RiShutter's units

            // ---- per point (presence: see the header comment)
            Strided<float> P, N, Ng, I;  // stride 3, camera space; I is the incident vector (the eye's direction to P)
            Strided<float> u, v, du, dv; // stride 1; du and dv are the parametric steps
            Strided<float> rayWidth;     // stride 1; da * t + db of the ray that hit
            Strided<float> dPdu, dPdv;   // stride 3
            Strided<float> dPdx, dPdy;   // stride 3; reserved: no producer fills them in 026
            Strided<float> time;         // stride 1
            Strided<float> dPdtime;      // stride 3
            Strided<openrender::sampling::SampleKey> sampleKey; // stride 1
    };

} // namespace openrender::shading

#endif
