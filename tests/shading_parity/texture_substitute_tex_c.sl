/* Spec 027 (T025): texture(), color form, two coordinates, no parameter list. Compiled for both
 * backends by test_texture_substitute.cpp (the surface name is renamed per backend). */
surface tsub_tex_c()
{
    Ci = texture("tex_a.tex", s, t);
    Oi = 1;
}
