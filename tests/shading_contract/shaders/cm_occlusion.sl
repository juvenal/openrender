/* Spec 026 contract-test shader (tests/shading_contract). occlusion(). */
surface cm_occlusion()
{
    float o = occlusion(P, normalize(N), 8);
    Ci = color(o, o, o);
    Oi = 1;
}
