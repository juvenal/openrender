/* Spec 027 (T025): environment(), float form (channel 0), no parameter list. */
surface tsub_env_f()
{
    float f = environment("env_a.env"[0], N);
    Ci = color(f, f, f);
    Oi = 1;
}
