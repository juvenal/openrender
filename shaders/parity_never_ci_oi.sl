/*
 * parity_never_ci_oi.sl
 *
 * Regression fixture for spec 014-jit-shading-parity, T032 (SC-001's
 * durable -L visual guard). Deliberately never assigns Ci or Oi, so the
 * rendered color/opacity comes entirely from CShadingContext::complete()'s
 * default-fill fallback (interpolating the bound Color/Opacity attributes
 * into Ci/Oi when usedParameters lacks PARAMETER_CI/PARAMETER_OI). Before
 * the fix, the JIT backend always set both bits regardless of whether the
 * shader referenced Ci/Oi, so this fallback never ran under .slo and the
 * render read uninitialized varying storage instead.
 */
surface parity_never_ci_oi(
    float unused_param = 1.0;
)
{
    float dummy = unused_param * 2;
}
