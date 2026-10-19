/* Spec 039, Phase 6 follow-up (Sampling_InstanceIndependence_*): sums the draw (the red channel
 * of Cl) of the light with id 2 only, the id being the green channel of Cl (see
 * sampling_instance_light.sl). Ci.r is therefore the draw of that one instance at this point,
 * whatever other instances of the light shader are in the scene. No `if`-gated uniform
 * assignment (GitHub #8): the selection is arithmetic.
 */
surface sampling_instance_probe()
{
    normal Nf = faceforward(normalize(N), I);
    float r2 = 0;
    illuminance(P, Nf, PI) {
        r2 += comp(Cl, 0) * step(1.5, comp(Cl, 1)) * (1 - step(2.5, comp(Cl, 1)));
    }
    Ci = color(r2, 0, 0);
    Oi = 1;
}
