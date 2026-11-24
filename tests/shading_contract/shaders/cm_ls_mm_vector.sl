/* GitHub #115 contract-test shader (tests/shading_contract). lightsource() into a vector destination of parameters of other types, and of a name the light lacks: RISpec 3.2 says the destination is unchanged and the call returns 0. */
surface cm_ls_mm_vector()
{
    vector d = vector(-1, -2, -3);
    float found = 0;
    illuminance(P, N, PI / 2) {
        found += lightsource("k", d);
        found += lightsource("s", d);
        found += lightsource("m", d);
        found += lightsource("nosuch", d);
    }
    float c0 = xcomp(d) + ycomp(d) + zcomp(d);
    Ci = color(c0, found, 0);
    Oi = 1;
}
