/* Phase 3 (US2) triage repro — gather()/gatherElse/gatherEnd/gatherHeader.
 * Compiled with plain `oshader` (non-JIT); the resulting .rslo text is
 * grepped for each literal mnemonic to confirm the frontend actually
 * emits it. Not rendered, not JIT-compiled — reachability only.
 *
 * Grammar: rsloGatherHeader (rslo.y:1831) accepts SL_GATHER "(" <comma
 * list> ")" followed by a matched/unmatched statement and optional
 * "else" clause (CGatherThenElse, rslo.y:1847-1876). Both the with-else
 * and without-else forms are exercised below so gatherElse's reachability
 * (conditional on the else clause being present) is confirmed separately
 * from gather/gatherEnd/gatherHeader (present in both forms).
 */
surface gather_probe()
{
    normal Nn = normalize(N);
    color Csum = 0;
    float samples = 4;

    /* with else: exercises gatherHeader, gather, gatherElse, gatherEnd */
    gather("illuminance", P, Nn, PI * 0.5, samples, "surface:Ci", Csum)
    {
        Ci += Csum;
    }
    else
    {
        Ci += 0;
    }

    /* without else: exercises gatherHeader, gather, gatherEnd (no gatherElse) */
    gather("illuminance", P, Nn, PI * 0.5, samples, "surface:Ci", Csum)
    {
        Ci += Csum;
    }
}
