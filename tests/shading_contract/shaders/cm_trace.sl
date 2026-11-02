/* Spec 026 contract-test shader (tests/shading_contract). trace(): a colour form (a bundle) and a float form (a probe). */
surface cm_trace()
{
    vector R = reflect(normalize(I), normalize(N));
    color c = trace(P, R);
    float d = trace(P, R);
    Ci = c + color(d * 0.001);
    Oi = 1;
}
