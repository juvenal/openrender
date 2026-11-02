/* Spec 026 contract-test shader (tests/shading_contract). transmission() (a bundle). */
surface cm_transmission()
{
    point q = P + vector(0.3, 0.4, -0.5);
    Ci = transmission(P, q);
    Oi = 1;
}
