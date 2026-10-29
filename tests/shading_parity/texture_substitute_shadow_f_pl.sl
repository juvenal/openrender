/* Spec 027 (T025): shadow(), float form with a parameter list. Interpreter (.rslo) only. */
surface tsub_shadow_f_pl()
{
    float f = shadow("shadow_a.sm", P, "bias", 0.25, "samples", 3, "blur", 0.04, "filter", "box");
    Ci = color(f, f, f);
    Oi = 1;
}
