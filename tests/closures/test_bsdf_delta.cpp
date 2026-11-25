/*
 * Spec 035, Bsdf_reflection_*, Bsdf_refraction_* and Bsdf_Delta_* (label closures): the delta lobes (the `reflection` and `refraction`
 * closures) and a tree that holds one (contracts/closure-abi.md "BSDF table", conventions 3 and 4).
 *
 *   Bsdf_reflection_Values   `reflection`: evaluate and pdf give nothing and set OR_BSDF_HAS_DELTA; sample returns the mirror direction, f the
 *                            Fresnel-weighted delta value divided by |cos theta_i| (F / |cos|, or 1 / |cos| for eta 0), the delta flag, and
 *                            pdfForward == pdfReverse == 1 for the component alone (the table replaces them by the lobe-choice probability)
 *   Bsdf_refraction_Values   `refraction`: the Snell direction, f = (1 - F) / |cos theta_i| times the radiance-mode factor (eta_wo/eta_wi)^2,
 *                            from outside and from inside; total internal reflection leaves a refraction-only lobe with a failed sample
 *   Bsdf_Delta_Pair          reflection + refraction of one index in a tree: evaluate and pdf give 0 with HAS_DELTA; sample chooses between the two,
 *                            pdfForward is the DISCRETE choice probability (they sum to 1 and match the choice frequencies within 3 sigma), and in
 *                            IMPORTANCE mode the estimator f |cos| / p averages 1 (F + (1 - F), the furnace) within 3 sigma
 *   Bsdf_Delta_Mixed         diffuse + a mirror (eta 0) in one tree: the lobe-choice probability is proportional to mean(weight) * albedo(wo)
 *                            (convention 4), evaluate omits the delta lobe and reports it, the density is p_diffuse cos / pi, `requested` removes
 *                            lobes from the choice and the renormalisation, and sample may choose the delta lobe
 *
 * Written red first: bsdfMath.h, bsdf.hpp and the components' operations do not exist yet.
 */
#include <string>

#include "bsdf_checks.h"
#include "bsdfMath.h"

using namespace clbsdf;
using namespace openrender::shading;

static void sectionReflection() {
    cltest::KeyedRandom rnd(0xDE17A001ull);
    for (float eta : {0.0f, 1.5f, 0.6667f}) {
        ClosureReflectionBlock b = {{0, 0, 1}, eta};
        const Subject s = componentSubject("reflection", kClosureReflection, &b);
        for (int i = 0; i < 500; ++i) {
            const V3 wo = uniformUpperHemisphere(rnd.next(), rnd.next()), wi = uniformUpperHemisphere(rnd.next(), rnd.next());
            if (wo.z < 1e-2f)
                continue;
            for (OrTransportMode m : {OR_TRANSPORT_RADIANCE, OR_TRANSPORT_IMPORTANCE}) {
                const OrBsdfEval e = s.evaluate(wo, wi, m), q = s.pdf(wo, wi, m);
                CL_CHECK(e.f[0] == 0.0f && e.pdfForward == 0.0f && e.pdfReverse == 0.0f);
                CL_CHECK(q.pdfForward == 0.0f && q.pdfReverse == 0.0f);
                CL_CHECK(e.flags & OR_BSDF_HAS_DELTA);
                CL_CHECK(q.flags & OR_BSDF_HAS_DELTA);
                const OrBsdfSample r = s.sample(wo, rnd.next(), rnd.next(), 0.5f, m);
                CL_CHECK(r.flags & OR_BSDF_DELTA);
                CL_CHECK(!(r.flags & (OR_BSDF_FAILED | OR_BSDF_TRANSMISSION)));
                CL_CHECK((r.flags & OR_BSDF_REFLECTION) && (r.flags & OR_BSDF_SPECULAR));
                CL_CHECK_BITS(r.wi[0], -wo.x); // the mirror direction, exactly
                CL_CHECK_BITS(r.wi[1], -wo.y);
                CL_CHECK_BITS(r.wi[2], wo.z);
                const double F = bsdf::fresnelDielectric(wo.z, eta); // eta 0: F = 1
                CL_CHECK_NEAR(r.f[0], F / wo.z, floatTol(F / wo.z, F / wo.z) * 4);
                CL_CHECK_BITS(r.pdfForward, 1.0f);
                CL_CHECK_BITS(r.pdfReverse, 1.0f);
            }
        }
        // below the horizon: a failed sample, nothing else
        const OrBsdfSample bad = s.sample(V3{0.6f, 0.0f, -0.8f}, 0.3f, 0.4f, 0.5f, OR_TRANSPORT_RADIANCE);
        CL_CHECK(bad.flags & OR_BSDF_FAILED);
        CL_CHECK(bad.f[0] == 0.0f && bad.pdfForward == 0.0f);
    }
}

