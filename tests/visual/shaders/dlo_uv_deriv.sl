/* GitHub #234: a dynamic-load object's ray hit has defined parametric derivatives, all 0. Ci is (|dPdu| + |dPdv|,
 * |Du(P)| + |Dv(P)|, area(P)); the first two must be exactly 0, the third is area()'s floor (1e-6, never 0, never NaN). */
surface dlo_uv_deriv()
{
    Ci = color(length(dPdu) + length(dPdv), length(Du(P)) + length(Dv(P)), area(P));
    Oi = 1;
}
