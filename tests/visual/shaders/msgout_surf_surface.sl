/* GitHub #209: a surface with a varying output parameter s, read by msgout_surf_light with surface(). */
surface msgout_surf_surface(output varying float s = 0.5)
{
    color c = 0;
    illuminance(P) c += Cl;
    Ci = c;
    Oi = 1;
}
