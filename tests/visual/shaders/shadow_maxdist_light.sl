/* GitHub #236: a traced shadow() inside a light's illuminate block, where L is defined, with a "maxdist" parameter. It carries the
 * control that ShadingParity_JitPLEngaged_envshadow_raytrace_maxdist_* used to give through a surface outside illuminance (which now
 * answers 0, shadowLightState()): with an occluder in the ray's way, a short maxdist must give a different Cl from the default one.
 * Cl is 1 - shadow(), so the receiver reads 0 where the ray meets the occluder and 1 where maxdist cuts it short. */
light shadow_maxdist_light(point from = point (0, 0, 3); uniform float maxdist = 1e30)
{
    illuminate(from) {
        float s = shadow("raytrace", Ps, "maxdist", maxdist);
        Cl = color(1 - s, 1 - s, 1 - s);
    }
}
