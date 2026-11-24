/* GitHub #115 contract-test shader (tests/shading_contract). lightsource() into a float destination of parameters of other types, and of a name the light lacks: RISpec 3.2 says the destination is unchanged and the call returns 0. */
surface cm_ls_mm_float()
{
    float d = -1;
    float found = 0;
    illuminance(P, N, PI / 2) {
        found += lightsource("s", d);
        found += lightsource("v", d);
        found += lightsource("m", d);
        found += lightsource("nosuch", d);
    }
    float c0 = d;
    Ci = color(c0, found, 0);
    Oi = 1;
}
