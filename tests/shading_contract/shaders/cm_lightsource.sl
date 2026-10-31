/* Spec 026 contract-test shader (tests/shading_contract). lightsource() inside an illuminance loop: a float and a vector (non-float) parameter. */
surface cm_lightsource()
{
    color Cd = 0;
    float f = -1;
    float found = 0;
    vector v = vector(-1, -2, -3);
    illuminance(P, N, PI / 2) {
        found += lightsource("cmfloat", f);
        found += 2 * lightsource("cmvec", v);
        Cd += Cl;
    }
    Ci = color(f, xcomp(v), found);
    Oi = Cd;
}
