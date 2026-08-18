/*
 * show_st_hsv(): Color a surface point by its (s,t) parametric coordinates,
 * via the explicit-colorspace "hsv" color constructor (cfrom).
 *
 * Diagnostic shader for spec 011-jit-opcode-parity: exercises the "hsv"
 * conversion path in convertColorFrom() rather than the identity "rgb"
 * path, so a JIT/interpreter divergence in the resolved color system
 * (not just a dropped write) shows up as a visible pixel difference.
 */

surface
show_st_hsv () {
    Ci = color "hsv" (s, t, 1);
    Oi = 1;
}
