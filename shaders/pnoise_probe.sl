/* GitHub issue #3 (spec 017, US5) regression: pnoise() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure mode
 * as visibility()/occlusion() etc. Exercises the 1D float form
 * (pnoise(f,period)) and the 3D float form (pnoise(p,pp)); the 2D and 4D
 * forms, and all vector-result forms, share the exact same dispatch
 * mechanism (disambiguated by operand count + stride, llvmEmitter.cpp)
 * and are implemented but not separately probed here.
 */
surface pnoise_probe()
{
    /* pnoiseFloat()/pnoiseVector() (noise.cpp) already normalize their
     * output to roughly [0,1], same convention as plain noise() -- no
     * extra remapping needed before writing to Ci. */
    float n1d = pnoise(u * 4, 4);
    point Pp = transform("shader", P);
    float n3d = pnoise(Pp, point(2, 2, 2));

    Ci = color(n1d, n3d, 0.5);
}
