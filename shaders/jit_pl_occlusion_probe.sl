/* GitHub #55: occlusion() and indirectdiffuse() with optional parameter
 * lists, for the ShadingParity_JitPL* tests (tests/visual/CMakeLists.txt).
 * The same scene renders under the interpreter (.rslo) and the LLVM JIT
 * (.slo), and the two must be bit-identical. As in jit_pl_trace_probe.sl,
 * every value comes from a shader parameter, so a test changes a value
 * without moving a call site.
 *
 *   red    occlusion(P, Nf, 16, ...)
 *   green  indirectdiffuse(P, Nf, 16, ...)
 *   blue   the "environmentdir" channel occlusion() binds (an output, not a
 *          parameter: COcclusionLookup::bind treats any other varying name as
 *          a channel of the irradiance cache)
 *
 * Each call keeps its own in-memory cache (its own "handle", "filemode" "")
 * so the two never share samples. No call sits inside an `if`.
 */
surface jit_pl_occlusion_probe(uniform float maxdist = 1e30;
                               uniform float bias = 0.01;
                               uniform float samplebase = 1;
                               uniform float maxerror = 0.6;
                               uniform string coordsys = "world";
                               uniform color envcolor = 0;
                               uniform float maxbrightness = 1;
                               uniform string ohandle = "pl55occlusion";
                               uniform string ihandle = "pl55indirect")
{
    normal Nn = normalize(N);
    vector Nf = faceforward(Nn, I);
    vector ed = 0;

    float occ = occlusion(P, Nf, 16, "maxdist", maxdist, "bias", bias, "samplebase", samplebase,
                          "maxerror", maxerror, "coordsystem", coordsys, "handle", ohandle,
                          "filemode", "", "environmentdir", ed);
    color id = indirectdiffuse(P, Nf, 16, "maxdist", maxdist, "bias", bias, "samplebase", samplebase,
                               "maxerror", maxerror, "coordsystem", coordsys, "environmentcolor", envcolor,
                               "maxBrightness", maxbrightness, "handle", ihandle, "filemode", "");

    float g = (comp(id, 0) + comp(id, 1) + comp(id, 2)) / 3;
    Ci = color(occ, g, 0.5 + 0.5 * ycomp(normalize(ed + vector(0, 0, 1e-6))));
    Oi = 1;
}
