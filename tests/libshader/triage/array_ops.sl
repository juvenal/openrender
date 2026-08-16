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
 * sfroma/usfroma are triggered by consuming the read directly in an
 * expression context (a string comparison) instead of assigning it to a
 * declared local.
 *
 * Candidates exercised: ffroma, vfroma, mfroma, sfroma, uffroma, uvfroma,
 * umfroma, usfroma, ftoa, vtoa, mtoa, stoa.
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
    uniform vector uvarr[2] = {vector(1,0,0), vector(0,1,0)};
    uniform matrix umarr[1] = {matrix(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)};
    uniform string usarr[2] = {"a", "b"};

    varying float farr[3];
    varying vector varr[2];
    varying matrix marr[1];
    varying string sarr[2];

    varying float findex = mod(u * 3, 3);
    uniform float uidx = 1;

    /* Array element writes: ftoa, vtoa, mtoa, stoa. */
    farr[0] = u;
    varr[0] = vector(u, v, 0);
    marr[0] = umarr[0];
    sarr[0] = "x";

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

    /* String reads: sarr is grammar-forced uniform (see header note), so a
     * uniform index gives a matched read (sfroma) and a varying index gives
     * a mismatched read (usfroma); both consumed directly in an expression
     * since no string variable can hold the varying-index result. */
    float matchFlag = 0;
    if (sarr[uidx] == "x")    matchFlag += 1;   /* sfroma */
    if (usarr[findex] == "a") matchFlag += 1;   /* usfroma */

    Ci = Cs * (rf + comp(rv,0,0) + comp(rm,0,0) + ruf + comp(ruv,0,0) + comp(rum,0,0) + matchFlag);
}
