/* Spec 039, Phase 6 follow-up (Sampling_DisplaceThreadSweep, Sampling_DisplacePassConsistency):
 * a displacement that is a draw. The same (u, v) of a surface must displace by the same amount
 * in the dicer's displacement-only probe passes and in the full shading pass, whichever thread
 * runs them and in whatever order.
 */
displacement sampling_displace_probe()
{
    P += 0.05 * random() * normalize(N);
    N = calculatenormal(P);
}
