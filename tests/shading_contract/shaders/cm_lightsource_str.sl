/* GitHub #176 contract-test shader (tests/shading_contract). lightsource() of a string parameter inside an illuminance loop, into a parameter of its own so the test reads the copied pointer back from the slot. */
surface cm_lightsource_str(string s4 = "unset")
{
    float found = 0;
    illuminance(P, N, PI / 2) {
        found += lightsource("cmstring", s4);
    }
    Ci = color(found, 0, 0);
    Oi = 1;
}
