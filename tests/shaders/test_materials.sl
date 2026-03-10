surface test_materials(float Ka = 1; float Kd = 1;) {
    Ci = Kd * diffuse(faceforward(normalize(N), I));
}