static void sectionRefraction() {
    cltest::KeyedRandom rnd(0xDE17A002ull);
    const float eta = 1.5f;
    ClosureRefractionBlock b = {{0, 0, 1}, eta};
    const Subject s = componentSubject("refraction", kClosureRefraction, &b);
    for (int i = 0; i < 1000; ++i) {
        // from outside (wo above the surface, entering the denser medium) and from inside (wo below, leaving it)
        const bool inside = (i & 1) != 0;
        V3 wo = uniformUpperHemisphere(rnd.next(), rnd.next());
        if (wo.z < 1e-2f)
            continue;
        if (inside)
            wo.z = -wo.z;
        for (OrTransportMode m : {OR_TRANSPORT_RADIANCE, OR_TRANSPORT_IMPORTANCE}) {
            const OrBsdfEval e = s.evaluate(wo, V3{0, 0, -wo.z / std::fabs(wo.z)}, m);
            CL_CHECK(e.f[0] == 0.0f && e.pdfForward == 0.0f && e.pdfReverse == 0.0f);
            CL_CHECK(e.flags & OR_BSDF_HAS_DELTA);
            const OrBsdfSample r = s.sample(wo, rnd.next(), rnd.next(), 0.5f, m);
            const double ratio = inside ? eta : 1.0 / eta;                       // wo's medium index over wi's: the scale of the tangential part
            const double sinO2 = (double)wo.x * wo.x + (double)wo.y * wo.y;      // sin^2 of the angle of wo
            const bool tir = sinO2 * (inside ? eta * eta : 1.0 / (eta * eta)) > 1.0; // sin^2 of the transmitted angle would exceed 1
            if (tir) {
                CL_CHECK(r.flags & OR_BSDF_FAILED); // a refraction-only lobe has nothing to give past the critical angle
                CL_CHECK(r.f[0] == 0.0f && r.pdfForward == 0.0f);
                continue;
            }
            CL_CHECK(r.flags & OR_BSDF_DELTA);
            CL_CHECK(!(r.flags & (OR_BSDF_FAILED | OR_BSDF_REFLECTION)));
            CL_CHECK((r.flags & OR_BSDF_TRANSMISSION) && (r.flags & OR_BSDF_SPECULAR));
            CL_CHECK((wo.z > 0) != (r.wi[2] > 0)); // the other side of the surface
            // Snell's law in vector form: the tangential part of wi is -(the tangential part of wo) times (wo's index / wi's index)
            CL_CHECK_NEAR(r.wi[0], -wo.x * ratio, 1e-5);
            CL_CHECK_NEAR(r.wi[1], -wo.y * ratio, 1e-5);
            CL_CHECK_NEAR(r.eta, inside ? 1.0 / eta : eta, 1e-6); // the relative index of the sampled transmission (wi's over wo's)
            const double F = bsdf::fresnelDielectric(wo.z, eta);
            // value / |cos theta_i|; the radiance-mode factor is (eta_wo / eta_wi)^2, 1 in importance mode (the delta analogue of Bsdf_ImportanceFactor)
            const double expected = (1.0 - F) / std::fabs((double)r.wi[2]) * (m == OR_TRANSPORT_RADIANCE ? ratio * ratio : 1.0);
            CL_CHECK_NEAR(r.f[0], expected, floatTol(expected, expected) * 4);
            CL_CHECK_BITS(r.pdfForward, 1.0f);
        }
    }
    // total internal reflection with a LESS dense medium: eta 0.6667 entering at a grazing angle. The reflection closure of the same index gives
    // F = 1 there, so the pair still conserves energy
    ClosureRefractionBlock rb = {{0, 0, 1}, 0.6667f};
    ClosureReflectionBlock mb = {{0, 0, 1}, 0.6667f};
    const Subject refr = componentSubject("refraction eta 0.6667", kClosureRefraction, &rb), refl = componentSubject("reflection eta 0.6667", kClosureReflection, &mb);
    const V3 grazing = normalized(V3{0.9f, 0.0f, 0.2f});
    CL_CHECK(refr.sample(grazing, 0.5f, 0.5f, 0.5f, OR_TRANSPORT_IMPORTANCE).flags & OR_BSDF_FAILED);
    const OrBsdfSample mirror = refl.sample(grazing, 0.5f, 0.5f, 0.5f, OR_TRANSPORT_IMPORTANCE);
    CL_CHECK_NEAR(mirror.f[0] * mirror.wi[2], 1.0, 1e-5); // F = 1: all of it
}

