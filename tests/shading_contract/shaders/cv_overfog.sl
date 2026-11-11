/* Spec 056 contract-test shader (tests/shading_contract). An "over"-form volume like fog (research R4):
 * Ci = Ci*T + background*d, Oi = Oi*T + d, with T = exp(-length(I)/distance) and d = 1 - T. The compositing
 * identity of the contract case is exact for this form. */
volume cv_overfog(float distance = 2; color background = color(0.1, 0.3, 0.9))
{
    float T = exp(-length(I) / distance);
    float d = 1 - T;
    Ci = Ci * T + background * d;
    Oi = Oi * T + color(d);
}
