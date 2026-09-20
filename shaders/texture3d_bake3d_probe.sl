/* GitHub issue #3 (spec 017, US5) regression: texture3d()/bake3d()
 * silently no-op'd under the LLVM JIT -- same kHandledOpcodes[] coverage-
 * gate failure mode as visibility()/occlusion() etc. Reuses US1's
 * occlusion()/indirectdiffuse() point-cloud-lookup JIT architecture
 * (research.md D11), not a ray batch.
 *
 * Only the base positional arguments are supported by the JIT side (same
 * scoping decision as occlusion/indirectdiffuse's own "!"-suffixed
 * extra-channel-binding extension): no user-bound channels, and the
 * named "coordinatesystem"/"interpolate"/"radius"/"radiusscale" optional
 * parameters always use CTexture3dLookup::init()'s own defaults. This
 * means neither call transfers any real per-point DATA -- both simply
 * report their fixed "1" success flag (matching the interpreter's own
 * BAKE3DEXPR/TEXTURE3DEXPR, which unconditionally set `*res = 1`
 * regardless of channel content). This probe therefore exercises
 * execution/memory-safety and the correct success-flag value, not a
 * baked-data round-trip. bake3d()'s "rgb" channels argument is a
 * syntactically-valid placeholder only (must name an actual predeclared
 * display channel -- CPointCloud's constructor errors on an empty or
 * unknown one -- texture3d.cpp:119-136) -- its 3-float schema is never
 * actually read back by this probe's own zero-channel JIT/interpreter
 * lookup path.
 *
 * bake3d() and texture3d() are called against the SAME point-cloud
 * filename within one shader execution: CRenderer::getTexture3d() caches
 * texture3d objects per-frame by name (rendererFiles.cpp), so the second
 * (read) call resolves to the SAME in-memory CPointCloud object the first
 * (write) call just created -- no separate bake/read render passes or an
 * on-disk file are needed for this probe.
 */
surface texture3d_bake3d_probe()
{
    uniform string pcName = "017_texture3d_bake3d_probe.ptc";

    float bakeOk = bake3d(pcName, "rgb", P, N);
    float readOk = texture3d(pcName, P, N);

    Ci = color(bakeOk, readOk, 0.5);
}
