/* Spec 026 contract-test shader (tests/shading_contract). lightsource() inside an illuminance loop, asking for a 20-float array parameter (larger than the JIT's 16-float scratch: refused, the result is 0) and for a small float parameter of the same light (answered: 7). Ci = (the array query's destination, the small query's destination, the sum of the array query's results), Oi = the lighting. */
surface cm_lightsource_big()
{
    color Cd = 0;
    float f = -1;
    float g = -1;
    float found = 0;
    illuminance(P, N, PI / 2) {
        found += lightsource("k", f);
        lightsource("small", g);
        Cd += Cl;
    }
    Ci = color(f, g, found);
    Oi = Cd;
}
