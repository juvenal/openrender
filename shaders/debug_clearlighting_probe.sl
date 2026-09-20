/* GitHub issue #3 (spec 017, US5) regression: clearlighting() silently
 * no-op'd under the LLVM JIT -- same kHandledOpcodes[] coverage-gate
 * failure mode as visibility()/attribute() etc. `debug()` is exercised by
 * this same builtin-function group in the spec, but is currently
 * uncallable from ANY RSL shader for an unrelated, pre-existing reason
 * (GitHub issue #5: "debug" has zero addBuiltInFunction registrations in
 * rslo.cpp -- a compiler-registration gap, same as atmosphere()'s), so it
 * is not exercised by this probe; its JIT-side implementation
 * (jitDebug/op_debug) still lands so it's ready once issue #5 is fixed.
 *
 * clearlighting() resets CShadingState::lightsExecuted/lights/freeLights
 * (execute.cpp's clearLighting() macro) -- exercising it here mainly
 * proves the JIT wrapper is memory-safe (doesn't corrupt subsequent
 * shading state) rather than proving its cache-invalidation semantic
 * specifically: diffuse()'s own iterateLights() cache check already
 * invalidates correctly on its own whenever N/P/tags actually change, so
 * a deterministic light source (arealight.sl) produces the same Cl
 * whether or not the cache was bypassed. This probe calls diffuse()
 * twice with clearlighting() forcing a full light-loop re-execution in
 * between and confirms the JIT and interpreter paths still agree.
 */
surface debug_clearlighting_probe()
{
    normal Nf = faceforward(normalize(N), I);

    color c1 = diffuse(Nf);
    clearlighting();
    color c2 = diffuse(Nf);

    Ci = (c1 + c2) / 2;
}
