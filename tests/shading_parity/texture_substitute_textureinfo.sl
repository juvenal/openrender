/* Spec 027 (T025): textureinfo(), "resolution" of a 2D map. The result is the found flag in the red
 * channel and the two resolution values in green and blue (zero if not found). */
surface tsub_textureinfo()
{
    float res[2] = {-1, -1};
    float found = textureinfo("tex_a.tex", "resolution", res);
    Ci = color(found, res[0], res[1]);
    Oi = 1;
}
