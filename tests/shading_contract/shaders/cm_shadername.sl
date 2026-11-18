/* Spec 033 characterisation (ShadingContract_MessageRequests). shadername() with no argument and with the slot names
   of the shaders bound to the object, run against the producers of cm_producer_*.sl. Ci = (own name matched, surface
   producer matched, displacement producer matched); Oi.x = atmosphere producer matched. `if` into varying flags, as
   shadername_probe.sl does (GitHub #6 and #8). */
surface cm_shadername()
{
    uniform string selfName = shadername();
    uniform string surfName = shadername("surface");
    uniform string dispName = shadername("displacement");
    uniform string atmName = shadername("atmosphere");
    varying float a = 0;
    varying float b = 0;
    varying float c = 0;
    varying float d = 0;
    if (selfName == "cm_shadername")
        a = 1;
    if (surfName == "cm_producer_surface")
        b = 1;
    if (dispName == "cm_producer_displacement")
        c = 1;
    if (atmName == "cm_producer_volume")
        d = 1;
    Ci = color(a, b, c);
    Oi = color(d, 0, 0);
}
