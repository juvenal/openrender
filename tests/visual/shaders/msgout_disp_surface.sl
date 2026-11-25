/* GitHub #209: v is declared varying and initialised from a constant. The call must read the varying output parameter d of the
 * displacement shader into it (RISpec 3.2: varying into uniform is a failure). Ci = (|v - xcomp(P)|, found, 0): (0, 1, 0) when
 * the value arrived per point; v stays -1 and found is 0 when the call is refused. */
surface msgout_disp_surface()
{
    float v = -1;
    float found = displacement("d", v);
    Ci = color(abs(v - xcomp(P)), found, 0);
    Oi = 1;
}
