/* Spec 027 (T025): texture(), color form with a parameter list. Interpreter (.rslo) only: the JIT
 * ignores the list until Phase 9. */
surface tsub_tex_c_pl()
{
    Ci = texture("tex_a.tex", s, t, "blur", 0.02, "fill", 0.5, "samples", 4, "width", 2, "filter", "box");
    Oi = 1;
}
