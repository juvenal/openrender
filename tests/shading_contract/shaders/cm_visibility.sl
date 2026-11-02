/* Spec 026 contract-test shader (tests/shading_contract). visibility() (a probe). */
surface cm_visibility()
{
    point q = P + vector(0.3, 0.4, -0.5);
    float v = visibility(P, q);
    Ci = color(v, v, v);
    Oi = 1;
}
