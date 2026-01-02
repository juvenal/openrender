/**
 * show_st(): Color a surface point according to its s,t coordinates.
 *
 * From the "The RenderMan Companion"
 */

surface
show_st () {
    Ci = color "rgb" (s, t, 0);
    Oi = 1;
}
