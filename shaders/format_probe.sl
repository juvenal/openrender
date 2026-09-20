/* GitHub issue #3 (spec 017, US4) regression: format() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure
 * mode as visibility()/occlusion() etc. Exercises %f/%p/%s -- the
 * minimum token forms this task requires (note: PRINTEXPR,
 * scriptFunctions.h, recognizes %f/%d/%c/%n/%p/%s/%m only -- there is
 * no %v specifier; %c/%n/%p all print the identical "(%f,%f,%f)" form
 * for a vector/point/color/normal operand) -- mixing a float, a vector,
 * and a string operand in one call (confirmed via a throwaway probe's
 * compiled .rslo that each trailing operand keeps its own real RSL
 * type, unlike the interpreter's dual float-pointer and string-pointer
 * resolution of the same slot).
 *
 * Note: format()/printf() share PRINTEXPR (scriptFunctions.h) for this
 * token-scanning logic in the INTERPRETER -- but printf()'s own JIT
 * dispatch (llvmEmitter.cpp) turned out to be a silent no-op (grouped
 * with return/jmp, "no per-vertex output"), not a working implementation
 * to reuse, contrary to this spec's original research notes (corrected
 * in research.md). format() was implemented by transcribing PRINTEXPR
 * directly instead.
 *
 * Uses match() (already-shipped, T037) to turn the result-string
 * comparison into a float flag directly assigned (not if-gated), so
 * it's immune to GitHub issue #8 by construction.
 */
surface format_probe()
{
    uniform float f = 1.5;
    uniform vector v = vector(1, 2, 3);
    uniform string s = "hi";
    uniform string result = format("%f %p %s", f, v, s);

    uniform float ok = match(result, "1.500000 (1.000000,2.000000,3.000000) hi");

    Ci = color(ok, ok, 0.5);
}
