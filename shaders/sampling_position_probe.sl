/* Spec 039, T124 (Sampling_CropConsistent_raytrace_position): Ci is the shaded point P in
 * camera space, so the image records each sample's position to the last bit. A crop render must
 * reproduce it exactly: the ray of a pixel must be generated from the same raster position
 * whichever window renders it.
 */
surface sampling_position_probe()
{
    Oi = 1;
    Ci = color(xcomp(P), ycomp(P), zcomp(P));
}
