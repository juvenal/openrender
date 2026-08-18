/*
 * show_ctransform(): Color a surface point by its (s,t) parametric
 * coordinates, run through ctransform("hsv", ...).
 *
 * Diagnostic shader for spec 011-jit-opcode-parity: the JIT emitter
 * currently misroutes "ctransform" into the pfrom/point-transform family
 * (a 4x4 matrix transform), not the colorspace-conversion family it
 * actually is. This exercises ctransform() in isolation, with no matrix
 * transform involved anywhere else in the shader, so the two math families
 * cannot coincidentally agree.
 */

surface
show_ctransform () {
    Ci = ctransform("hsv", color(s, t, 0));
    Oi = 1;
}
