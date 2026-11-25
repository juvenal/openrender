/*
 * Spec 035, the conventions of contracts/closure-abi.md "BSDF table" that are not a numerical estimator: Bsdf_Horizon_*, Bsdf_NotScatterer,
 * Bsdf_ImportanceFactor, Bsdf_Status_*, Bsdf_Frame and Bsdf_Wrapper (label closures). Every case goes through the tree-level table
 * (prepare / evaluate / sample / pdf) except where it says a component.
 *
 *   Bsdf_Horizon_Grid        below-horizon and grazing directions, an exhaustive grid of cosines from -1 to 1 (zero, tiny and denormal included),
 *                            for every lobe kind: f 0, densities 0, OR_BSDF_FAILED on a sample, never a negative or a non-finite value (convention 6)
 *   Bsdf_Horizon_Invalid     a NaN or an infinite component of a direction is rejected with OR_STATUS_BAD_ARGUMENT (convention 10)
 *   Bsdf_NotScatterer        a tree with only EDF and signal lobes, and the empty tree, answer zero with OR_BSDF_NOT_SCATTERER (convention 7)
 *   Bsdf_ImportanceFactor    the radiance-mode factor (eta_wo / eta_wi)^2 on a transmissive lobe (entering from outside: 1 / 2.25, from inside:
 *                            2.25 for eta 1.5), identical reflection in both modes, importance mode unscaled (convention 5, US2 AS3, A1)
 *   Bsdf_Status_Table        OR_STATUS_OK with FAILED, OR_STATUS_BAD_ARGUMENT for a null pointer or a structSize smaller than 1.0.0's (convention 10),
 *                            the table header (structSize, abiVersion)
 *   Bsdf_Frame               a lobe's own normal N is its local frame (a tilted component normal), not the shading frame's
 *   Bsdf_Wrapper             the C++ wrapper (bsdf.hpp): prepare, evaluate, sample and pdf with a Spectrum f
 *
 * The geometric normal Ng is part of the query and the interface REPORTS its side only: it does not reject a light leak (a wi on the other side
 * of Ng than wo gives the same answer as with Ng flipped), which Bsdf_Status_Table checks and which this comment documents.
 *
 * Written red first: bsdfMath.h, bsdf.hpp and orBsdfTable do not exist yet.
 */
#include <string>

#include "bsdf.hpp"
#include "bsdf_checks.h"
#include "bsdfMath.h"

using namespace clbsdf;
using namespace openrender::shading;
using openrender::Spectrum;

// one tree of one lobe of each kind that scatters
struct Lobe {
        const char *name;
        int32_t id;
        std::vector<unsigned char> block;
};

static std::vector<Lobe> allLobes() {
    std::vector<Lobe> v;
    auto add = [&v](const char *name, int32_t id, const void *b, size_t n) { v.push_back({name, id, std::vector<unsigned char>((const unsigned char *)b, (const unsigned char *)b + n)}); };
    static ClosureDiffuseBlock d = {{0, 0, 1}};
    static ClosureOrenNayarBlock on = {{0, 0, 1}, 0.5f};
    static ClosureMicrofacetBlock mr, mt, mb, md;
    mr = {internClosureString("ggx"), {0, 0, 1}, 0.3f, 1.5f, 0};
    mt = {internClosureString("beckmann"), {0, 0, 1}, 0.3f, 1.5f, 1};
    mb = {internClosureString("ggx"), {0, 0, 1}, 0.5f, 1.5f, 2};
    md = {internClosureString("ggx"), {0, 0, 1}, 5e-5f, 1.5f, 2}; // below the floor: delta
    static ClosureReflectionBlock r = {{0, 0, 1}, 1.5f};
    static ClosureRefractionBlock t = {{0, 0, 1}, 1.5f};
    add("diffuse", kClosureDiffuse, &d, sizeof(d));
    add("oren_nayar", kClosureOrenNayar, &on, sizeof(on));
    add("microfacet reflect", kClosureMicrofacet, &mr, sizeof(mr));
    add("microfacet refract", kClosureMicrofacet, &mt, sizeof(mt));
    add("microfacet both", kClosureMicrofacet, &mb, sizeof(mb));
    add("microfacet delta", kClosureMicrofacet, &md, sizeof(md));
    add("reflection", kClosureReflection, &r, sizeof(r));
    add("refraction", kClosureRefraction, &t, sizeof(t));
    return v;
}

