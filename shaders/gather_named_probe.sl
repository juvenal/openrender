surface gather_named_probe()
{
    normal Nn = normalize(N);
    color Csum = 0;
    float samples = 8;
    point rOrigin = 0;
    vector rDir = 0;
    float rLen = 0;
    color Ctotal = 0;

    gather("illuminance", P, Nn, PI * 0.5, samples,
           "bias", 0.01,
           "maxdist", 1000.0,
           "samplebase", 2,
           "distribution", "cosine",
           "label", "myGather",
           "surface:Ci", Csum,
           "ray:origin", rOrigin,
           "ray:direction", rDir,
           "ray:length", rLen)
    {
        Ctotal += Csum;
    }
    else
    {
        Ctotal += 0;
    }

    Ci = 0.05 + Ctotal / samples;
    Oi = 1;
}
