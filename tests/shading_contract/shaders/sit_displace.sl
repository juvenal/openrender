/* Spec 026 Situations shader (tests/shading_contract). A displacement shader: the displacement-only passes and the bound estimate of rows 8 and 9. */
displacement sit_displace()
{
    printf("SITDISP\n");
    P = P + normalize(N) * 0.02;
    N = calculatenormal(P);
}
