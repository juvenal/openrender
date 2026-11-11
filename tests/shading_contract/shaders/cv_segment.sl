/* Spec 056 contract-test shader (tests/shading_contract). A volume shader that returns the segment vector I as its colour
 * and the segment length as its opacity, so a test reads the segment the step ran on from what trace() returns. */
volume cv_segment()
{
    Ci = color(xcomp(I), ycomp(I), zcomp(I));
    Oi = color(length(I));
}
