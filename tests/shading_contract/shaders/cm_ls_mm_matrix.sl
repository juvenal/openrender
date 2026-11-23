/* GitHub #115 contract-test shader (tests/shading_contract). lightsource() into a matrix destination of parameters of other types, and of a name the light lacks: RISpec 3.2 says the destination is unchanged and the call returns 0. */
surface cm_ls_mm_matrix()
{
    matrix d = 7;
    float found = 0;
    illuminance(P, N, PI / 2) {
        found += lightsource("k", d);
        found += lightsource("s", d);
        found += lightsource("v", d);
        found += lightsource("nosuch", d);
    }
    float c0 = comp(d, 0, 0) + comp(d, 1, 1) + comp(d, 3, 3);
    Ci = color(c0, found, 0);
    Oi = 1;
}
