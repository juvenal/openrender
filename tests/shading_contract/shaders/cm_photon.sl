/* Spec 029 contract-test shader (tests/shading_contract). photonmap(): the lookup goes through the services' photon map. */
surface cm_photon()
{
    Ci = photonmap("m", P, normalize(N), "estimator", 50);
    Oi = 1;
}
