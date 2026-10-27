/* Spec 027 (T030a): indirectdiffuse() with an environment map, for the characterization scenes of
 * specs/027-texture-system/tools/scenes. "environmentmap" makes the hider's lookupEnvironment() read the
 * map for rays that escape.
 */
surface char_indirectdiffuse_probe(uniform string envmap = "";
                                   uniform float samples = 16)
{
    vector Nf = faceforward(normalize(N), I);
    Ci = indirectdiffuse(P, Nf, samples, "environmentmap", envmap);
    Oi = 1;
}
