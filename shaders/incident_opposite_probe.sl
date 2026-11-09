/**
 * incident_opposite_probe(): a surface that reports incident() and opposite() (spec 056)
 *
 * Ci = (incident("testVolParam"), opposite("testVolParam"), 0): the value of the volume parameter on the side the ray
 * comes from, and on the other side. A -1 stands for "not found". Bind param_query_volume as Interior and as Exterior
 * with different testVolParam values.
 *
 * Used by the volume-incident-opposite scene (examples/rib/tests).
 */
surface incident_opposite_probe()
{
    float f = -1;
    float g = -1;
    incident("testVolParam", f);
    opposite("testVolParam", g);
    Ci = color(f, g, 0);
    Oi = 1;
}
