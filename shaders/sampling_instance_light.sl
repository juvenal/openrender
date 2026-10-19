/* Spec 039, Phase 6 follow-up (Sampling_InstanceIndependence_*): a light whose Cl is a draw.
 * Cl = (random(), id, 0): the red channel is the light's draw at this point, the green channel
 * names the instance (the scene gives each instance its own `id`), so a surface can tell the
 * lights apart. Two instances of this shader at one point must draw from streams of their own:
 * adding, removing or reordering another instance must not change what an instance draws.
 */
light sampling_instance_light(float id = 1)
{
    illuminate(point(0, 0, -4)) {
        Cl = color(random(), id, 0);
    }
}
