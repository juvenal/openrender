/*
 * Spec 035, Phase 3 (T054): the probe surface of the delegating cldbl harness. A constant colour from Cs and the surface's own opacity,
 * no lights, no texture: the image of a frame of it is Cs wherever the object covers the pixel.
 */
surface cl_probe_surface()
{
    Ci = Cs * Os;
    Oi = Os;
}
