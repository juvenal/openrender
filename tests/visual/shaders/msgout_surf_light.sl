/* GitHub #209: a light that reads the varying output parameter s of the surface it lights, with surface(). */
light msgout_surf_light()
{
    float v = -1;
    float found = surface("s", v);
    solar(vector(0, 0, 1), 0) Cl = color(v, found, 0);
}
