/* GitHub #111: specular() alone, for the ShadingParity_JitSpecular_* tests
 * (tests/visual/CMakeLists.txt). The same scene renders under the interpreter
 * (.rslo) and the LLVM JIT (.slo), and the two must be bit-identical.
 *
 *   roughness      the roughness passed to specular()
 *   roughnessvary  0: the same roughness at every point; otherwise the
 *                  roughness grows with s, so it differs from point to point
 */
surface jit_specular_probe(float roughness = 0.1; float roughnessvary = 0)
{
    normal Nf = faceforward(normalize(N), I);
    vector V = -normalize(I);
    float r = roughness * (1 + roughnessvary * s);
    Ci = specular(Nf, V, r);
    Oi = 1;
}
