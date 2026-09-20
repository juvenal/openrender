/* GitHub issue #1 repro/regression: random()/urandom() silently no-op under
 * the LLVM JIT (--jit/.slo) because "random"/"urandom" were missing from
 * llvmEmitter.cpp's kHandledOpcodes[] dispatch table -- emitFunction()'s
 * coverage gate (llvmEmitter.cpp:826) silently skipped the call, emitting
 * zero IR, so the destination variable's grid buffer was never written.
 *
 * Exercises all four DEFFUNC/DEFLINKFUNC codegen paths added by that fix:
 * random() "f=", random() "v=", urandom() "f=", urandom() "v=".
 *
 * Deliberately avoids comp() -- one of the still-unhandled JIT builtins
 * (tracked in the follow-up GitHub issue for the broader missing-builtin
 * sweep) -- in favor of xcomp()/ycomp()/zcomp(), which are handled, so this
 * probe isolates random()/urandom() without being contaminated by an
 * unrelated JIT gap.
 *
 * `Oi = 1;` is required: the JIT does not default Oi to opaque when a
 * shader never assigns it, so any .slo shader that omits this renders
 * fully transparent regardless of Ci (see usfroma_probe.sl's header note
 * for the empirical trace that found this).
 */
surface random_probe()
{
    float rf = random();
    vector rv = random();
    float uf = urandom();
    vector uv = urandom();

    Oi = 1;
    Ci = color(rf, xcomp(rv), uf) * 0.5 +
         color(xcomp(uv), ycomp(uv), zcomp(uv)) * 0.5;
}
