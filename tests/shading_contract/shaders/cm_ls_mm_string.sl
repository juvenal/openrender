/* GitHub #115 contract-test shader (tests/shading_contract). lightsource() into a string destination of parameters of other types, and of a name the light lacks: RISpec 3.2 says the destination is unchanged and the call returns 0. */
surface cm_ls_mm_string()
{
    string d = "unset";
    float found = 0;
    illuminance(P, N, PI / 2) {
        found += lightsource("k", d);
        found += lightsource("v", d);
        found += lightsource("m", d);
        found += lightsource("nosuch", d);
    }
    float c0 = 0;
    if (d == "unset")
        c0 = 1;
    Ci = color(c0, found, 0);
    Oi = 1;
}
