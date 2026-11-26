/*
 * Spec 035, Bsdf_oren_nayar_* (label closures): the `oren_nayar` component, the qualitative model of Oren and Nayar 1994 as OSL's
 * `oren_nayar` closure uses it (f = (A + B max(0, cos(phi_i - phi_o)) sin(alpha) tan(beta)) / pi, A = 1 - sigma^2 / (2 (sigma^2 + 0.33)),
 * B = 0.45 sigma^2 / (sigma^2 + 0.09), alpha the larger and beta the smaller of the two polar angles), sampled with a cosine density.
 *
 *   Bsdf_oren_nayar_Sigma0      sigma 0 equals `diffuse` bit for bit: f, both densities, the flags and the sampled direction
 *   Bsdf_oren_nayar_{Reciprocity,Energy,PdfNorm,SamplePdf,ChiSquare,ReversePdf}   the six checks, for sigma in {0.3, 0.7, 1.2}
 *
 * Energy: the model is not energy conserving by construction, so it has no furnace test (not lossless): its sampled albedo must be at
 * most 1 + 3 sigma and must agree with the uniform-sphere integral.
 *
 * Written red first: bsdfMath.h and the components' operations do not exist yet.
 */
#include <string>

#include "bsdf_checks.h"
#include "closureBuiltins.h"

using namespace clbsdf;
using namespace openrender::shading;

static void sectionSigma0() {
    ClosureOrenNayarBlock on = {{0, 0, 1}, 0.0f};
    ClosureDiffuseBlock d = {{0, 0, 1}};
    const Subject a = componentSubject("oren_nayar sigma 0", kClosureOrenNayar, &on), b = componentSubject("diffuse", kClosureDiffuse, &d);
    cltest::KeyedRandom rnd(0x0E0A0001ull);
    for (int i = 0; i < 5000; ++i) {
        const V3 wo = uniformUpperHemisphere(rnd.next(), rnd.next()), wi = uniformUpperHemisphere(rnd.next(), rnd.next());
        for (OrTransportMode m : {OR_TRANSPORT_RADIANCE, OR_TRANSPORT_IMPORTANCE}) {
            const OrBsdfEval x = a.evaluate(wo, wi, m), y = b.evaluate(wo, wi, m);
            CL_CHECK_BITS(x.f[0], y.f[0]);
            CL_CHECK_BITS(x.f[1], y.f[1]);
            CL_CHECK_BITS(x.f[2], y.f[2]);
            CL_CHECK_BITS(x.pdfForward, y.pdfForward);
            CL_CHECK_BITS(x.pdfReverse, y.pdfReverse);
            CL_CHECK_INT(x.flags, y.flags);
        }
        const float u1 = rnd.next(), u2 = rnd.next();
        const OrBsdfSample r = a.sample(wo, u1, u2, 0.5f, OR_TRANSPORT_RADIANCE), q = b.sample(wo, u1, u2, 0.5f, OR_TRANSPORT_RADIANCE);
        for (int k = 0; k < 3; ++k)
            CL_CHECK_BITS(r.wi[k], q.wi[k]);
        CL_CHECK_BITS(r.f[0], q.f[0]);
        CL_CHECK_BITS(r.pdfForward, q.pdfForward);
        CL_CHECK_INT(r.flags, q.flags);
    }
}

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "Sigma0") {
        sectionSigma0();
        return CL_RESULT("Bsdf_oren_nayar_Sigma0");
    }
    Options o;
    bool known = true;
    for (float sigma : {0.3f, 0.7f, 1.2f}) {
        ClosureOrenNayarBlock block = {{0, 0, 1}, sigma};
        Subject s = componentSubject("oren_nayar", kClosureOrenNayar, &block);
        s.name += " sigma " + std::to_string(sigma);
        s.reflectionOnly = true;
        s.lossless = false;
        known = runCheckSection(which, s, o, 0x0E0A0002ull ^ (uint64_t)(sigma * 100)) && known;
    }
    if (!known) {
        std::printf("usage: test_bsdf_oren_nayar <Sigma0|Reciprocity|Energy|PdfNorm|SamplePdf|ChiSquare|ReversePdf>\n");
        return 2;
    }
    return CL_RESULT(("Bsdf_oren_nayar_" + which).c_str());
}
