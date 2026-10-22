/* Spec 039, Phase 6 (US1/US6): properties of random() under the sampler.
 * Run under the interpreter (.rslo) and the LLVM JIT (.slo) by the
 * SamplingJitParity_RandomProbe_* visual tests, which read the output pixel
 * back exactly. Every flag is 0 or 1:
 *   R  = 1 if the derivative of a varying random() is not zero (want 0). The
 *        two derivative-offset vertices of a grid mirror their real vertex's
 *        draw (research R3.10a), so Du() and Dv() of a draw are exactly 0;
 *   G  = 1 if four varying draws -- two call sites and the two iterations of
 *        one loop -- are pairwise distinct (FR-019) (want 1);
 *   B  = 1 if two urandom() draws are distinct and every draw lies in [0, 1)
 *        (want 1).
 * random() and urandom() cannot be assigned to a uniform variable (the
 * compiler rejects it), so there is no uniform form to probe from source.
 * No `if`-gated uniform assignment (GitHub #8) and no optional parameters
 * (GitHub #55): every flag is arithmetic on step().
 */
float differ(float x; float y)
{
    return step(1e-7, abs(x - y));
}

float inRange(float x)
{
    return step(0, x) * (1 - step(1, x));
}

surface sampling_random_probe()
{
    float a = random();
    float b = random();
    float l0 = 0;
    float l1 = 0;
    uniform float i;
    for (i = 0; i < 2; i += 1) {
        float d = random();
        l0 = l1;
        l1 = d;
    }
    float u0 = urandom();
    float u1 = urandom();

    float derivative = step(1e-9, abs(Du(a)) + abs(Dv(a)) + abs(Du(l1)) + abs(Dv(l1)));
    float distinct = differ(a, b) * differ(a, l0) * differ(a, l1) *
                     differ(b, l0) * differ(b, l1) * differ(l0, l1);
    float urandomOk = differ(u0, u1) * inRange(u0) * inRange(u1);
    float rangeOk = inRange(a) * inRange(b) * inRange(l0) * inRange(l1);

    Ci = color(derivative, distinct, urandomOk * rangeOk);
    Oi = 1;
}
