/* Spec 027 (T025): shadow(), float form, no parameter list. The point is P. */
surface tsub_shadow_f()
{
    float f = shadow("shadow_a.sm", P);
    Ci = color(f, f, f);
    Oi = 1;
}
