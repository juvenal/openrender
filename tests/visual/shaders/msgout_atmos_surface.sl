/* GitHub #209: as msgout_disp_surface, for atmosphere(). Ci = (v, found, 0): (0.5, 1, 0) when read, (-1, 0, 0) when refused. */
surface msgout_atmos_surface()
{
    float v = -1;
    float found = atmosphere("a", v);
    Ci = color(v, found, 0);
    Oi = 1;
}
