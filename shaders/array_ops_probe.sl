/* Phase 3 (US2) triage repro — array move-op opcode candidates.
 * Compiled with plain `oshader` (non-JIT); the resulting .rslo text is
 * grepped for each literal mnemonic to confirm the frontend actually
 * emits it. Not rendered, not JIT-compiled — reachability only.
 *
 * expression.cpp:CArrayExpression::getCode (lines 640-683) dispatches
 * array-element READS: matched uniformity (array and destination both
 * uniform, or both varying) emits f/v/m/sfroma; mismatched uniformity
 * (uniform array read into a varying-forcing context, e.g. indexed by a
 * varying expression) emits the u-prefixed uf/uv/um/usfroma family instead.
 * expression.cpp:CArrayAssignmentExpression::getCode (lines 1689-1716)
 * dispatches array-element WRITES (`arr[i] = expr;`) to f/v/m/stoa,
 * uniformly regardless of the array's own uniformity.
 *
 * String reads (sfroma/usfroma) need a different trigger than the
 * float/vector/matrix cases: rslo.y:342-347's rsloStringSpecifier
 * unconditionally bakes SLC_UNIFORM into the bare `string` type, so a
 * `varying` prefix is a no-op and no RSL string variable can ever hold a
 * varying value ("Can not assign to uniform variable" if attempted). A
 * varying-indexed string-array read is still a valid *expression*, so
 * sfroma is triggered by consuming the read directly in an expression
 * context (a string comparison) instead of assigning it to a declared
 * local.
 *
 * usfroma (varying-index read of a uniform string array) is DELIBERATELY
 * NOT exercised here. Bisected in a standalone repro
 * (scratchpad bisect4/6.sl, this session) down to a 4-line minimal case:
 * `if (usarr[findex] == "a") ...` with a correctly-sized 3-element
 * uniform string array and a varying index provably bounded to {0,1,2}
 * segfaults CShadingContext::execute — the .rslo INTERPRETER's own
 * bytecode dispatch loop, not the JIT emitter (reproduces identically on
 * a plain, non-`-slo` render). Since no valid rslo ground-truth reference
 * can be produced, usfroma cannot be covered by this rslo-vs-slo parity
 * visual test; the interpreter itself is out of scope to fix (ground-truth
 * non-goal). See DEVNOTES_DETAILS/BUGS.md. usfroma's JIT-emitter coverage
 * is still verified separately via LibShader_OpcodeCoverage.
 *
 * Candidates exercised here: ffroma, vfroma, mfroma, sfroma, uffroma,
 * uvfroma, umfroma, ftoa, vtoa, mtoa, stoa.
 *
 * NOTE: farr/varr are declared uninitialized and filled via element
 * assignment rather than a `{...}` initializer list. A `varying` array
 * initializer list containing a binary expression among varying elements
 * (e.g. `varying float farr[3] = {u, v, u + v};`) segfaults oshader —
 * bisected down to the binary-expression element specifically (a 3-element
 * varying list of plain variables, or a 2-element list with a binary
 * expression, both compile fine; a 2-element varying list `{u, u + v}`
 * reproduces the crash). This is an independent oshader compiler bug, out
 * of scope for this triage; flagged for the user separately, not fixed
 * here — this repro just avoids the crashing pattern.
 */
surface array_ops_probe()
{
    uniform float ufarr[3] = {1, 2, 3};
    uniform vector uvarr[3] = {vector(1,0,0), vector(0,1,0), vector(0,0,1)};
    uniform matrix umarr[3] = {matrix(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1),
                                matrix(2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,1),
                                matrix(3,0,0,0, 0,3,0,0, 0,0,3,0, 0,0,0,1)};

    varying float farr[3];
    varying vector varr[3];
    varying matrix marr[3];
    varying string sarr[2];

    /* findex ranges continuously over [0,3) and truncates to an int index
     * of 0, 1, or 2 — every varying-indexed array below must have exactly
     * 3 elements so the read stays in bounds for every index value. */
    varying float findex = mod(u * 3, 3);
    uniform float uidx = 1;

    /* Array element writes: ftoa, vtoa, mtoa, stoa. */
    farr[0] = u;
    farr[1] = v;
    farr[2] = u + v;
    varr[0] = vector(u, v, 0);
    varr[1] = vector(v, u, 0);
    varr[2] = vector(u, u, v);
    marr[0] = umarr[0];
    marr[1] = umarr[1];
    marr[2] = umarr[2];
    sarr[0] = "x";
    sarr[1] = "y";

    /* Matched-uniformity reads (varying array + varying index -> varying
     * result matches the varying array): ffroma, vfroma, mfroma. */
    float rf = farr[findex];
    vector rv = varr[findex];
    matrix rm = marr[findex];

    /* Mismatched-uniformity reads (uniform array + varying index forces a
     * varying result, differing from the array's own uniformity):
     * uffroma, uvfroma, umfroma. */
    float ruf = ufarr[findex];
    vector ruv = uvarr[findex];
    matrix rum = umarr[findex];

    /* String read: sarr is grammar-forced uniform (see header note); a
     * uniform index gives a matched read (sfroma), consumed directly in an
     * expression since no string variable can hold the read result. usfroma
     * (varying-index read of a uniform string array) is deliberately not
     * exercised here — see header note. */
    float matchFlag = 0;
    if (sarr[uidx] == "x")    matchFlag += 1;   /* sfroma */

    Ci = Cs * (rf + comp(rv,0,0) + comp(rm,0,0) + ruf + comp(ruv,0,0) + comp(rum,0,0) + matchFlag);
}
