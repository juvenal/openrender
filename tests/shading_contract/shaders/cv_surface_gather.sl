/* Spec 056 contract-test shader (tests/shading_contract). gather() of one ray along +z, -z or +x by s (as cv_surface_trace; a cone of angle 0, so
 * the direction is exact), with a maxdist of 5. The block and the else branch both add the surface:Ci output;
 * the Oi encodes ray:direction's z and the number of misses (the step must change neither). */
surface cv_surface_gather()
{
    vector dir = (s < 0.15) ? vector(0, 0, 1) : ((s < 0.25) ? vector(0, 0, -1) : vector(1, 0, 0));
    color c = 0;
    color tot = 0;
    float nmiss = 0;
    vector rd = 0;
    gather("illuminance", P, dir, 0, 1, "bias", 0.02, "maxdist", 5, "surface:Ci", c, "ray:direction", rd) {
        tot += c;
    }
    else {
        nmiss += 1;
        tot += c;
    }
    Ci = tot;
    Oi = color(zcomp(rd), nmiss, 1);
}
