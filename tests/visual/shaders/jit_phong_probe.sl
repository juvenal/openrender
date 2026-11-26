/* GitHub #113: phong() alone, for the ShadingParity_JitPhong_* tests
 * (tests/visual/CMakeLists.txt). The same scene renders under the interpreter
 * (.rslo) and the LLVM JIT (.slo), and the two must be bit-identical.
 *
 *   size      the exponent passed to phong()
 *   sizevary  0: the same size at every point; otherwise the size grows with s,
 *             so it differs from point to point
 */
surface jit_phong_probe(float size = 2; float sizevary = 0)
{
    normal Nf = faceforward(normalize(N), I);
    vector V = -normalize(I);
    float sz = size * (1 + sizevary * s);
    Ci = phong(Nf, V, sz);
    Oi = 1;
}
