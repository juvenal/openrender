/* GitHub #234: a dynamic-load object's ray hit has defined u, v, s, t and filter widths. The dlo interface supplies no
 * parametrisation, so they are all 0. Ci is (du, dv, u + v + s + t); every channel must be exactly 0. */
surface dlo_uv_width()
{
    Ci = color(du, dv, u + v + s + t);
    Oi = 1;
}
