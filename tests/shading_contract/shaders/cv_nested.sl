/* Spec 056 contract-test shader (tests/shading_contract). A volume shader that calls trace() once and writes the
 * returned colour into Ci: the ray it spawns gets no volume step (FR-009) and counts toward the ray depth. */
volume cv_nested()
{
    Ci = trace(P, vector(0, 0, 1));
    Oi = 1;
}
