/* Spec 026 contract-test shader (tests/shading_contract). environment("raytrace"). */
surface cm_env()
{
    vector R = reflect(normalize(I), normalize(N));
    Ci = environment("raytrace", R);
    Oi = 1;
}
