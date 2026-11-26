/* GitHub #234: a perfect mirror, so that a dynamic-load object is reached by a secondary ray (depth 1). */
surface dlo_uv_mirror()
{
    Ci = trace(P, reflect(normalize(I), normalize(N)));
    Oi = 1;
}
