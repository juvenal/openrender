/*
 * Spec 035, Bsdf_diffuse_* (label closures): the `diffuse` component (Lambert) in its local frame, whose z axis is the component's N.
 *
 *   Bsdf_diffuse_Values        f == 1/pi with no cosine, density cos(theta)/pi of the direction it describes, flags "diffuse reflection,
 *                              not delta", the horizon (US2 AS1)
 *   Bsdf_diffuse_{Reciprocity,Energy,PdfNorm,SamplePdf,ChiSquare,ReversePdf}   the six checks of bsdf_checks.h; Energy includes the white
 *                              furnace (a diffuse lobe of albedo 1 is lossless)
 *
 * Written red first: bsdfMath.h and the components' operations do not exist yet.
 */
#include <string>

#include "bsdf_checks.h"
#include "closureBuiltins.h"

using namespace clbsdf;
using namespace openrender::shading;

static Subject diffuseSubject(ClosureDiffuseBlock &b) {
    Subject s = componentSubject("diffuse", kClosureDiffuse, &b);
    s.reflectionOnly = true;
    s.lossless = true;
    return s;
}

static void sectionValues() {
    ClosureDiffuseBlock block = {{0, 0, 1}};
    const Subject s = diffuseSubject(block);
    cltest::KeyedRandom rnd(0xD1FF0001ull);
    for (int i = 0; i < 2000; ++i) {
        const V3 wo = uniformUpperHemisphere(rnd.next(), rnd.next()), wi = uniformUpperHemisphere(rnd.next(), rnd.next());
        if (wo.z < 1e-3f || wi.z < 1e-3f)
            continue;
        for (OrTransportMode m : {OR_TRANSPORT_RADIANCE, OR_TRANSPORT_IMPORTANCE}) {
            const OrBsdfEval e = s.evaluate(wo, wi, m);
            const double invPi = 1.0 / kPi;
            CL_CHECK_NEAR(e.f[0], invPi, floatTol(invPi, invPi)); // f = albedo / pi: no cosine in f
            CL_CHECK_BITS(e.f[1], e.f[0]);
            CL_CHECK_BITS(e.f[2], e.f[0]);
            CL_CHECK_INT(e.numChannels, 3);
            CL_CHECK_NEAR(e.pdfForward, wi.z * invPi, floatTol(e.pdfForward, wi.z * invPi)); // the density of wi: cos(theta_i) / pi
            CL_CHECK_NEAR(e.pdfReverse, wo.z * invPi, floatTol(e.pdfReverse, wo.z * invPi)); // and of wo for the swapped query
            CL_CHECK((e.flags & OR_BSDF_REFLECTION) && (e.flags & OR_BSDF_DIFFUSE));
            CL_CHECK(!(e.flags & (OR_BSDF_TRANSMISSION | OR_BSDF_DELTA | OR_BSDF_HAS_DELTA | OR_BSDF_FAILED | OR_BSDF_NOT_SCATTERER)));
        }
        const OrBsdfSample r = s.sample(wo, rnd.next(), rnd.next(), 0.5f, OR_TRANSPORT_RADIANCE);
        CL_CHECK(!(r.flags & (OR_BSDF_FAILED | OR_BSDF_DELTA)));
        CL_CHECK(r.wi[2] > 0.0f);
        CL_CHECK_NEAR(r.pdfForward, r.wi[2] / kPi, floatTol(r.pdfForward, r.wi[2] / kPi));
        CL_CHECK_NEAR(r.pdfReverse, wo.z / kPi, floatTol(r.pdfReverse, wo.z / kPi));
        CL_CHECK((r.flags & OR_BSDF_REFLECTION) && (r.flags & OR_BSDF_DIFFUSE));
    }
    // the horizon: wo or wi on or below the plane gives nothing; a sample from below fails
    const V3 up{0, 0, 1}, flat{1, 0, 0}, down{0.6f, 0.0f, -0.8f};
    for (const V3 &bad : {flat, down}) {
        CL_CHECK(s.evaluate(bad, up, OR_TRANSPORT_RADIANCE).f[0] == 0.0f);
        CL_CHECK(s.evaluate(up, bad, OR_TRANSPORT_RADIANCE).f[0] == 0.0f);
        CL_CHECK(s.pdf(up, bad, OR_TRANSPORT_RADIANCE).pdfForward == 0.0f);
        const OrBsdfSample r = s.sample(bad, 0.3f, 0.6f, 0.5f, OR_TRANSPORT_RADIANCE);
        CL_CHECK(r.flags & OR_BSDF_FAILED);
        CL_CHECK(r.f[0] == 0.0f && r.pdfForward == 0.0f && r.pdfReverse == 0.0f);
    }
    // a tilted component normal is the caller's frame, not the component's: the component's N is read by the table (Bsdf_Conventions)
}

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    ClosureDiffuseBlock block = {{0, 0, 1}};
    if (which == "Values")
        sectionValues();
    else {
        Options o;
        if (!runCheckSection(which, diffuseSubject(block), o, 0xD1FF0002ull)) {
            std::printf("usage: test_bsdf_diffuse <Values|Reciprocity|Energy|PdfNorm|SamplePdf|ChiSquare|ReversePdf>\n");
            return 2;
        }
    }
    return CL_RESULT(("Bsdf_diffuse_" + which).c_str());
}
