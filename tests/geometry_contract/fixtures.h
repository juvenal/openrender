/**
 * Project: openRender
 *
 * File: fixtures.h
 *
 * Description:
 *   Builders for the primitives the spec 031 contract programs dice and intersect, constructed directly (the
 *   programs that link only libRIBVector have no Ri context to create them). Every object sits in front of the
 *   camera on the +z axis with an identity camera transform, so camera space is the coordinates written here.
 *   Needs a CRiStandardSession for the standard declarations (P).
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#ifndef GEOMETRY_CONTRACT_FIXTURES_H
#define GEOMETRY_CONTRACT_FIXTURES_H

#include "attributes.h"
#include "declarations.h"
#include "patches.h"
#include "pl.h"
#include "points.h"
#include "quadrics.h"
#include "riServices.h"
#include "xform.h"

namespace contract {

    // attributes and a transform that translates by (0, 0, z)
    struct Placement {
        CAttributes *attributes;
        CXform *xform;
        explicit Placement(float z) : attributes(new CAttributes()), xform(new CXform()) {
            xform->translate(0, 0, z);
            attributes->attach();
            xform->attach();
        }
    };

    // A P-only parameter list over `n` vertices (data copied by CPl's owner semantics: pass an owned array)
    inline CPl *positionList(int n, float *data) {
        CPlParameter *p = new CPlParameter[1];
        p[0].variable = CDeclarations::find("P");
        p[0].numItems = n;
        p[0].index = 0;
        p[0].container = CONTAINER_VERTEX;
        return new CPl(n, 1, p, data);
    }

    inline CSurface *sphere(const Placement &pl) { return new CSphere(pl.attributes, pl.xform, NULL, 0, 1.0f, -1.0f, 1.0f, 6.2831853f); }

    inline CSurface *bilinearPatch(const Placement &pl) {
        float *v = new float[12]{-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0};
        CPl *list = positionList(4, v);
        return new CBilinearPatch(pl.attributes, pl.xform, list->vertexData(), NULL, 0.0f, 0.0f, 1.0f, 1.0f, v);
    }

    // a bilinear-degree NURBS patch (order 2 x 2) whose (u,v) square carries one triangular trim loop
    inline CSurface *trimmedNurbsPatch(const Placement &pl) {
        float *v = new float[12]{-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0};
        CPl *list = positionList(4, v);
        static float knots[4] = {0, 0, 1, 1};
        CTrimTest *trim = new CTrimTest();
        trim->numLoops = 1;
        trim->loopPolylines = new CTrimPolyline[1];
        trim->loopPolylines[0].numVertices = 3;
        trim->loopPolylines[0].uv = new float[6]{0.1f, 0.1f, 0.9f, 0.1f, 0.5f, 0.9f};
        return new CNURBSPatch(pl.attributes, pl.xform, list->vertexData(), NULL, 2, 2, knots, knots, v, trim);
    }

    inline CSurface *points(const Placement &pl, int n) {
        float *v = new float[3 * n];
        for (int i = 0; i < n; i++) {
            v[3 * i + 0] = -0.5f + (float)i / (float)n;
            v[3 * i + 1] = 0.0f;
            v[3 * i + 2] = 0.0f;
        }
        return new CPoints(pl.attributes, pl.xform, positionList(n, v), n);
    }

} // namespace contract

#endif
