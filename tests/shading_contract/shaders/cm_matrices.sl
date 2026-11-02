/* Spec 026 contract-test shader (tests/shading_contract). One transform per run, chosen by the uniform parameter `which`: every named system, a user system, object, shader, light, a colour space, the two-space and the matrix forms, and a name that does not exist. (texture3d's coordinate system is asserted in the rendered program.) */
surface cm_matrices(uniform float which = 0)
{
    point p = point(0.25, 0.5, 0.75);
    vector v = vector(0.5, -0.25, 1);
    normal n = normal(0, 1, 0);
    matrix M = 1;
    point r = point(0, 0, 0);
    color c = color(0.2, 0.4, 0.6);
    color rc = color(0, 0, 0);

    if (which == 0) r = transform("world", p);
    else if (which == 1) r = transform("camera", p);
    else if (which == 2) r = transform("current", p);
    else if (which == 3) r = transform("cmuser", p);
    else if (which == 4) r = transform("object", p);
    else if (which == 5) r = transform("shader", p);
    else if (which == 6) r = transform("light", p);
    else if (which == 7) r = transform("NDC", p);
    else if (which == 8) rc = ctransform("rgb", "hsv", c);
    else if (which == 9) r = transform("world", "cmuser", p);
    else if (which == 10) r = vtransform("world", v);
    else if (which == 11) r = vtransform("cmuser", "world", v);
    else if (which == 12) r = ntransform("world", n);
    else if (which == 13) r = transform(M, p);
    else if (which == 14) r = transform("world", M, p);
    else if (which == 15) r = transform("cmunknown", p);

    Ci = color(xcomp(r), ycomp(r), zcomp(r)) + rc;
    Oi = 1;
}
