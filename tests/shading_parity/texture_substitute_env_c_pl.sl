/* Spec 027 (T025): environment(), color form with a parameter list. Interpreter (.rslo) only. */
surface tsub_env_c_pl()
{
    Ci = environment("env_a.env", N, "blur", 0.03, "fill", 0.6, "samples", 5, "filter", "box");
    Oi = 1;
}
