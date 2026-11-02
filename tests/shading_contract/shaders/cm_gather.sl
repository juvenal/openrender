/* Spec 026 contract-test shader (tests/shading_contract). gather(). */
surface cm_gather()
{
    color Cs = 0;
    color Ct = 0;
    gather("illuminance", P, normalize(N), PI * 0.5, 4, "bias", 0.02, "label", "cmgather", "surface:Ci", Cs) {
        Ct += Cs;
    }
    Ci = Ct * 0.25;
    Oi = 1;
}
