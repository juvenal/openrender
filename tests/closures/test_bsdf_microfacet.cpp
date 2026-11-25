/*
 * Spec 035, Bsdf_microfacet_* and Bsdf_RoughnessFloor (label closures): the `microfacet` component, GGX and Beckmann, `refract` 0
 * (reflection), 1 (transmission) and 2 (both, Fresnel weighted: reflection F, transmission 1 - F), eta in {0, 1.0001, 1.5} (eta 0 is
 * "no Fresnel term, F = 1" and exists for refract 0 only), alpha in {0.05, 0.3, 0.9}.
 *
 *   Bsdf_microfacet_Math        the analytic checks of bsdfMath.h: the integral of D cos theta_m over the hemisphere is 1, the Fresnel limits
 *   Bsdf_microfacet_{Reciprocity,Energy,PdfNorm,SamplePdf,ChiSquare,ReversePdf}   the six checks over the whole grid. Reciprocity runs in
 *                               both transport modes (f(a, b, m) == f(b, a, dual m) for the transmissive lobes, plus the plain symmetry
 *                               for refract 0). Energy runs in IMPORTANCE mode (A1). alpha 0.05 is too peaked for a uniform-sphere
 *                               estimator to have a usable sigma, so the cross-estimator comparison, the density normalisation and the
 *                               chi-square (whose cell integral a 48 x 48 midpoint rule cannot resolve at that width) run for alpha >= 0.3;
 *                               alpha 0.05 keeps reciprocity, the sampled albedo bound, sample consistency and the reverse density.
 *                               eta 1.0001 with refract 1 or 2 runs the sampled estimator only (no cross-estimator, no chi-square, no
 *                               1 + 3 sigma bound), and keeps reciprocity, sample consistency and the reverse density
 *                               ChiSquare integrates each cell of the density with the plain 48 x 48 midpoint rule except that the two cells
 *                               at the poles are integrated in theta and a sub-cell holding a jump of the density is split at it
 *                               (integrateCells, microfacetEdges below); no cell is left out of the statistic. Each row is tested at
 *                               the per-row level that keeps the family-wise level at 0.001 over the rows the test runs (see main)
 *                               SamplePdf skips the refract 1, alpha 0.05, eta 1.5 rows with wo inside the medium (wo.z -0.60, both distributions):
 *                               transmission out of the medium needs a microfacet tilt of 11.4 degrees or more, and the lobe's mass there is
 *                               about exp(-16) (Beckmann: 0 usable samples of 20000; GGX: 303), too few for a pointwise comparison
 *   Bsdf_microfacet_Refusal     eta 0 with refract 1 or 2 is refused when the node is built (the builder returns nullptr) and by prepare
 *                               on a forged node (OR_STATUS_BAD_ARGUMENT): Phase 1's test, repeated at the BSDF level
 *   Bsdf_RoughnessFloor         below kMicrofacetDeltaAlpha (1e-4) the lobe is the `reflection`/`refraction` delta it converges to: no
 *                               density, the delta flag, the same sample; at and above it every density is finite
 *
 * Written red first: bsdfMath.h and the components' operations do not exist yet.
 */
#include <cmath>
#include <cstring>
#include <string>

#include "bsdf_checks.h"
#include "bsdfMath.h"
#include "closureBuiltins.h"
#include "closureTree.h"

using namespace clbsdf;
using namespace openrender::shading;

struct Config {
        const char *distribution;
        int refract;
        float eta, alpha;
};

static std::vector<Config> grid() {
    std::vector<Config> g;
    for (const char *d : {"ggx", "beckmann"})
        for (int r : {0, 1, 2})
            for (float a : {0.05f, 0.3f, 0.9f}) {
                if (r == 0)
                    for (float eta : {0.0f, 1.0001f, 1.5f})
                        g.push_back({d, r, eta, a});
                else
                    for (float eta : {1.0001f, 1.5f})
                        g.push_back({d, r, eta, a});
            }
    return g;
}

