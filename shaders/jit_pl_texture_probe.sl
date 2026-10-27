/* Spec 027 (T030, FR-022): texture() with optional parameter lists, for the
 * ShadingParity_JitPL_texture_* tests (tests/visual/CMakeLists.txt). The same scene
 * renders under the interpreter (.rslo) and the LLVM JIT (.slo), and the two must be
 * bit-identical once the JIT honours the lists (Phase 9). As in jit_pl_trace_probe.sl,
 * every value comes from a shader parameter, so a test changes a value without moving a
 * call site.
 *
 *   red    texture() color form, two coordinates (the mean of its three channels)
 *   green  texture() float form, channel 1
 *   blue   texture() color form, the eight-coordinate form (the mean of its channels)
 *
 * No call sits inside an `if`.
 */
surface jit_pl_texture_probe(uniform string map = "texture-png-source.tex";
                             uniform float blur = 0;
                             uniform float width = 1;
                             uniform float swidth = 1;
                             uniform float twidth = 1;
                             uniform float fill = 0;
                             uniform float samples = 1;
                             uniform string filter = "box")
{
    color c2 = texture(map, s, t, "blur", blur, "width", width, "swidth", swidth, "twidth", twidth,
                       "fill", fill, "samples", samples, "filter", filter);
    float f1 = texture(map[1], s, t, "blur", blur, "width", width, "swidth", swidth, "twidth", twidth,
                       "fill", fill, "samples", samples, "filter", filter);
    color c8 = texture(map, s, t, s + 0.15, t, s, t + 0.15, s + 0.15, t + 0.15, "blur", blur, "width", width,
                       "swidth", swidth, "twidth", twidth, "fill", fill, "samples", samples, "filter", filter);

    Ci = color((comp(c2, 0) + comp(c2, 1) + comp(c2, 2)) / 3, f1, (comp(c8, 0) + comp(c8, 1) + comp(c8, 2)) / 3);
    Oi = 1;
}
