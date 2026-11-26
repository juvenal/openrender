/* GitHub #229: the point form rotate(Q, angle, P1, P2) rotates Q by angle radians about the axis through P1 and P2
 * (RISpec 3.2) in both backends, for the RotatePoint_* tests (tests/visual/atan2-scenes.cmake). The values come from
 * parameters, so the compiler cannot fold them; they are floats, because a point parameter is carried into the shader's
 * coordinate system and the scene is translated. Ci is the rotated point.
 */
surface rotatep_probe(float qx = 1, qy = 0, qz = 0; float angle = 1.5707964;
                      float p1x = 0, p1y = 0, p1z = 0; float p2x = 0, p2y = 0, p2z = 1)
{
    point r = rotate(point(qx, qy, qz), angle, point(p1x, p1y, p1z), point(p2x, p2y, p2z));
    Ci = color(xcomp(r), ycomp(r), zcomp(r));
    Oi = 1;
}