// The z of the jumps of a microfacet lobe's density (a ring: every azimuth), for the chi-square quadrature (bsdf_checks.h integrateCells).
// Derived, then confirmed by a scan for jumps on every row (D6, 2026-11-25):
//  - A transmission lobe (refract 1 or 2) of a distribution whose D is not 0 at the horizon of the microfacet normal (GGX: D(90 deg) = alpha^2 / pi;
//    Beckmann: 0, so no jump) ends where the microfacet normal m is horizontal (m.z = 0, the boundary of the facet hemisphere). For a horizontal m
//    the refracted direction wi = -wo / eta_rel + (wo.m / eta_rel - cos theta_t) m has z = -wo.z / eta_rel, with eta_rel = eta_t / eta_i the index of
//    the medium wi lies in over the one wo lies in: eta for wo outside, 1 / eta for wo inside. So the density stops at wi.z = -wo.z / eta_rel; it
//    exists only while |wo.z| / eta_rel <= 1 (a wo inside with |wo.z| > 1 / eta has no horizontal normal that refracts: no edge).
//  - The critical cone (wo inside: wi.m = 0 where the refraction vanishes) is not a jump: the density has a factor |wi.m| and goes to 0 continuously.
//  - Known limitation: a Beckmann transmission lobe (D cos fallback density, no factor wo.m) has a hard support boundary on the curve
//    wo.m = 0, not a ring; its lowest z is -wo.z / eta_rel - sqrt(1 - 1 / eta_rel^2) sqrt(1 - wo.z^2) (-0.978831 for wo.z +0.5, eta 1.5). It is
//    not split, and the rows it touches pass.
//  - A reflection lobe ends at wi.z = 0 (below the horizon is refused): the cell boundary z = 0, never inside a sub-cell, so it needs no entry.
static std::vector<double> microfacetEdges(const Config &c, double woZ) {
    std::vector<double> edges;
    if (c.refract == 0 || std::strcmp(c.distribution, "ggx") != 0)
        return edges;
    const double etaRel = woZ > 0 ? (double)c.eta : 1.0 / (double)c.eta;
    const double z = -woZ / etaRel;
    if (std::fabs(z) <= 1.0)
        edges.push_back(z);
    return edges;
}

static std::string describe(const Config &c) {
    char t[100];
    std::snprintf(t, sizeof(t), "microfacet %s refract %d eta %g alpha %g", c.distribution, c.refract, c.eta, c.alpha);
    return t;
}

static void sectionMath() {
    // D is a density over the microfacet normals: the integral of D(m) cos(theta_m) over the hemisphere is 1 (midpoint rule over theta)
    for (float alpha : {0.05f, 0.3f, 0.9f})
        for (int dist = 0; dist < 2; ++dist) {
            const int n = 200000;
            double sum = 0;
            for (int i = 0; i < n; ++i) {
                const double theta = (i + 0.5) / n * (kPi / 2);
                const float c = (float)std::cos(theta);
                const double d = dist == 0 ? bsdf::ggxD(alpha, c) : bsdf::beckmannD(alpha, c);
                sum += d * c * std::sin(theta) * (kPi / 2) / n * 2.0 * kPi;
            }
            std::printf("  D normalisation %s alpha %g: %.6f\n", dist == 0 ? "ggx" : "beckmann", alpha, sum);
            CL_CHECK_NEAR(sum, 1.0, floatTol(1.0, 1.0));
        }
    // Fresnel: eta = n_t / n_i for a positive cosine; a negative cosine is the ray inside the denser medium
    CL_CHECK_NEAR(bsdf::fresnelDielectric(1.0f, 1.5f), 0.04, floatTol(0.04, 0.04));   // ((1.5 - 1) / (1.5 + 1))^2
    CL_CHECK_NEAR(bsdf::fresnelDielectric(1.0f, 1.0f), 0.0, 1e-7);                    // no interface
    CL_CHECK_NEAR(bsdf::fresnelDielectric(0.5f, 0.0f), 1.0, 1e-7);                    // eta 0 is "no Fresnel term": F = 1
    CL_CHECK_NEAR(bsdf::fresnelDielectric(1e-6f, 1.5f), 1.0, 1e-3);                   // grazing
    CL_CHECK_NEAR(bsdf::fresnelDielectric(-1.0f, 1.5f), 0.04, floatTol(0.04, 0.04));  // from inside at normal incidence: the same 4 %
    CL_CHECK_NEAR(bsdf::fresnelDielectric(-0.3f, 1.5f), 1.0, 1e-7);                   // inside, beyond the critical angle (sin 0.954 > 1/1.5): total internal reflection
    float last = -1;
    for (int i = 0; i <= 100; ++i) { // F grows as the angle grows, from outside
        const float f = bsdf::fresnelDielectric(1.0f - 0.0099f * i, 1.5f);
        CL_CHECK(f >= last - 1e-6f && f >= 0.0f && f <= 1.0f);
        last = f;
    }
    CL_CHECK_BITS(bsdf::kMicrofacetDeltaAlpha, 1e-4f);
}

