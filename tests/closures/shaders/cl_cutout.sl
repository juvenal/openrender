/*
 * Spec 035, Phase 3 (T058): a probe surface whose opacity is its own, not the object's Os: Oi = 0.25 whatever Os is. Under `closures` the
 * image keeps this opacity (Q-R5, RISpec 3.2 section 12.1); under camerahitmode "primitive" the primitive's Os overrides it afterwards.
 */
surface cl_cutout()
{
    Oi = 0.25;
    Ci = Cs * Oi;
}
