/* Spec 027 (research R15, T004): lookup values for the Texturing-only test (SC-003).
 *
 * Calls the eight-coordinate texture() only: the form that carries its own footprint,
 * so a program that links no shading code can reproduce each call exactly (it builds the
 * same four (s,t) corners and the same TextureOpt). environment() and shadow() are not
 * here on purpose: they take one direction / one point and Shading derives the
 * footprint, which the Texturing-only program would have to invent.
 *
 * Every lookup uses "samples" 1 and the box filter, so the sample stream has no effect
 * on the value (there is no jitter to draw), and printf prints the result with %c.
 * Only options the Texturing-only program can set from a caller-built TextureOpt are used:
 * no "blur" (an unbaked lookup with blur draws extra stream dimensions), no "width" (the
 * caller's footprint is not read by the eight-coordinate form), no per-channel form (the
 * channel is chosen by Shading), no filter other than box.
 * The shader's output does not depend on the shaded point, so every shaded point prints
 * the same lines: the script sorts them with `sort -u`.
 *
 * One baked fixture and two unbaked ones (a power-of-two PNG and a non-power-of-two PNG,
 * which take the synthesized-source path).
 */
void probe(string name, tag)
{
    color c;

    /* A: a small footprint (point-like) */
    c = texture(name, 0.30, 0.40, 0.31, 0.40, 0.30, 0.41, 0.31, 0.41, "filter", "box", "samples", 1);
    printf("%s A %c\n", tag, c);
    /* B: a medium footprint */
    c = texture(name, 0.25, 0.25, 0.35, 0.25, 0.25, 0.35, 0.35, 0.35, "filter", "box", "samples", 1);
    printf("%s B %c\n", tag, c);
    /* C: a wide footprint (selects a coarse level) */
    c = texture(name, 0.10, 0.10, 0.60, 0.10, 0.10, 0.60, 0.60, 0.60, "filter", "box", "samples", 1);
    printf("%s C %c\n", tag, c);
    /* D: across the wrap boundary */
    c = texture(name, 0.95, 0.95, 1.05, 0.95, 0.95, 1.05, 1.05, 1.05, "filter", "box", "samples", 1);
    printf("%s D %c\n", tag, c);
    /* E: a skewed, anisotropic quad */
    c = texture(name, 0.20, 0.30, 0.45, 0.32, 0.22, 0.34, 0.47, 0.36, "filter", "box", "samples", 1);
    printf("%s E %c\n", tag, c);
}

surface lookupprobe(string baked = "concurrency_rgb.tex";
                    string unbaked = "tiny_rgb8.png";
                    string nonpot = "nonpot_rgb.png")
{
    probe(baked, "baked");
    probe(unbaked, "unbaked");
    probe(nonpot, "nonpot");
    Ci = 0;
    Oi = 1;
}
