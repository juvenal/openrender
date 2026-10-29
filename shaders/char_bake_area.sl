/* Spec 027 (T030a): the bake pass of the point-hierarchy characterization scene (the recipe of
 * doc/Documentation/Point_based_GI.html): bakes the micropolygon areas into a point cloud's "_area"
 * channel with bake3d(). Pass 2 is char_occlusion_probe with "pointbased" 1.
 */
surface char_bake_area(uniform string filename = "")
{
    normal Nn = normalize(N);
    float a = area(P, "dicing");
    float ok = bake3d(filename, "_area", P, Nn, "interpolate", 1, "_area", a);
    Ci = Cs * Os * abs(normalize(I) . Nn);
    Oi = Os;
}
