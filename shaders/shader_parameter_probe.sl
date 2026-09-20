/* GitHub issue #3 (spec 017, US5) regression: surface()/displacement()/
 * incident()/opposite() silently no-op under the LLVM JIT -- same
 * kHandledOpcodes[] coverage-gate failure mode as visibility() etc. (US1).
 * These query a NAMED parameter from the object's currently-bound shader
 * instance of the corresponding type (surface() queries its own bound
 * instance -- this shader queries its own declared parameters
 * self-referentially; displacement()/incident()/opposite() query
 * companion shaders bound via the paired RIB scene's Displacement/
 * Interior/Exterior statements -- see shaders/param_query_displacement.sl
 * and shaders/param_query_volume.sl). Covers both the float and vector
 * result-type forms (research.md D10).
 *
 * atmosphere() is DELIBERATELY not exercised here: found, incidentally,
 * to be unregistered in the compiler's own symbol table
 * (src/libshader/compiler/rslo.cpp's addBuiltInFunction calls never list
 * it, unlike its siblings surface/displacement/incident/opposite, all
 * present) -- it cannot be called from ANY RSL shader today, under either
 * backend, a genuine pre-existing compiler bug unrelated to JIT coverage.
 * Filed as a separate GitHub issue; jitAtmosphereParameter's JIT-side
 * implementation still exists (shading.cpp) for when that's fixed, but
 * has no probe coverage until then.
 */
surface shader_parameter_probe(float testParam = 0.5;
                               vector testVec = vector(0.6, 0.7, 0.8);)
{
    float foundSurfF, foundSurfV, foundDispF, foundDispV;
    float foundInc, foundOpp;
    float sF, dF, iF, oF;
    vector sV, dV;

    foundSurfF = surface("testParam", sF);
    foundSurfV = surface("testVec", sV);
    foundDispF = displacement("testDispParam", dF);
    foundDispV = displacement("testDispVec", dV);
    foundInc = incident("testVolParam", iF);
    foundOpp = opposite("testVolParam", oF);

    float sumFound = foundSurfF + foundSurfV + foundDispF + foundDispV +
                      foundInc + foundOpp;

    Ci = color(sF + dF, sV[0] + dV[0], sumFound / 6);
}
