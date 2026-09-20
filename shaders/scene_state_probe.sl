/* GitHub issue #3 (spec 017, US5) regression: attribute()/option()/
 * rendererinfo() silently no-op under the LLVM JIT -- same
 * kHandledOpcodes[] coverage-gate failure mode as visibility() etc.
 * Shares the exact same PARAMETEREXPR mechanism as surface()/displacement()/
 * incident()/opposite() (shader_parameter_probe.sl, research.md D10),
 * differing only in querying scene/attribute-level state (accessor 0)
 * instead of a bound shader instance's own parameters. Covers the float
 * (attribute "ShadingRate"), vector (option "Format"), and string
 * (rendererinfo "renderer") result-type forms -- attribute()/option()
 * always write directly into dest (the CVariable pointer and globalIndex
 * output params are never populated by these three accessors, confirmed
 * by reading
 * CShadingContext::attributes/options/rendererInfo directly), so this
 * probe also exercises jitParameterFinish's cVar==nullptr self-copy path
 * from a different angle than shader_parameter_probe.sl's incident()/
 * opposite() coverage.
 */
surface scene_state_probe()
{
    uniform float sr;
    uniform float fmt[3];
    uniform string rendererName;

    float foundAttr = attribute("ShadingRate", sr);
    float foundOpt = option("Format", fmt);
    float foundInfo = rendererinfo("renderer", rendererName);

    float sumFound = foundAttr + foundOpt + foundInfo;

    Ci = color(sr / 10, fmt[0] / 1000, sumFound / 3);
}
