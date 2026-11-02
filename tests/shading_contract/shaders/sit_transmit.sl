/* Spec 026 Situations shader (tests/shading_contract). transmission() toward an occluder above: the shadow-type hits of row 5. */
surface sit_transmit()
{
    point q = P + vector(0, 3, 0);
    Ci = transmission(P, q);
    Oi = 1;
}
