/* Companion volume-class shader for spec 017 (US5) T055's
 * shader_parameter_probe.sl -- bindable as Atmosphere/Interior/Exterior.
 * Declares one named parameter so atmosphere()/incident()/opposite() have
 * something real to find. Passes color/opacity through unchanged.
 */
volume param_query_volume(float testVolParam = 0.75;
                          vector testVolVec = vector(0.4, 0.5, 0.6);)
{
    Ci = Ci;
    Oi = Oi;
}
