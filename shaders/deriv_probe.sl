/* GitHub issue #3 (spec 017, US5) regression: Deriv() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure
 * mode as visibility()/occlusion() etc.
 *
 * Exercises the float form (Deriv(numerator,denominator)) with a
 * genuinely varying numerator (u*u, whose d/du is meaningful across the
 * grid) and, separately, a uniform numerator (a shader-parameter-derived
 * constant) to exercise the uniform-stride guard (research.md D12/T010-
 * T012) -- a uniform operand has no spatial derivative, so its d/du and
 * d/dv must both come out exactly zero rather than reading out of
 * bounds against a stride-0 broadcast pointer.
 */
surface deriv_probe()
{
    varying float varyingNum = u * u;
    float dVaryingDu = Deriv(varyingNum, u);

    uniform float uniformNum = 3;
    float dUniformDu = Deriv(uniformNum, u);

    float uniformGuardOk = 0;
    if (dUniformDu == 0) uniformGuardOk = 1;

    Ci = color(clamp(dVaryingDu, 0, 1), uniformGuardOk, 0.5);
}
