/* Spec 027 (T030, FR-022): environment() and shadow() with optional parameter lists, for the
 * ShadingParity_JitPL_envshadow_* tests (tests/visual/CMakeLists.txt). As in
 * jit_pl_texture_probe.sl every value comes from a shader parameter. The map names are
 * parameters too, so a case can pass "raytrace" (the traced forms, clarification Q7).
 *
 *   red    environment() color form (the mean of its three channels)
 *   green  environment() float form, channel 0
 *   blue   shadow() float form
 *
 * No call sits inside an `if`.
 */
surface jit_pl_envshadow_probe(uniform string envmap = "env-source.tex";
                               uniform string shadowmap = "shadow-source.tex";
                               uniform float blur = 0;
                               uniform float width = 1;
                               uniform float fill = 0;
                               uniform float samples = 1;
                               uniform float bias = 0.01;
                               uniform float samplecone = 0;
                               uniform float maxdist = 1e30;
                               uniform string filter = "box")
{
    normal Nn = normalize(N);
    vector Nf = faceforward(Nn, I);
    vector R = reflect(normalize(I), Nf);

    color e = environment(envmap, R, "blur", blur, "width", width, "fill", fill, "samples", samples,
                          "bias", bias, "samplecone", samplecone, "maxdist", maxdist, "filter", filter);
    float ef = environment(envmap[0], R, "blur", blur, "width", width, "fill", fill, "samples", samples,
                           "bias", bias, "samplecone", samplecone, "maxdist", maxdist, "filter", filter);
    float sh = shadow(shadowmap, P, "blur", blur, "width", width, "fill", fill, "samples", samples,
                      "bias", bias, "samplecone", samplecone, "maxdist", maxdist, "filter", filter);

    Ci = color((comp(e, 0) + comp(e, 1) + comp(e, 2)) / 3, ef, sh);
    Oi = 1;
}
