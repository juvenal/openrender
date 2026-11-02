/* Spec 026 contract-test shader (tests/shading_contract). Run with no current object, as the imager does (FR-027): attribute() has nothing to read and answers 0 without crashing, while option() ("MaxRayDepth") and rendererinfo() ("renderer") still answer 1. Ci = (the three results), Oi = (attribute's destination untouched, option's value, renderer string set). */
surface cm_nullobject()
{
    float fa = -7;
    float fo = -1;
    string s = "unset";
    float ra = attribute("ShadingRate", fa);
    float ro = option("MaxRayDepth", fo);
    float rr = rendererinfo("renderer", s);

    Ci = color(ra, ro, rr);
    Oi = color((fa == -7) ? 1 : 0, fo, (s == "unset") ? 0 : 1);
}
