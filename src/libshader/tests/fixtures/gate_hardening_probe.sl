/* Fixture for the gate-hardening negative test (spec 017-jit-builtin-
 * function-coverage, US2, T018, contracts/gate-hardening-contract.md's
 * Verification Obligation #2). Calls degrees() -- a real RSL builtin
 * function (scriptFunctions.h) with no emitFunction() case as of this
 * writing -- to confirm oshader --jit fails loudly (nonzero exit,
 * diagnostic naming the mnemonic, no .slo written) instead of silently
 * skipping it.
 *
 * NOTE: if a future spec implements degrees() under the JIT, this fixture
 * must be repointed at a different still-unhandled builtin function
 * (`ctest -L libshader` failure output from LibShader_OpcodeCoverage names
 * every current gap) -- it is not itself the correctness guard for
 * degrees(); that is LibShader_OpcodeCoverage's job.
 */
surface gate_hardening_probe()
{
    Ci = color(degrees(1), 0, 0);
}
