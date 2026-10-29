/* Spec 027 (T025): texture(), float form (channel 1), no parameter list. */
surface tsub_tex_f()
{
    float f = texture("tex_a.tex"[1], s, t);
    Ci = color(f, f, f);
    Oi = 1;
}
