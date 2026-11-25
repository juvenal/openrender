/* GitHub #209: a displacement shader with a varying output parameter, read by msgout_disp_surface with displacement(). */
displacement msgout_disp(output varying float d = 0)
{
    d = xcomp(P);
}
