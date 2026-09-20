/* GitHub issue #3 (spec 017, US5) regression: textureinfo() silently
 * no-op'd under the LLVM JIT -- same kHandledOpcodes[] coverage-gate
 * failure mode as visibility()/occlusion() etc.
 *
 * Deliberately queries a NONEXISTENT texture file rather than a real one:
 * loading ANY real otexmake-produced .tex file currently hits an
 * unrelated, separately-filed bug (GitHub issue #7 -- libtiff's own
 * benign "ExtraSamples" warning is routed through this engine's FATAL
 * error() path, so orender exits nonzero even though the texture loads
 * and the render completes correctly) that would fail this test's
 * exit-code check regardless of what the rendered pixels show.
 *
 * CRenderer::getTextureInfo() returns NULL for a file it can't locate
 * (rendererFiles.cpp) -- both the interpreter and the JIT then take
 * TEXTUREINFO_PRE's `found = 0` / "prevent writing result" path
 * identically, so this still directly exercises the coverage-gate defect
 * class this spec targets (silent no-op under JIT) even though it can't
 * verify a successful lookup's actual channel data.
 */
surface textureinfo_probe()
{
    uniform string missing = "017_textureinfo_probe_does_not_exist.tex";

    /* Pre-set to a sentinel that only a genuinely silently-skipped call
     * (the exact defect class this spec targets) would leave behind --
     * TEXTUREINFOF's `*res = found;` always executes regardless of
     * whether the query matched, so a correctly-dispatched call always
     * overwrites this with 0 (not found), distinguishing "correctly
     * computed not-found" from "opcode silently no-op'd". */
    float res[2];
    float resFound = -1;
    resFound = textureinfo(missing, "resolution", res);

    float exists = 0;
    float existsFound = -1;
    existsFound = textureinfo(missing, "exists", exists);

    float resCorrect = 0;
    if (resFound == 0) resCorrect = 1;
    float existsCorrect = 0;
    if (existsFound == 0) existsCorrect = 1;

    Ci = color(resCorrect, existsCorrect, 0.5);
}
