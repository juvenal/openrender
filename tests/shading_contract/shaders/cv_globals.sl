/* Spec 056 contract-test shader (tests/shading_contract). Reads the globals a volume shader is promised
 * (contracts/segment-step.md, section 3). The colour is (length(E), time, a sum of surface-only globals that
 * the step zero-fills), so the expected value is (0, the ray's time, 0). */
volume cv_globals()
{
    Ci = color(length(E), time, length(N) + length(Ng) + u + v + s + t);
    Oi = color(length(I));
}
