/**
 * spherelight(): spherical area light source shader
 *
 * openRender: RenderMan compliant renderer
 *
 * RSL replacement for the previously hardcoded CSphereLight
 * (src/ri/hcshader.{h,cpp}). Approximates a spherical area emitter: for
 * each shading point, samples `numSamples` random positions on the
 * sphere's surface, casts a raytraced shadow test to each, and averages
 * 1/distance^2 falloff over the unshadowed samples -- numSamples trades
 * noise for soft-shadow quality, exactly as the original C++ shader's own
 * sampling loop did.
 *
 * Render with the interpreter (.rslo), this engine's default -- the LLVM
 * JIT path (--jit / "shaderformat" "slo") renders this light fully black.
 * See CLAUDE.md's "Known gotchas" #12; this is an engine-level random()
 * JIT defect, not something fixable from within this shader.
 */

light
spherelight (point from       = point "shader" (0, 0, 0);
             float radius     = 0;
             color lightcolor = 1;
             float intensity  = 1;
             float numSamples = 1) {

    float worldRadius;
    float i, u1, u2, theta, z, r;
    vector dir, toSample;
    point Psample;
    float vis;

    /* radius is authored in shader space; carry it into current space the
     * same way the original hardcoded light scaled it (by the local
     * transform), rather than leaving it a bare unscaled number. */
    worldRadius = length(vtransform("shader", vector(radius, 0, 0)));

    illuminate (from) {
        vis = 0;

        for (i = 0; i < numSamples; i += 1) {
            /* Uniform point on the unit sphere via the standard
             * cylindrical-projection construction (Archimedes' theorem) --
             * exact and rejection-free, unlike the original's reject-until-
             * inside-the-unit-cube loop, but produces the same uniform
             * surface distribution. */
            u1 = random();
            u2 = random();
            theta = 2 * PI * u1;
            z = 1 - 2 * u2;
            r = sqrt(max(0, 1 - z * z));
            dir = vector(r * cos(theta), r * sin(theta), z);

            Psample = from + worldRadius * dir;
            toSample = Psample - Ps;

            vis += visibility(Ps, Psample) / (toSample . toSample);
        }

        vis /= numSamples;
        Cl = vis * intensity * lightcolor;
    }
}
