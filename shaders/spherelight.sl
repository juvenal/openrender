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
 * Both the interpreter (.rslo, this engine's default) and the LLVM JIT
 * (--jit / "shaderformat" "slo") render this light (the JIT's random() and
 * visibility() were fixed by spec 017, GitHub #1 and #3). visibility() occludes
 * objects flagged Attribute "visibility" "int transmission" 1 (GitHub #169); an
 * unflagged object casts no shadow.
 *
 * DEPRECATED (spec 057): light a scene with a real sphere instead,
 * Sphere under AreaLightSource "arealight". This shader stays for existing
 * scenes and its output is unchanged. The two are different emitters, not
 * one emitter sampled two ways: this shader radiates isotropically from
 * the sphere's surface points with no emitter cosine, so a receiver sees
 * 1 / r^2 from every point of the sphere, including the far side; a
 * geometric sphere is a Lambertian emitter, whose points that face away
 * from a receiver contribute nothing (arealight.sl: N.normalize(L) / L.L
 * inside illuminate(P, N, PI/2)). Expect a different falloff, not just
 * sampling noise.
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
