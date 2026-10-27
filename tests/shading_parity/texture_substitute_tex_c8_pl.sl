/* Spec 027 (T025): texture(), color form, the eight-coordinate form with a parameter list.
 * Interpreter (.rslo) only. */
surface tsub_tex_c8_pl()
{
    Ci = texture("tex_a.tex", s, t, s + 0.1, t, s, t + 0.2, s + 0.1, t + 0.2, "samples", 3, "filter", "box");
    Oi = 1;
}
