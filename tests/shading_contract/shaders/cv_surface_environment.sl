/* Spec 056 contract-test shader (tests/shading_contract). environment("raytrace") toward +z, -z or +x (by s, as cv_surface_trace). */
surface cv_surface_environment()
{
    vector dir = (s < 0.15) ? vector(0, 0, 1) : ((s < 0.25) ? vector(0, 0, -1) : vector(1, 0, 0));
    Ci = environment("raytrace", dir);
    Oi = 1;
}
