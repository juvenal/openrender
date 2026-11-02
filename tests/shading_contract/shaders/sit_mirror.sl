/* Spec 026 Situations shader (tests/shading_contract). A mirror: its trace() makes the secondary hits of row 4. */
surface sit_mirror()
{
    vector R = reflect(normalize(I), normalize(N));
    Ci = trace(P, R);
    Oi = 1;
}
