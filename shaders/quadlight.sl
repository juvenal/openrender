/**
 * quadlight(): rectangular area light source shader
 *
 * openRender: RenderMan compliant renderer
 *
 * RSL replacement for the previously hardcoded CQuadLight
 * (src/ri/hcshader.{h,cpp}). A four-cornered planar emitter with
 * Lambertian (cosine) falloff from its own surface normal: for each
 * shading point, samples `numSamples` bilinearly-interpolated positions
 * across the quad, casts a raytraced shadow test to each, and averages a
 * 1/distance^2 * cos(light-normal, sample-direction) weighted term over
 * the unshadowed samples -- numSamples trades noise for soft-shadow
 * quality, exactly as the original C++ shader's own sampling loop did.
 *
 * `reverse` replaces the original's automatic lookup of the current
 * attribute state's "inside" flag (an internal engine bit with no
 * RSL-visible equivalent) with an explicit override a scene author can
 * set directly. `useDirection` similarly replaces the original's
 * "parameter was supplied" check (RSL has no way to ask whether an
 * optional parameter was set) with an explicit switch: leave it at its
 * default of 0 to compute the light's normal from the four corners, or
 * set it to 1 and supply `direction` to override that normal outright.
 *
 * The light's own normal is written into the built-in global N (as
 * arealight.sl/shadowarea.sl/rayarea.sl do) rather than kept in a local
 * variable, and that N is what illuminate()'s 3-argument (P, axis, angle)
 * form is given: this engine's compiler only evaluates that hemisphere
 * cone test correctly when axis is the global N, not an arbitrary local
 * normal -- confirmed by direct render testing (a locally-computed axis
 * silently illuminates nothing, with no compile or runtime error).
 *
 * The falloff term is N.normalize(L), not -N.normalize(L): illuminate()'s
 * own L convention here already has the sign the original C++ shader
 * built by hand (which negated its own manually-computed L) -- matching
 * shadowarea.sl/rayarea.sl's identical N.normalize(L) usage. Confirmed by
 * render testing: the negated form silently produced a fully black light
 * (a negative Cl, clamped away) with no compile or runtime error either.
 *
 * Render with the interpreter (.rslo), this engine's default -- the LLVM
 * JIT path (--jit / "shaderformat" "slo") renders this light as visible
 * checkerboard noise instead of a clean soft shadow. See CLAUDE.md's
 * "Known gotchas" #12; this is an engine-level random() JIT defect, not
 * something fixable from within this shader.
 */

light
quadlight (point  P0           = point "shader" (-1, -1, 0),
                  P1           = point "shader" ( 1, -1, 0),
                  P2           = point "shader" (-1,  1, 0),
                  P3           = point "shader" ( 1,  1, 0);
           vector direction    = vector (0, 0, 0);
           float  useDirection = 0;
           color  lightcolor   = 1;
           float  intensity    = 1;
           float  numSamples   = 1;
           float  reverse      = 0) {

    point  center;
    float  i, u, v, vis;
    point  edge0, edge1, Psample;
    vector toSample;

    center = (P0 + P1 + P2 + P3) / 4;

    if (useDirection != 0) {
        N = normalize(direction);
    }
    else {
        N = normalize((P1 - P0) ^ (P2 - P0));
    }

    if (reverse != 0) {
        N = N * -1;
    }

    illuminate (center, N, PI/2) {
        vis = 0;

        for (i = 0; i < numSamples; i += 1) {
            u = random();
            v = random();

            edge0 = P0 + u * (P1 - P0);
            edge1 = P2 + u * (P3 - P2);
            Psample = edge0 + v * (edge1 - edge0);

            toSample = Psample - Ps;
            vis += visibility(Ps, Psample) / (toSample . toSample);
        }

        vis /= numSamples;
        Cl = vis * intensity * lightcolor * (N . normalize(L));
    }
}
