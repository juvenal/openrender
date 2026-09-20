/* GitHub issue #3 (spec 017, US1) regression: trace() silently no-ops under
 * the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure mode as
 * visibility()/transmission(). trace(P, V) has two overloads selected by
 * the assignment target's type: TraceF ("f=pv!", nearest-hit distance) and
 * TraceV ("c=pv!", reflected surface color) -- both #define straight to
 * TRANSMISSIONEXPR_PRE/TRANSMISSIONEXPR/_UPDATE (giFunctions.h), differing
 * only in TRACE2EXPR_POST vs. TRACEEXPR_POST's post-processing, so this one
 * probe exercises both JIT dispatch branches (float-vs-color, selected by
 * destination stride in llvmEmitter.cpp).
 */
surface trace_probe()
{
    vector Vdir = normalize(vector(0.2, 0.15, 1));
    float hitDist = trace(P, Vdir);
    color hitColor = trace(P, Vdir);

    Ci = hitColor + color(hitDist, hitDist, hitDist) * 0.001;
}