static OrBsdf *prepareOne(CMemPage *&arena, const Lobe &l, float weight = 1.0f) {
    ClosureTreeBuilder builder(arena, 4, nullptr);
    const float w[3] = {weight, weight, weight};
    OrBsdf *out = nullptr;
    prepareTree(builder.component(l.id, w, l.block.data(), l.block.size()), arena, &out);
    return out;
}

static bool sane(const OrBsdfEval &e) {
    return std::isfinite(e.f[0]) && std::isfinite(e.f[1]) && std::isfinite(e.f[2]) && e.f[0] >= 0 && e.f[1] >= 0 && e.f[2] >= 0 && std::isfinite(e.pdfForward) && e.pdfForward >= 0 &&
           std::isfinite(e.pdfReverse) && e.pdfReverse >= 0;
}

static void sectionHorizonGrid() {
    CMemPage *arena;
    memoryInit(arena);
    const double cosines[] = {-1.0, -0.5, -1e-3, -1e-30, -1e-42, 0.0, 1e-42, 1e-30, 1e-3, 0.5, 1.0}; // 1e-42 is a denormal float
    for (const Lobe &l : allLobes()) {
        OrBsdf *bsdf = prepareOne(arena, l);
        CL_CHECK(bsdf != nullptr);
        const Subject s = tableSubject(l.name, bsdf);
        for (double co : cosines)
            for (double ci : cosines)
                for (double phi : {0.0, 1.3}) {
                    const V3 wo = fromCosAzimuth(co, 0.4), wi = fromCosAzimuth(ci, 0.4 + phi);
                    for (OrTransportMode m : {OR_TRANSPORT_RADIANCE, OR_TRANSPORT_IMPORTANCE}) {
                        const OrBsdfEval e = s.evaluate(wo, wi, m), q = s.pdf(wo, wi, m);
                        if (!sane(e) || !sane(q)) {
                            char t[200];
                            std::snprintf(t, sizeof(t), "%s evaluate/pdf not finite or negative at cos(wo) %g cos(wi) %g", l.name, co, ci);
                            cltest::fail(__FILE__, __LINE__, t);
                        }
                    }
                }
        for (double co : cosines) {
            const V3 wo = fromCosAzimuth(co, 0.4);
            for (float u1 : {0.0f, 0.001f, 0.5f, 0.999f})
                for (float u2 : {0.0f, 0.3f, 0.999f}) {
                    const OrBsdfSample r = s.sample(wo, u1, u2, 0.37f, OR_TRANSPORT_RADIANCE);
                    const OrBsdfEval asEval = {{r.f[0], r.f[1], r.f[2], r.f[3]}, r.numChannels, r.pdfForward, r.pdfReverse, r.flags};
                    CL_CHECK(sane(asEval) && std::isfinite(r.wi[0]) && std::isfinite(r.wi[1]) && std::isfinite(r.wi[2]));
                    if (r.flags & OR_BSDF_FAILED)
                        CL_CHECK(r.f[0] == 0.0f && r.pdfForward == 0.0f && r.pdfReverse == 0.0f); // a failed sample is all zero
                    if (co <= 0 && !(l.id == kClosureRefraction || l.id == kClosureMicrofacet))
                        CL_CHECK(r.flags & OR_BSDF_FAILED); // a one-sided lobe from below the horizon fails (a transmissive lobe may serve the inside)
                }
        }
    }
    memoryTini(arena);
}

