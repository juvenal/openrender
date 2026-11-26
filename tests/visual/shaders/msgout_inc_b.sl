/* GitHub #209: reads s of the volume shader on the side the ray comes from (kind 0, incident()) or on the other side (kind 1,
 * opposite()) into a varying local initialised from a constant. The source is the volume shader's stored default, uniform, so
 * this cannot fail before the fix; it guards the exemption. Ci = (v, found, 0) = (0.5, 1, 0). */
surface msgout_inc_b(float kind = 0)
{
    float v = -1;
    float found = 0;
    if (kind == 0)
        found = incident("s", v);
    else
        found = opposite("s", v);
    Ci = color(v, found, 0);
    Oi = 1;
}
