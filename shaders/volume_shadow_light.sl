/**
 * volume_shadow_light(): a light with a ray-traced shadow that rewrites N (spec 056)
 *
 * The light sits `reach` units behind each surface point along -z (illuminate(Ps - (0, 0, reach)): L = (0, 0, reach) for
 * every point), so the ray-traced shadow() ray runs from the surface point to the light and its length is exactly
 * `reach` for every point, whatever the surface's derivatives (the sampled ray direction is jittered by the ray
 * differentials, which is negligible for a long reach). Before it asks for the shadow the light overwrites the global
 * N with its negation, as an area light writes its own normal into N (gotcha 10). The side the shadow ray leaves into
 * must therefore come from the snapshot of the surface's N that Shading takes before the lights run (research R7), not
 * from the N the light sees. The shipped shadowdistant takes a shadow map name; this one always traces.
 *
 * Used by the volume-shadow-light scene (examples/rib/tests).
 */
light volume_shadow_light(float intensity = 1; color lightcolor = 1; float reach = 3)
{
    illuminate(Ps - vector(0, 0, reach)) {
        N = -N;
        color vis = 1 - shadow("raytrace", Ps);
        Cl = vis * intensity * lightcolor;
    }
}
