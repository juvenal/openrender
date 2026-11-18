/* Spec 033 (ShadingSystem_BindUnknownSpace, P-Q2): a parameter default written in a coordinate system that does not exist.
   Bound by either backend, the default must come out as the point itself (the identity), with no diagnostic. */
surface cm_nosuch(point p = point "nosuch" (1, 2, 3))
{
    Ci = color(0.5, 0.5, 0.5);
    Oi = 1;
}
