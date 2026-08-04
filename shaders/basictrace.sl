/**





 */

surface
basictrace() {
    normal Nn = normalize(N);
    vector In = normalize(I);
    float facingRatio = Nn . (-In);
    vector R = reflect(In, Nn);
    Ci = Cs * facingRatio;
    Ci += Cs * trace(P, R);
}