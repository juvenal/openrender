/* T009 -- US1 Red artifact: minimal reproduction of the usfroma interpreter
 * crash (varying-index read of a uniform string array), FR-002.
 *
 * Shape recorded at specs/011-jit-opcode-parity/triage-results.md:85 and
 * bisected in that spec's session to this exact 4-line minimal case: a
 * correctly-sized uniform string array read at a varying index provably
 * bounded in-range, consumed inline in an expression (a string comparison)
 * rather than assigned to a declared local -- string variables are
 * grammar-forced uniform (rslo.y:342-347's rsloStringSpecifier), so no RSL
 * string variable can ever hold the varying read result. This inline
 * consumption is what triggers the `usfroma` opcode instead of `sfroma`.
 *
 * STALE (corrected 2026-09-20, spec 017-jit-builtin-function-coverage,
 * T076/research.md D6): this comment previously claimed `Oi = 1;` was
 * required here because "the JIT does not default Oi to opaque when a
 * shader never assigns it." That is no longer true -- spec 014's
 * PARAMETER_CI/PARAMETER_OI default-fill gating fix landed after this
 * comment was written, and test_used_parameters_gating.cpp's T008 /
 * test_used_parameters_oracle.cpp's T010 both explicitly test and pass
 * "shader never assigns Ci/Oi -> PARAMETER_OI correctly clear, .slo/.rslo
 * usedParameters bit-identical" today. The `Oi = 1;` line below has been
 * removed as no longer needed; kept here only as a record of a confound
 * this probe once had to work around.
 */
surface usfroma_probe()
{
    uniform string usarr[3] = {"a", "b", "c"};

    /* findex ranges continuously over [0,3) and truncates to an int index
     * of 0, 1, or 2 -- provably in-range for usarr's 3 elements. */
    varying float findex = mod(u * 3, 3);

    float matchFlag = 0;
    if (usarr[findex] == "a")      matchFlag = 1;   /* usfroma */
    else if (usarr[findex] == "b") matchFlag = 0.5;
    else                            matchFlag = 0;

    Ci = color(matchFlag, matchFlag, matchFlag);
}
