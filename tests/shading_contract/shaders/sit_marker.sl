/* Spec 026 Situations shader (tests/shading_contract). Prints a marker line with the ray depth every time it runs: the branch shade() took is whether it appears. */
surface sit_marker()
{
    printf("SITMARK %f\n", raydepth());
    Ci = color(0, 0, 1);
    Oi = 1;
}
