/* Spec 056 contract-test shader (tests/shading_contract). transmission() toward a point 2 units along the direction
 * (by the point's s: +z, -z, +x as cv_surface_trace). */
surface cv_surface_transmission()
{
    vector dir = (s < 0.15) ? vector(0, 0, 1) : ((s < 0.25) ? vector(0, 0, -1) : vector(1, 0, 0));
    point q = P + 2 * dir;
    Ci = transmission(P, q);
    Oi = 1;
}
