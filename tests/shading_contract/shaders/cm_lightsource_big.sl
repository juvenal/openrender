/* Spec 026 contract-test shader (tests/shading_contract). lightsource() inside an illuminance loop, asking for a 20-float array parameter (larger than the JIT's 16-float scratch: refused, the result is 0) and for a small float parameter of the same light (answered: 7). Ci = (the array query's destination, the small query's destination, 0), Oi = the lighting. The queries' results are not read: under the JIT lightsource() writes its result into lane 0 only (GitHub #115), so the other lanes of a result would be uninitialized memory. */
surface cm_lightsource_big()
{
    color Cd = 0;
    float f = -1;
    float g = -1;
    float found = 0;
    illuminance(P, N, PI / 2) {
        lightsource("k", f);
        lightsource("small", g);
        Cd += Cl;
    }
    Ci = color(f, g, found);
    Oi = Cd;
}
