/* GitHub #209: attribute() into a varying local initialised from a constant. The source is uniform, so this cannot fail before
 * the fix; it guards the exemption. Ci = (v, found, 0) = (1, 1, 0): the default ShadingRate is 1. */
surface msgout_scene_surface()
{
    float v = -1;
    float found = attribute("ShadingRate", v);
    Ci = color(v, found, 0);
    Oi = 1;
}
