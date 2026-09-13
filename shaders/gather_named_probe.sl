surface gather_named_probe()
{
    normal Nn = normalize(N);
    color Csum = 0;
    /* 64, not 8. This shader exists only to drive the sphere-gather visual
       tests, and gather() ray noise sets their floor. Measured block-avg-diff
       between two independent renders of the same scene (threshold is 20):
           samples=8    8.72 - 12.36      samples=32   5.05 - 6.25
           samples=64   3.88 - 5.34       (~1/sqrt(N), as expected)
       At 8 the test was mostly measuring its own noise and would have flaked
       eventually; the reference image was regenerated at 64 so both sides of
       the comparison are converged. Do not lower this without re-rendering
       examples/rib/tests/references/sphere-gather-reyes.tif to match -- the
       -slo variant is compared against that same reference. Costs ~1s. */
    float samples = 64;
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
