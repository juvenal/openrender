/* GitHub #153: a shadow/transmission ray that passes through a transparent hit
 * is continued to its original end point (the target point, or "maxdist"),
 * never to infinity. The probe shoots one transmission ray from P toward
 * (tx, P.y, P.z + dz), limited to maxdist and biased by bias, and encodes the
 * transmission it returns in Ci. The scenes put a half-opaque sheet on the
 * path and an opaque wall beyond the end point: the right answer is the
 * sheet's 0.5, and a continued ray that lost its end point reports the wall
 * (0). The shader is deliberately minimal and uses a "transmission" query only.
 */
surface transmission_destination_probe(float tx = 3; float dz = -0.5;
                                       float maxd = 1e30; float bias = 0.01)
{
    point Q = point(tx, ycomp(P), zcomp(P) + dz);
    Ci = transmission(P, Q, "maxdist", maxd, "bias", bias);
    Oi = 1;
}
