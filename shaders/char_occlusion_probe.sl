/* Spec 027 (T030a): occlusion() with an environment map and, optionally, the point hierarchy, for the
 * characterization scenes of specs/027-texture-system/tools/scenes. "environmentmap" makes the hider's
 * lookupEnvironment() read the map for rays that escape (occlusionParams.environment);
 * "pointbased" 1 with a "filename" and a "maxsolidangle" makes CPointHierarchy::lookup read the limit
 * (occlusionParams.maxSolidAngle). The scene decides which; the defaults are the plain call.
 */
surface char_occlusion_probe(uniform string envmap = "";
                             uniform string filename = "";
                             uniform float pointbased = 0;
                             uniform float maxsolidangle = 0.05;
                             uniform float samples = 16)
{
    vector Nf = faceforward(normalize(N), I);
    float occ = occlusion(P, Nf, samples, "environmentmap", envmap, "pointbased", pointbased,
                          "filename", filename, "maxsolidangle", maxsolidangle);
    Ci = color(occ, occ, occ);
    Oi = 1;
}
