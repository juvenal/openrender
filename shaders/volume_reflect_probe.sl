/**
 * volume_reflect_probe(): a surface that traces one tilted "reflection" (spec 056)
 *
 * The reflected direction is computed from a fixed shading normal tilted by tiltx about y, (tiltx, 0, -sqrt(1 - tiltx^2))
 * in current (camera) space, not from N: whatever the polygon's own normal is, the ray of a camera ray along +z goes to
 * (2*tiltx*c, 0, 1 - 2*c*c) with c = sqrt(1 - tiltx^2), i.e. (0.866, 0, -0.5) for the default 0.5. A scene lays its
 * surfaces parallel to the camera plane, so every reflected segment to a parallel plane (or to the world bound's
 * parallel face) has the same length for every pixel. The side the ray leaves into is decided by the geometric N
 * (dot(R, N) > 0 is the exterior), which is the point of the probe.
 *
 * via = "trace" calls trace(P, R); via = "environment" calls environment("raytrace", R), the same entry point.
 *
 * Used by volume-exterior-fog-reflection, volume-primitive-hitmode, volume-environment-raytrace,
 * volume-miss-clipping, volume-random-fog, volume-csg-fragment and atmosphere-fog-reflection-raytrace.
 */
surface volume_reflect_probe(float tiltx = 0.5; string via = "trace")
{
    vector In = normalize(I);
    normal Nt = normal(tiltx, 0, -sqrt(1 - tiltx * tiltx));
    vector R = reflect(In, Nt);
    if (via == "environment") {
        Ci = environment("raytrace", R);
    }
    else {
        Ci = trace(P, R);
    }
    Oi = 1;
}
