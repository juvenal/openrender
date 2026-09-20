/* GitHub issue #3 (spec 017, US5) regression: phong()/specularbrdf()
 * silently no-op'd under the LLVM JIT -- same kHandledOpcodes[] coverage-
 * gate failure mode as visibility()/occlusion() etc.
 *
 * phong()'s JIT wrapper (callPhong, shading.cpp) reuses callSpecular's
 * already-shipped light-iteration shape directly -- exercised here under
 * a real bound LightSource ("distantlight"), same as
 * shaders/debug_clearlighting_probe.sl. `size` (phong's exponent
 * argument) is uniform-only, matching specular()'s own `roughness`
 * argument precedent.
 *
 * specularbrdf() is pure math (no light loop): tests both the normal
 * (L/V roughly aligned) case and a near-anti-parallel V/L case that would
 * make the halfway vector (V+L) collapse toward zero-length, exercising
 * the same NaN guard specular() already needed (project gotcha #2 --
 * `dotvv(halfway,halfway) > 0` before normalizing).
 */
surface phong_specularbrdf_probe()
{
    normal Nf = faceforward(normalize(N), I);
    vector Vf = -normalize(I);

    color phongTerm = phong(Nf, Vf, 20);

    /* Normal case: L roughly aligned with V (both pointing away from the
     * surface toward the viewer-ish direction) -- halfway is well-formed. */
    vector Lnormal = normalize(vector(0, 0, 1));
    color brdfNormal = specularbrdf(Lnormal, Nf, Vf, 0.1);

    /* Near-anti-parallel case: L pointing almost exactly opposite Vf,
     * so V+L is close to (but not exactly) the zero vector. */
    vector Lantiparallel = normalize(-Vf + vector(0.0001, 0.0001, 0.0001));
    color brdfAntiparallel = specularbrdf(Lantiparallel, Nf, Vf, 0.1);

    /* brdfAntiparallel must be finite (no NaN) -- self-compare (NaN != NaN
     * is the standard portable NaN test, and this engine has no isnan()
     * builtin exposed to RSL) folded into a float flag. Uses `if` rather
     * than `?:` -- unrelated to this probe, GitHub issue #6 found a JIT
     * bug specifically for ternary conditions built from a STRING
     * equality; using `if` here is just following the same established,
     * lower-risk habit, not a known requirement for color comparisons. */
    float antiparallelFinite = 0;
    if (brdfAntiparallel == brdfAntiparallel) antiparallelFinite = 1;

    Ci = phongTerm * 0.3 + brdfNormal * 0.3 + color(antiparallelFinite, antiparallelFinite, antiparallelFinite) * 0.2;
}