static void sectionRefusal() {
    CMemPage *arena;
    memoryInit(arena);
    ClosureTreeBuilder builder(arena, 16, nullptr);
    const float one[3] = {1, 1, 1};
    const char *ggx = internClosureString("ggx");
    for (int refract : {1, 2}) {
        ClosureMicrofacetBlock m = {ggx, {0, 0, 1}, 0.3f, 0.0f, refract};
        CL_CHECK(builder.component(kClosureMicrofacet, one, &m, sizeof(m)) == nullptr); // refused when the node is built
        // a node forged without the builder (bytes in the arena, as a plug-in or a bad writer could) is refused by prepare
        char *raw = (char *)ralloc((int)(sizeof(OrClosureComponent) + sizeof(m) + 16), arena);
        OrClosureComponent *c = (OrClosureComponent *)(((uintptr_t)raw + 15) & ~(uintptr_t)15);
        c->id = kClosureMicrofacet;
        c->w[0] = c->w[1] = c->w[2] = 1.0f;
        std::memcpy(c + 1, &m, sizeof(m));
        const OrBsdfTable *table = orBsdfTable();
        OrShadingFrame frame = {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}};
        OrClosureAllocator alloc = {&arena, [](void *ctx, size_t size, size_t) -> void * { return ralloc((int)size, *(CMemPage **)ctx); }};
        OrBsdf *bsdfOut = nullptr;
        CL_CHECK_INT(table->prepare((const OrClosureNode *)c, &frame, &alloc, &bsdfOut), OR_STATUS_BAD_ARGUMENT);
    }
    // eta 0 with refract 0 is fine (no Fresnel term)
    ClosureMicrofacetBlock ok = {ggx, {0, 0, 1}, 0.3f, 0.0f, 0};
    CL_CHECK(builder.component(kClosureMicrofacet, one, &ok, sizeof(ok)) != nullptr);
    memoryTini(arena);
}