static void sectionPair() {
    CMemPage *arena;
    memoryInit(arena);
    ClosureTreeBuilder builder(arena, 4, nullptr);
    ClosureReflectionBlock rb = {{0, 0, 1}, 1.5f};
    ClosureRefractionBlock tb = {{0, 0, 1}, 1.5f};
    const float one[3] = {1, 1, 1};
    const OrClosureNode *tree = builder.add(builder.component(kClosureReflection, one, &rb, sizeof(rb)), builder.component(kClosureRefraction, one, &tb, sizeof(tb)));
    OrBsdf *bsdf = nullptr;
    CL_CHECK_INT(prepareTree(tree, arena, &bsdf), OR_STATUS_OK);
    const Subject s = tableSubject("glass pair", bsdf);
    cltest::KeyedRandom rnd(0xDE17A003ull);
    for (double c : {0.9, 0.5, 0.15}) {
        const V3 wo = woFor(c);
        // evaluate and pdf give the delta lobes nothing and say so
        const OrBsdfEval e = s.evaluate(wo, uniformSphere(rnd.next(), rnd.next()), OR_TRANSPORT_IMPORTANCE);
        CL_CHECK(e.f[0] == 0.0f && e.pdfForward == 0.0f && (e.flags & OR_BSDF_HAS_DELTA));
        // the choice: the two probabilities sum to 1 and match the frequencies; the estimator averages F + (1 - F) = 1
        const int n = 200000;
        int reflected = 0;
        double pr = -1, pt = -1, sum = 0, sumSq = 0;
        for (int i = 0; i < n; ++i) {
            const OrBsdfSample r = s.sample(wo, rnd.next(), rnd.next(), rnd.next(), OR_TRANSPORT_IMPORTANCE);
            CL_CHECK(r.flags & OR_BSDF_DELTA);
            CL_CHECK(r.pdfForward > 0 && r.pdfForward <= 1);
            double x = (double)r.f[0] * std::fabs((double)r.wi[2]) / (double)r.pdfForward;
            if (r.flags & OR_BSDF_REFLECTION) {
                ++reflected;
                pr = r.pdfForward;
            }
            else
                pt = r.pdfForward;
            sum += x;
            sumSq += x * x;
        }
        CL_CHECK(pr > 0 && pt > 0);
        CL_CHECK_NEAR(pr + pt, 1.0, 1e-5); // the discrete probabilities of the two lobes
        const double freq = (double)reflected / n, sigma = std::sqrt(pr * (1 - pr) / n);
        CL_CHECK_NEAR(freq, pr, 3.0 * sigma + 1e-9);
        const Estimate a = fromSums(sum, sumSq, n);
        CL_CHECK_NEAR(a.value, 1.0, 3.0 * a.sigma + kRoundingFloor);
        std::printf("  delta pair wo.z %.2f: p_reflect %.5f p_transmit %.5f, frequency %.5f (sigma %.2e), albedo %.6f (sigma %.2e)\n", c, pr, pt, freq, sigma, a.value, a.sigma);
    }
    memoryTini(arena);
}

