/* GitHub #115 contract-test shader (tests/shading_contract). lightsource() of a float, a vector, a string and a matrix parameter of every light of an illuminance loop, summed over the lights.
 * Ci = (sum of k, sum of xcomp(v), sum of comp(m, 0, 0)); Oi = (a code for the strings: "one" 1, "two" 10, the number of calls that returned 1, 0). */
surface cm_ls_all()
{
    float k = -1;
    vector v = vector(-1, -2, -3);
    string s = "unset";
    matrix m = 0;
    float ksum = 0, vsum = 0, msum = 0, ssum = 0, found = 0;
    illuminance(P, N, PI / 2) {
        found += lightsource("k", k);
        found += lightsource("v", v);
        found += lightsource("s", s);
        found += lightsource("m", m);
        ksum += k;
        vsum += xcomp(v);
        msum += comp(m, 0, 0);
        if (s == "one")
            ssum += 1;
        else if (s == "two")
            ssum += 10;
    }
    Ci = color(ksum, vsum, msum);
    Oi = color(ssum, found, 0);
}