static void sectionHorizonInvalid() {
    CMemPage *arena;
    memoryInit(arena);
    const Lobe l = allLobes()[0];
    OrBsdf *bsdf = prepareOne(arena, l);
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    const V3 good{0, 0, 1};
    for (const V3 &bad : {V3{nan, 0, 1}, V3{0, nan, 1}, V3{0, 0, nan}, V3{inf, 0, 1}, V3{0, -inf, 0}}) {
        OrBsdfEval e;
        std::memset(&e, 0, sizeof(e));
        OrBsdfSample r;
        std::memset(&r, 0, sizeof(r));
        const OrBsdfQuery q1 = makeQuery(bad, good, OR_TRANSPORT_RADIANCE), q2 = makeQuery(good, bad, OR_TRANSPORT_RADIANCE);
        CL_CHECK_INT(orBsdfTable()->evaluate(bsdf, &q1, &e), OR_STATUS_BAD_ARGUMENT);
        CL_CHECK_INT(orBsdfTable()->evaluate(bsdf, &q2, &e), OR_STATUS_BAD_ARGUMENT);
        CL_CHECK_INT(orBsdfTable()->pdf(bsdf, &q1, &e), OR_STATUS_BAD_ARGUMENT);
        CL_CHECK_INT(orBsdfTable()->pdf(bsdf, &q2, &e), OR_STATUS_BAD_ARGUMENT);
        CL_CHECK_INT(orBsdfTable()->sample(bsdf, &q1, 0.3f, 0.4f, 0.5f, &r), OR_STATUS_BAD_ARGUMENT);
    }
    // a NaN or an out-of-range random number is the caller's error too
    OrBsdfSample r;
    const OrBsdfQuery q = makeQuery(good, good, OR_TRANSPORT_RADIANCE);
    CL_CHECK_INT(orBsdfTable()->sample(bsdf, &q, nan, 0.4f, 0.5f, &r), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(orBsdfTable()->sample(bsdf, &q, 0.3f, 1.5f, 0.5f, &r), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(orBsdfTable()->sample(bsdf, &q, 0.3f, 0.4f, -0.1f, &r), OR_STATUS_BAD_ARGUMENT);
    memoryTini(arena);
}

static void sectionNotScatterer() {
    CMemPage *arena;
    memoryInit(arena);
    ClosureTreeBuilder builder(arena, 4, nullptr);
    const float w[3] = {0.5f, 0.5f, 0.5f};
    ClosureUniformEdfBlock edf = {{1, 2, 3}};
    const OrClosureNode *tree = builder.add(builder.add(builder.component(kClosureTransparent, w, nullptr, 0), builder.component(kClosureHoldout, w, nullptr, 0)),
                                            builder.component(kClosureUniformEdf, w, &edf, sizeof(edf)));
    for (const OrClosureNode *t : {tree, (const OrClosureNode *)nullptr}) {
        OrBsdf *bsdf = nullptr;
        CL_CHECK_INT(prepareTree(t, arena, &bsdf), OR_STATUS_OK);
        CL_CHECK(bsdf != nullptr);
        const Subject s = tableSubject("emission and signal only", bsdf);
        const V3 wo = woFor(0.8), wi = woFor(0.6);
        const OrBsdfEval e = s.evaluate(wo, wi, OR_TRANSPORT_RADIANCE), q = s.pdf(wo, wi, OR_TRANSPORT_RADIANCE);
        CL_CHECK(e.f[0] == 0.0f && e.pdfForward == 0.0f && e.pdfReverse == 0.0f && (e.flags & OR_BSDF_NOT_SCATTERER));
        CL_CHECK(q.pdfForward == 0.0f && (q.flags & OR_BSDF_NOT_SCATTERER));
        const OrBsdfSample r = s.sample(wo, 0.3f, 0.4f, 0.5f, OR_TRANSPORT_RADIANCE);
        CL_CHECK(r.f[0] == 0.0f && r.pdfForward == 0.0f && (r.flags & OR_BSDF_NOT_SCATTERER));
        CL_CHECK(!(e.flags & OR_BSDF_FAILED));
    }
    // a tree that scatters is not flagged
    ClosureDiffuseBlock d = {{0, 0, 1}};
    OrBsdf *bsdf = nullptr;
    CL_CHECK_INT(prepareTree(builder.add(builder.component(kClosureDiffuse, w, &d, sizeof(d)), builder.component(kClosureUniformEdf, w, &edf, sizeof(edf))), arena, &bsdf), OR_STATUS_OK);
    CL_CHECK(!(tableSubject("diffuse and emission", bsdf).evaluate(woFor(0.8), woFor(0.6), OR_TRANSPORT_RADIANCE).flags & OR_BSDF_NOT_SCATTERER));
    memoryTini(arena);
}

static void sectionImportanceFactor() {
    const float eta = 1.5f;
    const char *ggx = internClosureString("ggx");
    cltest::KeyedRandom rnd(0x1FAC7001ull);
    // a transmissive lobe: wo and wi on opposite sides. The radiance-mode f is the importance-mode f times (eta_wo / eta_wi)^2
    for (int refract : {1, 2}) {
        ClosureMicrofacetBlock b = {ggx, {0, 0, 1}, 0.4f, eta, refract};
        const Subject s = componentSubject("microfacet transmissive", kClosureMicrofacet, &b);
        int checked = 0;
        for (int i = 0; i < 4000; ++i) {
            const V3 a = uniformSphere(rnd.next(), rnd.next()), c = uniformSphere(rnd.next(), rnd.next());
            if ((a.z > 0) == (c.z > 0))
                continue; // opposite sides only
            const double fi = s.evaluate(a, c, OR_TRANSPORT_IMPORTANCE).f[0], fr = s.evaluate(a, c, OR_TRANSPORT_RADIANCE).f[0];
            if (fi == 0.0 || fr == 0.0)
                continue;
            ++checked;
            const double woIndex = a.z > 0 ? 1.0 : eta, wiIndex = a.z > 0 ? eta : 1.0; // wo outside: index 1; wo inside the dense medium: 1.5
            const double factor = (woIndex / wiIndex) * (woIndex / wiIndex);
            CL_CHECK_NEAR(fr, fi * factor, floatTol(fr, fi * factor));
        }
        CL_CHECK(checked > 200);
    }
    // the delta pair: entering from outside divides by 2.25, from inside multiplies by it (the sampled f, which carries the same factor)
    ClosureRefractionBlock rb = {{0, 0, 1}, eta};
    const Subject d = componentSubject("refraction", kClosureRefraction, &rb);
    const V3 outside = normalized(V3{0.3f, 0.1f, 0.9f}), inside = normalized(V3{0.2f, 0.1f, -0.9f});
    for (const V3 &wo : {outside, inside}) {
        const OrBsdfSample ri = d.sample(wo, 0.4f, 0.4f, 0.5f, OR_TRANSPORT_IMPORTANCE), rr = d.sample(wo, 0.4f, 0.4f, 0.5f, OR_TRANSPORT_RADIANCE);
        const double factor = wo.z > 0 ? 1.0 / (eta * eta) : eta * eta;
        CL_CHECK_NEAR(rr.f[0], ri.f[0] * factor, floatTol(rr.f[0], ri.f[0] * factor) * 4);
        CL_CHECK_NEAR(factor, wo.z > 0 ? 1.0 / 2.25 : 2.25, 1e-6); // the figure of the contract: 2.25 from glass to air
    }
    // reflection is the same in both modes, bit for bit: diffuse, oren_nayar, a reflection-only microfacet, the reflection half of a dielectric
    ClosureDiffuseBlock db = {{0, 0, 1}};
    ClosureOrenNayarBlock ob = {{0, 0, 1}, 0.6f};
    ClosureMicrofacetBlock mr = {ggx, {0, 0, 1}, 0.3f, eta, 0}, mboth = {ggx, {0, 0, 1}, 0.3f, eta, 2};
    const Subject refl[] = {componentSubject("diffuse", kClosureDiffuse, &db), componentSubject("oren_nayar", kClosureOrenNayar, &ob), componentSubject("microfacet reflect", kClosureMicrofacet, &mr),
                            componentSubject("microfacet both, reflection side", kClosureMicrofacet, &mboth)};
    for (const Subject &s : refl)
        for (int i = 0; i < 500; ++i) {
            const V3 a = uniformUpperHemisphere(rnd.next(), rnd.next()), c = uniformUpperHemisphere(rnd.next(), rnd.next());
            const OrBsdfEval er = s.evaluate(a, c, OR_TRANSPORT_RADIANCE), ei = s.evaluate(a, c, OR_TRANSPORT_IMPORTANCE);
            CL_CHECK_BITS(er.f[0], ei.f[0]);
            CL_CHECK_BITS(er.pdfForward, ei.pdfForward);
        }
}

static void sectionStatusTable() {
    const OrBsdfTable *t = orBsdfTable();
    CL_CHECK(t != nullptr);
    CL_CHECK_INT(t->header.structSize, sizeof(OrBsdfTable));
    CL_CHECK_INT(t->header.abiVersion, OR_CLOSURE_ABI_VERSION);
    CL_CHECK(t->prepare && t->evaluate && t->sample && t->pdf);
    CMemPage *arena;
    memoryInit(arena);
    const Lobe l = allLobes()[0];
    OrBsdf *bsdf = prepareOne(arena, l);
    const V3 wo = woFor(0.8), wi = woFor(0.6);
    OrBsdfQuery q = makeQuery(wo, wi, OR_TRANSPORT_RADIANCE);
    OrBsdfEval e;
    OrBsdfSample r;
    // an answer with FAILED is still OK: a sample from below the horizon
    const OrBsdfQuery below = makeQuery(V3{0, 0, -1}, wi, OR_TRANSPORT_RADIANCE);
    CL_CHECK_INT(t->sample(bsdf, &below, 0.3f, 0.4f, 0.5f, &r), OR_STATUS_OK);
    CL_CHECK(r.flags & OR_BSDF_FAILED);
    CL_CHECK_INT(t->evaluate(bsdf, &q, &e), OR_STATUS_OK);
    // null pointers
    CL_CHECK_INT(t->evaluate(nullptr, &q, &e), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->evaluate(bsdf, nullptr, &e), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->evaluate(bsdf, &q, nullptr), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->pdf(nullptr, &q, &e), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->pdf(bsdf, nullptr, &e), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->pdf(bsdf, &q, nullptr), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->sample(nullptr, &q, 0.3f, 0.4f, 0.5f, &r), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->sample(bsdf, nullptr, 0.3f, 0.4f, 0.5f, &r), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->sample(bsdf, &q, 0.3f, 0.4f, 0.5f, nullptr), OR_STATUS_BAD_ARGUMENT);
    // a structSize smaller than 1.0.0's query
    q.structSize = sizeof(OrBsdfQuery) - 4;
    CL_CHECK_INT(t->evaluate(bsdf, &q, &e), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->pdf(bsdf, &q, &e), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->sample(bsdf, &q, 0.3f, 0.4f, 0.5f, &r), OR_STATUS_BAD_ARGUMENT);
    q.structSize = 0;
    CL_CHECK_INT(t->evaluate(bsdf, &q, &e), OR_STATUS_BAD_ARGUMENT);
    // a larger structSize (a newer caller) is accepted: only the 1.0.0 prefix is read
    q = makeQuery(wo, wi, OR_TRANSPORT_RADIANCE);
    q.structSize = sizeof(OrBsdfQuery) + 16;
    CL_CHECK_INT(t->evaluate(bsdf, &q, &e), OR_STATUS_OK);
    // prepare
    OrShadingFrame frame = {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}};
    OrClosureAllocator alloc = {&arena, arenaAllocate};
    OrBsdf *out = nullptr;
    CL_CHECK_INT(t->prepare(nullptr, nullptr, &alloc, &out), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->prepare(nullptr, &frame, nullptr, &out), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->prepare(nullptr, &frame, &alloc, nullptr), OR_STATUS_BAD_ARGUMENT);
    CL_CHECK_INT(t->prepare(nullptr, &frame, &alloc, &out), OR_STATUS_OK); // the empty tree prepares (it is not a scatterer)
    // an allocator that fails
    OrClosureAllocator failing = {nullptr, [](void *, size_t, size_t) -> void * { return nullptr; }};
    ClosureTreeBuilder builder(arena, 4, nullptr);
    const float w[3] = {1, 1, 1};
    ClosureDiffuseBlock d = {{0, 0, 1}};
    OrBsdf *none = nullptr;
    CL_CHECK_INT(t->prepare(builder.component(kClosureDiffuse, w, &d, sizeof(d)), &frame, &failing, &none), OR_STATUS_OUT_OF_MEMORY);
    // Ng reports a side and rejects nothing: flipping it, or putting wi on the other side of it from wo, changes no bit of the answer
    OrBsdfQuery a = makeQuery(wo, wi, OR_TRANSPORT_RADIANCE), b = a;
    b.Ng[2] = -1.0f;
    OrBsdfEval ea, eb;
    CL_CHECK_INT(t->evaluate(bsdf, &a, &ea), OR_STATUS_OK);
    CL_CHECK_INT(t->evaluate(bsdf, &b, &eb), OR_STATUS_OK);
    CL_CHECK(std::memcmp(&ea, &eb, sizeof(ea)) == 0);
    memoryTini(arena);
}

static void sectionFrame() {
    CMemPage *arena;
    memoryInit(arena);
    ClosureTreeBuilder builder(arena, 4, nullptr);
    const float w[3] = {1, 1, 1};
    ClosureDiffuseBlock tilted = {{1, 0, 0}}; // the component's own normal is +x; the shading frame (z) is not its frame
    OrBsdf *bsdf = nullptr;
    CL_CHECK_INT(prepareTree(builder.component(kClosureDiffuse, w, &tilted, sizeof(tilted)), arena, &bsdf), OR_STATUS_OK);
    const Subject s = tableSubject("diffuse with N = +x", bsdf);
    const V3 x{1, 0, 0}, nx{-1, 0, 0}, z{0, 0, 1}, mix = normalized(V3{0.6f, 0.0f, 0.8f});
    CL_CHECK_NEAR(s.evaluate(x, x, OR_TRANSPORT_RADIANCE).f[0], 1.0 / kPi, floatTol(1.0 / kPi, 1.0 / kPi));
    CL_CHECK_NEAR(s.evaluate(mix, mix, OR_TRANSPORT_RADIANCE).pdfForward, (0.6 / kPi), floatTol(0.6 / kPi, 0.6 / kPi)); // cos with +x is 0.6
    CL_CHECK(s.evaluate(nx, x, OR_TRANSPORT_RADIANCE).f[0] == 0.0f);   // wo below the lobe's horizon
    CL_CHECK(s.evaluate(x, nx, OR_TRANSPORT_RADIANCE).f[0] == 0.0f);
    CL_CHECK(s.evaluate(z, z, OR_TRANSPORT_RADIANCE).f[0] == 0.0f);    // z is on the lobe's horizon
    const OrBsdfSample r = s.sample(x, 0.3f, 0.6f, 0.5f, OR_TRANSPORT_RADIANCE);
    CL_CHECK(!(r.flags & OR_BSDF_FAILED) && r.wi[0] > 0.0f);           // the sample lives around +x
    // a component with no usable normal (zero) takes the shading frame's
    ClosureDiffuseBlock zero = {{0, 0, 0}};
    OrBsdf *fallback = nullptr;
    CL_CHECK_INT(prepareTree(builder.component(kClosureDiffuse, w, &zero, sizeof(zero)), arena, &fallback), OR_STATUS_OK);
    CL_CHECK_NEAR(tableSubject("diffuse, N zero", fallback).evaluate(z, z, OR_TRANSPORT_RADIANCE).f[0], 1.0 / kPi, floatTol(1.0 / kPi, 1.0 / kPi));
    // a component normal that is not unit length is normalised
    ClosureDiffuseBlock longN = {{0, 0, 3}};
    OrBsdf *norm = nullptr;
    CL_CHECK_INT(prepareTree(builder.component(kClosureDiffuse, w, &longN, sizeof(longN)), arena, &norm), OR_STATUS_OK);
    CL_CHECK_NEAR(tableSubject("diffuse, N length 3", norm).evaluate(z, z, OR_TRANSPORT_RADIANCE).pdfForward, 1.0 / kPi, floatTol(1.0 / kPi, 1.0 / kPi));
    memoryTini(arena);
}

static void sectionWrapper() {
    CMemPage *arena;
    memoryInit(arena);
    ClosureTreeBuilder builder(arena, 4, nullptr);
    ClosureDiffuseBlock d = {{0, 0, 1}};
    const float w[3] = {0.5f, 0.25f, 1.0f};
    const OrClosureNode *tree = builder.component(kClosureDiffuse, w, &d, sizeof(d));
    OrShadingFrame frame = {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}};
    Bsdf bsdf = Bsdf::prepare(tree, frame, arena);
    CL_CHECK(bsdf.valid());
    const V3 wo = woFor(0.8), wi = woFor(0.6);
    const OrBsdfQuery q = makeQuery(wo, wi, OR_TRANSPORT_RADIANCE);
    const BsdfEvalResult e = bsdf.evaluate(q);
    CL_CHECK_INT(e.status, OR_STATUS_OK);
    for (int c = 0; c < openrender::kSpectrumChannels; ++c)
        CL_CHECK_NEAR(e.f.c[c], w[c] / kPi, floatTol(w[c] / kPi, w[c] / kPi)); // the weight per channel over pi
    const BsdfEvalResult p = bsdf.pdf(q);
    CL_CHECK_NEAR(p.pdfForward, wi.z / kPi, floatTol(wi.z / kPi, wi.z / kPi));
    CL_CHECK(p.f.isZero());
    const BsdfSampleResult r = bsdf.sample(q, 0.3f, 0.6f, 0.5f);
    CL_CHECK_INT(r.status, OR_STATUS_OK);
    CL_CHECK(r.f.isFinite() && r.pdfForward > 0);
    // a tree that cannot be prepared (a refusing allocator is the only way left: the table checked it) gives an invalid handle
    CL_CHECK(!Bsdf().valid());
    memoryTini(arena);
}

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    const char *testName = "";
    if (which == "HorizonGrid") {
        sectionHorizonGrid();
        testName = "Bsdf_Horizon_Grid";
    }
    else if (which == "HorizonInvalid") {
        sectionHorizonInvalid();
        testName = "Bsdf_Horizon_Invalid";
    }
    else if (which == "NotScatterer") {
        sectionNotScatterer();
        testName = "Bsdf_NotScatterer";
    }
    else if (which == "ImportanceFactor") {
        sectionImportanceFactor();
        testName = "Bsdf_ImportanceFactor";
    }
    else if (which == "StatusTable") {
        sectionStatusTable();
        testName = "Bsdf_Status_Table";
    }
    else if (which == "Frame") {
        sectionFrame();
        testName = "Bsdf_Frame";
    }
    else if (which == "Wrapper") {
        sectionWrapper();
        testName = "Bsdf_Wrapper";
    }
    else {
        std::printf("usage: test_bsdf_conventions <HorizonGrid|HorizonInvalid|NotScatterer|ImportanceFactor|StatusTable|Frame|Wrapper>\n");
        return 2;
    }
    return CL_RESULT(testName);
}
