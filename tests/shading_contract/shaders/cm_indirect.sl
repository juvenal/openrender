/* Spec 026 contract-test shader (tests/shading_contract). indirectdiffuse(). */
surface cm_indirect()
{
    Ci = indirectdiffuse(P, normalize(N), 8);
    Oi = 1;
}
