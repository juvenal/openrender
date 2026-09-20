/* Companion Displacement shader for spec 017 (US5) T055's
 * shader_parameter_probe.sl -- declares one named parameter so the probe's
 * displacement() call has something real to find. Does nothing else (no
 * actual displacement).
 */
displacement param_query_displacement(float testDispParam = 0.25;
                                      vector testDispVec = vector(0.1, 0.2, 0.3);)
{
}
