/* GitHub issue #3 (spec 017, US3) regression: photonmap() silently
 * no-op'd under the LLVM JIT -- same kHandledOpcodes[] coverage-gate
 * failure mode as visibility()/occlusion() etc.
 *
 * Queries a NONEXISTENT photon map file: CRenderer::getPhotonMap()
 * always constructs a CPhotonMap object regardless of whether the file
 * was found (rendererFiles.cpp:405-431, same "always non-null" pattern
 * as getTexture3d's dummy blank-channel fallback) -- an empty/dummy
 * photon map's lookup() deterministically returns black, so both
 * backends can be compared without needing a real, portably-
 * referenceable .pm fixture file (the same portability problem already
 * hit for texture3d/textureinfo). Exercises both overloads (2- and
 * 3-argument forms) -- the 3rd (N) argument is accepted but unused even
 * by the interpreter's own macro.
 *
 * c2/c3 are pre-set to DIFFERENT sentinel colors before each call: a
 * genuinely silently-skipped opcode (this spec's own target defect
 * class) would leave each at its own distinct sentinel, giving
 * sameResult=0; a correctly-dispatched call overwrites both to the same
 * (black) empty-map result, giving sameResult=1. Without the sentinels,
 * a silent skip and a correct "both are black" result would be
 * numerically indistinguishable.
 */
surface photonmap_probe()
{
    uniform string missing = "017_photonmap_probe_does_not_exist.pm";

    color c2 = color(0.1, 0.2, 0.3);
    c2 = photonmap(missing, P);

    color c3 = color(0.4, 0.5, 0.6);
    c3 = photonmap(missing, P, N);

    float sameResult = 0;
    if (c2 == c3) sameResult = 1;

    Ci = color(sameResult, sameResult, 0.5);
}