static void sectionMixed() {
    CMemPage *arena;
    memoryInit(arena);
    ClosureTreeBuilder builder(arena, 4, nullptr);
    ClosureDiffuseBlock db = {{0, 0, 1}};
    ClosureReflectionBlock mb = {{0, 0, 1}, 0.0f}; // a mirror with no Fresnel term: albedo 1
    const float wd[3] = {0.8f, 0.8f, 0.8f}, wm[3] = {0.2f, 0.2f, 0.2f};
    const OrClosureNode *tree = builder.add(builder.component(kClosureDiffuse, wd, &db, sizeof(db)), builder.component(kClosureReflection, wm, &mb, sizeof(mb)));
    OrBsdf *bsdf = nullptr;
    CL_CHECK_INT(prepareTree(tree, arena, &bsdf), OR_STATUS_OK);
    cltest::KeyedRandom rnd(0xDE17A004ull);
    const V3 wo = woFor(0.7);
    // lobe choice proportional to mean(weight) * albedo(wo): diffuse 0.8 * 1, mirror 0.2 * 1 => p_diffuse = 0.8
    const Subject all = tableSubject("diffuse + mirror", bsdf);
    for (int i = 0; i < 200; ++i) {
        const V3 wi = uniformUpperHemisphere(rnd.next(), rnd.next());
        if (wi.z < 1e-3f)
            continue;
        const OrBsdfEval e = all.evaluate(wo, wi, OR_TRANSPORT_IMPORTANCE);
        CL_CHECK_NEAR(e.f[0], 0.8 / kPi, floatTol(0.8 / kPi, 0.8 / kPi)); // the diffuse lobe weighted by 0.8; the delta lobe is omitted
        CL_CHECK_NEAR(e.pdfForward, 0.8 * wi.z / kPi, floatTol(e.pdfForward, 0.8 * wi.z / kPi)); // the choice probability is part of the density
        CL_CHECK_NEAR(e.pdfReverse, 0.8 * wo.z / kPi, floatTol(e.pdfReverse, 0.8 * wo.z / kPi));
        CL_CHECK(e.flags & OR_BSDF_HAS_DELTA); // and it says there is one
        CL_CHECK(e.flags & OR_BSDF_DIFFUSE);
    }
    // sample chooses the mirror with probability 0.2 and reports the discrete probability
    int mirrorChosen = 0;
    const int n = 100000;
    for (int i = 0; i < n; ++i) {
        const OrBsdfSample r = all.sample(wo, rnd.next(), rnd.next(), rnd.next(), OR_TRANSPORT_IMPORTANCE);
        if (r.flags & OR_BSDF_DELTA) {
            ++mirrorChosen;
            CL_CHECK_NEAR(r.pdfForward, 0.2, 1e-6);
            CL_CHECK_BITS(r.wi[2], wo.z);
        }
        else
            CL_CHECK(r.pdfForward > 0);
    }
    CL_CHECK_NEAR((double)mirrorChosen / n, 0.2, 3.0 * std::sqrt(0.2 * 0.8 / n));
    // `requested` removes lobes from the choice, and the probabilities renormalise over the rest
    const Subject diffuseOnly = tableSubject("diffuse requested", bsdf, OR_BSDF_REFLECTION | OR_BSDF_DIFFUSE);
    const V3 wi = normalized(V3{0.3f, 0.2f, 0.7f});
    const OrBsdfEval e = diffuseOnly.evaluate(wo, wi, OR_TRANSPORT_IMPORTANCE);
    CL_CHECK_NEAR(e.pdfForward, wi.z / kPi, floatTol(e.pdfForward, wi.z / kPi)); // p_diffuse is 1 now
    CL_CHECK(!(e.flags & OR_BSDF_HAS_DELTA));                                  // the delta lobe is not requested, so there is none to report
    const Subject mirrorOnly = tableSubject("mirror requested", bsdf, OR_BSDF_REFLECTION | OR_BSDF_SPECULAR);
    for (int i = 0; i < 100; ++i) {
        const OrBsdfSample r = mirrorOnly.sample(wo, rnd.next(), rnd.next(), rnd.next(), OR_TRANSPORT_IMPORTANCE);
        CL_CHECK(r.flags & OR_BSDF_DELTA);
        CL_CHECK_NEAR(r.pdfForward, 1.0, 1e-6);
    }
    // nothing requested: nothing answered
    const Subject none = tableSubject("nothing requested", bsdf, 0);
    CL_CHECK(none.evaluate(wo, wi, OR_TRANSPORT_IMPORTANCE).f[0] == 0.0f);
    memoryTini(arena);
}

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    const char *testName = "";
    if (which == "Reflection") {
        sectionReflection();
        testName = "Bsdf_reflection_Values";
    }
    else if (which == "Refraction") {
        sectionRefraction();
        testName = "Bsdf_refraction_Values";
    }
    else if (which == "Pair") {
        sectionPair();
        testName = "Bsdf_Delta_Pair";
    }
    else if (which == "Mixed") {
        sectionMixed();
        testName = "Bsdf_Delta_Mixed";
    }
    else {
        std::printf("usage: test_bsdf_delta <Reflection|Refraction|Pair|Mixed>\n");
        return 2;
    }
    return CL_RESULT(testName);
}
