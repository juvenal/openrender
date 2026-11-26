/**
 * Project: openRender
 *
 * File: builtinBxdfs.cpp
 *
 * Description:
 *   The four built-in Bxdfs (spec 035, contracts/bxdf.md "Built-ins", research R21): diffuse (RISpec 3.2's matte), plastic, conductor (metal)
 *   and dielectric. Each is an OrBxdfTable with typed, range-checked parameters; `build` is declarative and writes the point's tree through
 *   the allocator only. Weights are not premultiplied by Os (the consumer applies Oi = Os, analyze B1); the GGX alpha is the roughness, not its
 *   square, in all four; a microfacet reflection built with eta 0 has no Fresnel term. There is no Ka: ambient is lighting work with no lobe.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <cstring>
#include <new>

#include "bxdfRegistry.h"
#include "closureBuiltins.h"
#include "closureRegistry.h"
#include "common/diagnostics.h"

namespace openrender::shading {

    namespace {

        // ---- raw tree construction through the allocator (the allocator is the batch arena: nothing is freed) ----------------------------
        void *take(const OrClosureAllocator *alloc, size_t size, size_t align) {
            return alloc != nullptr && alloc->allocate != nullptr ? alloc->allocate(alloc->context, size, align) : nullptr;
        }

        const OrClosureNode *component(const OrClosureAllocator *alloc, int32_t id, const float w[3], const void *block, size_t blockSize) {
            void *memory = take(alloc, sizeof(OrClosureComponent) + blockSize, alignof(OrClosureComponent));
            if (memory == nullptr)
                return nullptr;
            OrClosureComponent *c = new (memory) OrClosureComponent;
            c->id = id;
            c->w[0] = w[0];
            c->w[1] = w[1];
            c->w[2] = w[2];
            std::memcpy(c + 1, block, blockSize);
            return reinterpret_cast<const OrClosureNode *>(c);
        }

        const OrClosureNode *add(const OrClosureAllocator *alloc, const OrClosureNode *a, const OrClosureNode *b) {
            if (a == nullptr)
                return b;
            if (b == nullptr)
                return a;
            void *memory = take(alloc, sizeof(OrClosureAdd), alignof(OrClosureAdd));
            if (memory == nullptr)
                return nullptr;
            OrClosureAdd *n = new (memory) OrClosureAdd;
            n->id = OR_CLOSURE_ADD;
            n->closureA = a;
            n->closureB = b;
            return reinterpret_cast<const OrClosureNode *>(n);
        }

        const OrClosureNode *diffuseLobe(const OrClosureAllocator *alloc, const float w[3], const float N[3]) {
            ClosureDiffuseBlock block = {{N[0], N[1], N[2]}};
            return component(alloc, kClosureDiffuse, w, &block, sizeof(block));
        }

        const OrClosureNode *orenNayarLobe(const OrClosureAllocator *alloc, const float w[3], const float N[3], float sigma) {
            ClosureOrenNayarBlock block = {{N[0], N[1], N[2]}, sigma};
            return component(alloc, kClosureOrenNayar, w, &block, sizeof(block));
        }

        // microfacet("ggx", N, alpha, eta, refract): alpha is the roughness, not squared
        const OrClosureNode *ggxLobe(const OrClosureAllocator *alloc, const float w[3], const float N[3], float alpha, float eta, int32_t refract) {
            ClosureMicrofacetBlock block;
            block.distribution = internClosureString("ggx");
            block.N[0] = N[0];
            block.N[1] = N[1];
            block.N[2] = N[2];
            block.alpha = alpha;
            block.eta = eta;
            block.refract = refract;
            return component(alloc, kClosureMicrofacet, w, &block, sizeof(block));
        }

        void scaled(float out[3], float k, const float c[3]) {
            out[0] = k * c[0];
            out[1] = k * c[1];
            out[2] = k * c[2];
        }

        float scalar(const void *const *values, int i) {
            return *static_cast<const float *>(values[i]);
        }
        const float *colour(const void *const *values, int i) {
            return static_cast<const float *>(values[i]);
        }

        void refuse(const OrBxdfDiagnostics *diag, const char *message) {
            if (diag != nullptr && diag->report != nullptr)
                diag->report(diag->context, (int)CODE_RANGE, message);
        }

        // ---- diffuse --------------------------------------------------------------------------------------------------------------------
        const OrBxdfParam kDiffuseParams[] = {
            {"Kd", OR_BXDF_FLOAT, {1.0f, 0.0f, 0.0f}, 0.0f, -1.0f, OR_BXDF_CLAMP, "diffuse reflectance (RISpec matte Kd)"},
            {"roughness", OR_BXDF_FLOAT, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f, OR_BXDF_CLAMP, "extension: above 0 the lobe is Oren-Nayar with sigma = roughness"},
        };
        struct SDiffuseData {
                float Kd, roughness;
        };
        OrBxdfData *diffuseCreate(const void *const *values, const OrBxdfDiagnostics *) {
            SDiffuseData *d = new SDiffuseData{scalar(values, 0), scalar(values, 1)};
            return reinterpret_cast<OrBxdfData *>(d);
        }
        void diffuseDestroy(OrBxdfData *data) { delete reinterpret_cast<SDiffuseData *>(data); }
        const OrClosureNode *diffuseBuild(const OrBxdfData *data, const OrBxdfPoint *p, const OrClosureAllocator *alloc) {
            const SDiffuseData *d = reinterpret_cast<const SDiffuseData *>(data);
            float w[3];
            scaled(w, d->Kd, p->Cs);
            return d->roughness > 0.0f ? orenNayarLobe(alloc, w, p->N, d->roughness) : diffuseLobe(alloc, w, p->N);
        }

        // ---- plastic --------------------------------------------------------------------------------------------------------------------
        const OrBxdfParam kPlasticParams[] = {
            {"Kd", OR_BXDF_FLOAT, {0.5f, 0.0f, 0.0f}, 0.0f, -1.0f, OR_BXDF_CLAMP, "diffuse reflectance (RISpec plastic Kd)"},
            {"Ks", OR_BXDF_FLOAT, {0.5f, 0.0f, 0.0f}, 0.0f, -1.0f, OR_BXDF_CLAMP, "specular reflectance (RISpec plastic Ks)"},
            {"roughness", OR_BXDF_FLOAT, {0.1f, 0.0f, 0.0f}, 0.0f, 1.0f, OR_BXDF_CLAMP, "GGX alpha (not squared)"},
            {"specularcolor", OR_BXDF_COLOR, {1.0f, 1.0f, 1.0f}, 0.0f, -1.0f, OR_BXDF_CLAMP, "highlight colour (RISpec plastic specularcolor)"},
            {"ior", OR_BXDF_FLOAT, {0.0f, 0.0f, 0.0f}, 0.0f, -1.0f, OR_BXDF_REFUSE, "extension: 0 is no Fresnel term, above 0 the dielectric Fresnel term; negative refuses"},
        };
        struct SPlasticData {
                float Kd, Ks, roughness, specularcolor[3], ior;
        };
        OrBxdfData *plasticCreate(const void *const *values, const OrBxdfDiagnostics *) {
            SPlasticData *d = new SPlasticData;
            d->Kd = scalar(values, 0);
            d->Ks = scalar(values, 1);
            d->roughness = scalar(values, 2);
            std::memcpy(d->specularcolor, colour(values, 3), sizeof(d->specularcolor));
            d->ior = scalar(values, 4);
            return reinterpret_cast<OrBxdfData *>(d);
        }
        void plasticDestroy(OrBxdfData *data) { delete reinterpret_cast<SPlasticData *>(data); }
        const OrClosureNode *plasticBuild(const OrBxdfData *data, const OrBxdfPoint *p, const OrClosureAllocator *alloc) {
            const SPlasticData *d = reinterpret_cast<const SPlasticData *>(data);
            float wd[3], ws[3];
            scaled(wd, d->Kd, p->Cs);
            scaled(ws, d->Ks, d->specularcolor);
            return add(alloc, diffuseLobe(alloc, wd, p->N), ggxLobe(alloc, ws, p->N, d->roughness, d->ior, kMicrofacetReflect));
        }

        // ---- conductor ------------------------------------------------------------------------------------------------------------------
        const OrBxdfParam kConductorParams[] = {
            {"Ks", OR_BXDF_FLOAT, {1.0f, 0.0f, 0.0f}, 0.0f, -1.0f, OR_BXDF_CLAMP, "reflectance, tinted by Cs (RISpec metal Ks)"},
            {"roughness", OR_BXDF_FLOAT, {0.1f, 0.0f, 0.0f}, 0.0f, 1.0f, OR_BXDF_CLAMP, "GGX alpha (not squared)"},
        };
        struct SConductorData {
                float Ks, roughness;
        };
        OrBxdfData *conductorCreate(const void *const *values, const OrBxdfDiagnostics *) {
            return reinterpret_cast<OrBxdfData *>(new SConductorData{scalar(values, 0), scalar(values, 1)});
        }
        void conductorDestroy(OrBxdfData *data) { delete reinterpret_cast<SConductorData *>(data); }
        const OrClosureNode *conductorBuild(const OrBxdfData *data, const OrBxdfPoint *p, const OrClosureAllocator *alloc) {
            const SConductorData *d = reinterpret_cast<const SConductorData *>(data);
            float w[3];
            scaled(w, d->Ks, p->Cs);
            return ggxLobe(alloc, w, p->N, d->roughness, 0.0f, kMicrofacetReflect); // eta 0: no Fresnel term
        }

        // ---- dielectric -----------------------------------------------------------------------------------------------------------------
        const OrBxdfParam kDielectricParams[] = {
            {"ior", OR_BXDF_FLOAT, {1.5f, 0.0f, 0.0f}, 0.0f, -1.0f, OR_BXDF_REFUSE, "index of refraction, above 0 (create refuses 0)"},
            {"roughness", OR_BXDF_FLOAT, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f, OR_BXDF_CLAMP, "GGX alpha (not squared); 0 is the specular limit"},
            {"reflectColor", OR_BXDF_COLOR, {1.0f, 1.0f, 1.0f}, 0.0f, -1.0f, OR_BXDF_CLAMP, "weight of the reflection lobe"},
            {"transmitColor", OR_BXDF_COLOR, {1.0f, 1.0f, 1.0f}, 0.0f, -1.0f, OR_BXDF_CLAMP, "weight of the transmission lobe"},
        };
        struct SDielectricData {
                float ior, roughness, reflectColor[3], transmitColor[3];
        };
        OrBxdfData *dielectricCreate(const void *const *values, const OrBxdfDiagnostics *diag) {
            const float ior = scalar(values, 0);
            if (!(ior > 0.0f)) { // the range is (0, inf): the descriptor's minimum is inclusive, so the zero is refused here
                refuse(diag, "Bxdf \"dielectric\": parameter \"ior\" must be greater than 0\n");
                return nullptr;
            }
            SDielectricData *d = new SDielectricData;
            d->ior = ior;
            d->roughness = scalar(values, 1);
            std::memcpy(d->reflectColor, colour(values, 2), sizeof(d->reflectColor));
            std::memcpy(d->transmitColor, colour(values, 3), sizeof(d->transmitColor));
            return reinterpret_cast<OrBxdfData *>(d);
        }
        void dielectricDestroy(OrBxdfData *data) { delete reinterpret_cast<SDielectricData *>(data); }
        const OrClosureNode *dielectricBuild(const OrBxdfData *data, const OrBxdfPoint *p, const OrClosureAllocator *alloc) {
            const SDielectricData *d = reinterpret_cast<const SDielectricData *>(data);
            return add(alloc, ggxLobe(alloc, d->reflectColor, p->N, d->roughness, d->ior, kMicrofacetReflect),
                       ggxLobe(alloc, d->transmitColor, p->N, d->roughness, d->ior, kMicrofacetRefract));
        }

        template <size_t N>
        OrBxdfTable makeTable(const char *name, const OrBxdfParam (&params)[N], OrBxdfData *(*create)(const void *const *, const OrBxdfDiagnostics *), void (*destroy)(OrBxdfData *),
                              const OrClosureNode *(*build)(const OrBxdfData *, const OrBxdfPoint *, const OrClosureAllocator *)) {
            OrBxdfTable t;
            t.header.structSize = sizeof(OrBxdfTable);
            t.header.abiVersion = OR_BXDF_ABI_VERSION;
            t.name = name;
            t.params = params;
            t.numParams = (int)N;
            t.create = create;
            t.destroy = destroy;
            t.build = build;
            return t;
        }

    } // namespace

    const OrBxdfTable *diffuseBxdfTable() {
        static const OrBxdfTable t = makeTable("diffuse", kDiffuseParams, diffuseCreate, diffuseDestroy, diffuseBuild);
        return &t;
    }
    const OrBxdfTable *plasticBxdfTable() {
        static const OrBxdfTable t = makeTable("plastic", kPlasticParams, plasticCreate, plasticDestroy, plasticBuild);
        return &t;
    }
    const OrBxdfTable *conductorBxdfTable() {
        static const OrBxdfTable t = makeTable("conductor", kConductorParams, conductorCreate, conductorDestroy, conductorBuild);
        return &t;
    }
    const OrBxdfTable *dielectricBxdfTable() {
        static const OrBxdfTable t = makeTable("dielectric", kDielectricParams, dielectricCreate, dielectricDestroy, dielectricBuild);
        return &t;
    }

} // namespace openrender::shading
