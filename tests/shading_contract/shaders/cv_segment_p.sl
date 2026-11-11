/* Spec 056 contract-test shader (tests/shading_contract). Like cv_segment, but the colour is the segment's endpoint P. */
volume cv_segment_p()
{
    Ci = color(xcomp(P), ycomp(P), zcomp(P));
    Oi = color(length(I));
}
