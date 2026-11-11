/* Spec 056 contract-test shader (tests/shading_contract). incident() and opposite() of one name, on whichever side the
 * ray comes from: Ci = (incident value, opposite value, 0), Oi = (found flags, 1). */
surface cv_incident_probe()
{
    float f = -1;
    float g = -1;
    float r1 = incident("cvside", f);
    float r2 = opposite("cvside", g);
    Ci = color(f, g, 0);
    Oi = color(r1, r2, 1);
}
