/* GitHub #234: dlo_uv_width plus a constant (0.25, 0.5, 0.75), so that a hit is told from a miss (black) when the dlo is seen in a
 * mirror: Ci must be exactly (0.25, 0.5, 0.75). */
surface dlo_uv_width_marked()
{
    Ci = color(0.25, 0.5, 0.75) + color(du, dv, u + v + s + t);
    Oi = 1;
}
