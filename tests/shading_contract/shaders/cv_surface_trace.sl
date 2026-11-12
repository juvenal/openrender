/* Spec 056 contract-test shader (tests/shading_contract). trace() of one ray per point, by the point's s: the first point
 * along +z (dot(dir, N) = 1 for the harness' N = (0,0,1): the exterior side), the second along -z (the interior side), the
 * third along +x (dot(dir, N) = 0, the tie, which reads interior). */
surface cv_surface_trace()
{
    vector dir = (s < 0.15) ? vector(0, 0, 1) : ((s < 0.25) ? vector(0, 0, -1) : vector(1, 0, 0));
    Ci = trace(P, dir);
    Oi = 1;
}
