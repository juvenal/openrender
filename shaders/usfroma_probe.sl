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
 * `Oi = 1;` is required for a reason unrelated to usfroma: the JIT does not
 * default Oi to opaque when a shader never assigns it (see measurements.md's
 * T008b section for the empirical trace that found this), so any .slo
 * shader that omits this line renders fully transparent regardless of Ci.
 * This probe is also used to compile a .slo counterpart (T017) once the
 * interpreter-side fix lands, so it must not carry that same confound.
 */
surface usfroma_probe()
{
    uniform string usarr[3] = {"a", "b", "c"};

    /* findex ranges continuously over [0,3) and truncates to an int index
     * of 0, 1, or 2 -- provably in-range for usarr's 3 elements. */
    varying float findex = mod(u * 3, 3);

    Oi = 1;

    float matchFlag = 0;
    if (usarr[findex] == "a")      matchFlag = 1;   /* usfroma */
    else if (usarr[findex] == "b") matchFlag = 0.5;
    else                            matchFlag = 0;

    Ci = color(matchFlag, matchFlag, matchFlag);
}
