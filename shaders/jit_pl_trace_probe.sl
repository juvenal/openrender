/* GitHub #55: trace(), transmission() and gather() with optional parameter
 * lists, for the ShadingParity_JitPL* tests (tests/visual/CMakeLists.txt).
 * The same scene renders under the interpreter (.rslo) and the LLVM JIT
 * (.slo), and the two must be bit-identical. Every value of every parameter
 * list comes from a shader parameter set in the RIB, so a test changes a
 * value without moving a call site (each call site draws its own sampler
 * sub-stream): the ShadingParity_JitPLEngaged_* tests compare two values at
 * the same call site to show that the parameter takes effect.
 *
 *   red    trace(P, R, ...)              reflection rays, full parameter list
 *   green  transmission(P, q, ...)       the same list
 *   blue   gather(...) with its own list, averaged with a plain trace(P, R)
 *          made after the gather (its label must be the default "", not the
 *          one gather() left behind)
 *
 * The floor and the occluder run jit_pl_label_probe.sl, which shades a hit by
 * the label of the ray that found it. No call sits inside an `if`.
 *
 * "sampleBase" is spelled the way CTraceLookup::bind (shaderPl.cpp) spells it;
 * the interpreter rejects "samplebase" for trace()/transmission().
 */
surface jit_pl_trace_probe(uniform float samples = 1;
                           uniform float samplecone = 0;
                           uniform float bias = 0.01;
                           uniform float sampleBase = 1;
                           uniform float maxdist = 1e30;
                           uniform string label = "";
                           uniform float gmaxdist = 1e30;
                           uniform float gbias = 0.01;
                           uniform float gsamplebase = 1;
                           uniform string gdistribution = "cosine";
                           uniform string glabel = "gather")
{
    normal Nn = normalize(N);
    vector Nf = faceforward(Nn, I);
    vector R = reflect(normalize(I), Nf);
    point q = P + vector(1.5, -1.6, 0.2);

    color tc = trace(P, R, "samples", samples, "samplecone", samplecone, "bias", bias,
                     "sampleBase", sampleBase, "maxdist", maxdist, "label", label);
    color tr = transmission(P, q, "samples", samples, "samplecone", samplecone, "bias", bias,
                            "sampleBase", sampleBase, "maxdist", maxdist, "label", label);

    color Cs = 0;
    color Ct = 0;
    gather("illuminance", P, Nf, PI * 0.4, 4, "maxdist", gmaxdist, "bias", gbias,
           "samplebase", gsamplebase, "distribution", gdistribution, "label", glabel,
           "surface:Ci", Cs) {
        Ct += Cs;
    }
    color pt = trace(P, R);

    float r = (comp(tc, 0) + comp(tc, 1) + comp(tc, 2)) / 3;
    float g = (comp(tr, 0) + comp(tr, 1) + comp(tr, 2)) / 3;
    float b = (comp(Ct, 0) + comp(Ct, 1) + comp(Ct, 2)) / 24 + (comp(pt, 0) + comp(pt, 1) + comp(pt, 2)) / 6;
    Ci = color(r, g, b);
    Oi = 1;
}