static void sectionRoughnessFloor() {
    const char *ggx = internClosureString("ggx");
    cltest::KeyedRandom rnd(0xF100A000ull);
    // below the floor the lobe is the delta reflection/refraction it converges to
    for (float eta : {0.0f, 1.5f}) {
        ClosureMicrofacetBlock m = {ggx, {0, 0, 1}, 5e-5f, eta, 0};
        ClosureReflectionBlock r = {{0, 0, 1}, eta};
        const Subject a = componentSubject("microfacet below the floor", kClosureMicrofacet, &m), b = componentSubject("reflection", kClosureReflection, &r);
        for (int i = 0; i < 200; ++i) {
            const V3 wo = uniformUpperHemisphere(rnd.next(), rnd.next()), wi = uniformUpperHemisphere(rnd.next(), rnd.next());
            if (wo.z < 1e-3f)
                continue;
            const OrBsdfEval e = a.evaluate(wo, wi, OR_TRANSPORT_IMPORTANCE);
            CL_CHECK(e.f[0] == 0.0f && e.pdfForward == 0.0f && e.pdfReverse == 0.0f);
            CL_CHECK(e.flags & OR_BSDF_HAS_DELTA);
            const float u1 = rnd.next(), u2 = rnd.next();
            const OrBsdfSample x = a.sample(wo, u1, u2, 0.5f, OR_TRANSPORT_IMPORTANCE), y = b.sample(wo, u1, u2, 0.5f, OR_TRANSPORT_IMPORTANCE);
            CL_CHECK(x.flags & OR_BSDF_DELTA);
            for (int k = 0; k < 3; ++k)
                CL_CHECK_BITS(x.wi[k], y.wi[k]);
            CL_CHECK_BITS(x.f[0], y.f[0]);
            CL_CHECK_INT(x.flags, y.flags);
        }
    }
    {
        ClosureMicrofacetBlock m = {ggx, {0, 0, 1}, 5e-5f, 1.5f, 1};
        ClosureRefractionBlock r = {{0, 0, 1}, 1.5f};
        const Subject a = componentSubject("microfacet below the floor, refract 1", kClosureMicrofacet, &m), b = componentSubject("refraction", kClosureRefraction, &r);
        for (int i = 0; i < 200; ++i) {
            const V3 wo = uniformUpperHemisphere(rnd.next(), rnd.next());
            if (wo.z < 1e-3f)
                continue;
            const float u1 = rnd.next(), u2 = rnd.next();
            const OrBsdfSample x = a.sample(wo, u1, u2, 0.5f, OR_TRANSPORT_RADIANCE), y = b.sample(wo, u1, u2, 0.5f, OR_TRANSPORT_RADIANCE);
            CL_CHECK_INT(x.flags, y.flags);
            for (int k = 0; k < 3; ++k)
                CL_CHECK_BITS(x.wi[k], y.wi[k]);
            CL_CHECK_BITS(x.f[0], y.f[0]);
        }
    }
    // at and above the floor the density is finite everywhere (no density that grows without bound as alpha falls toward it)
    for (float alpha : {1.0e-4f, 1.1e-4f, 2e-4f, 1e-3f}) {
        ClosureMicrofacetBlock m = {ggx, {0, 0, 1}, alpha, 1.5f, 0};
        const Subject s = componentSubject("microfacet at the floor", kClosureMicrofacet, &m);
        const V3 wo = normalized(V3{0.3f, 0.2f, 0.8f}), mirror = normalized(V3{-0.3f, -0.2f, 0.8f});
        for (const V3 &wi : {mirror, normalized(V3{-0.31f, -0.2f, 0.8f}), V3{0, 0, 1}}) {
            const OrBsdfEval e = s.evaluate(wo, wi, OR_TRANSPORT_IMPORTANCE);
            CL_CHECK(std::isfinite(e.f[0]) && e.f[0] >= 0 && std::isfinite(e.pdfForward) && e.pdfForward >= 0 && std::isfinite(e.pdfReverse));
            CL_CHECK(!(e.flags & (OR_BSDF_HAS_DELTA | OR_BSDF_DELTA))); // alpha = 1e-4 itself is NOT below the floor
        }
    }
}

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "Math") {
        sectionMath();
        return CL_RESULT("Bsdf_microfacet_Math");
    }
    if (which == "Refusal") {
        sectionRefusal();
        return CL_RESULT("Bsdf_microfacet_Refusal");
    }
    if (which == "RoughnessFloor") {
        sectionRoughnessFloor();
        return CL_RESULT("Bsdf_RoughnessFloor");
    }
    // The options of a configuration (the same for every section but for SamplePdf's wo list, D5)
    const auto optionsFor = [&which](const Config &c) {
        Options o;
        if (c.refract != 0)
            o.woCos = {0.95, 0.5, 0.12, -0.6}; // outside and inside
        if (c.alpha < 0.2f) {
            o.crossEstimators = false;
            o.chiSquare = false;
        }
        o.zEdges = [c](double woZ) { return microfacetEdges(c, woZ); };
        if (c.refract != 0 && c.eta < 1.001f) {
            // eta 1.0001 with a transmissive lobe: the half vector of a near-straight-through pair is ill conditioned, so the density is a
            // spike a uniform-sphere estimator cannot resolve and the sampled weight has no usable sigma: only the sampled estimator, with
            // no 1 + 3 sigma bound, as for alpha 0.05 (D3, maintainer 2026-11-25)
            o.crossEstimators = false;
            o.chiSquare = false;
            o.albedoBound = false;
        }
        if (which == "SamplePdf" && c.refract == 1 && c.alpha < 0.2f && c.eta > 1.0f + 1e-3f)
            o.woCos = {0.95, 0.5, 0.12}; // D5: no wo inside the medium, see the header
        return o;
    };
    // ChiSquare makes one decision per (configuration, wo) row, n of them, each at significance 0.001: the chance that some row of a correct
    // implementation fails is then about n * 0.001 (6.6% for the 68 rows of this grid), on every platform and with every fixed seed. The
    // per-row level is therefore the family-wise 0.001 divided over the n rows the test actually runs (Sidak: 1 - (1 - 0.001)^(1/n), counted
    // here at run time, not written down). This is a multiple-testing correction and nothing else: no tolerance was widened for a defect (a
    // calibration of the statistic on other seeds, 600 on Linux and 61 on macOS, gave a mean of 23.0 and 22.3 for 23 degrees of freedom, a
    // standard deviation of 6.43 for 6.78, and no exceedance of the 0.001 value; validation.md, D7). One macOS row, beckmann refract 2 eta 1.5
    // alpha 0.3 wo.z +0.50, read 55.0 (51.35 with 192 sub-cells) against 49.8 at 0.001, a tail of about 5e-4 of a fixed seed.
    int chiRows = 0;
    if (which == "ChiSquare")
        for (const Config &c : grid()) {
            const Options o = optionsFor(c);
            if (!o.chiSquare)
                continue;
            for (double wo : o.woCos)
                if (!(c.refract == 0 && wo < 0))
                    ++chiRows;
        }
    const double chiLevel = chiRows > 0 ? -std::expm1(std::log1p(-0.001) / chiRows) : 0.001;
    if (chiRows > 0)
        std::printf("chi-square family: %d rows, per-row level %.4g (Sidak, family-wise 0.001)\n", chiRows, chiLevel);
    bool known = true;
    uint64_t index = 0;
    for (const Config &c : grid()) {
        ClosureMicrofacetBlock block = {internClosureString(c.distribution), {0, 0, 1}, c.alpha, c.eta, c.refract};
        Subject s = componentSubject("microfacet", kClosureMicrofacet, &block);
        s.name = describe(c);
        s.reflectionOnly = c.refract == 0;
        s.lossless = false;
        Options o = optionsFor(c);
        if (chiRows > 0)
            o.chiLevel = chiLevel;
        known = runCheckSection(which, s, o, 0x61C20000ull + index++) && known;
        if (!known)
            break;
    }
    if (!known) {
        std::printf("usage: test_bsdf_microfacet <Math|Refusal|RoughnessFloor|Reciprocity|Energy|PdfNorm|SamplePdf|ChiSquare|ReversePdf>\n");
        return 2;
    }
    return CL_RESULT(("Bsdf_microfacet_" + which).c_str());
}
