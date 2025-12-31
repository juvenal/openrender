/**
 * clouds(): a surface shader for a cloudy surface
 *
 * From the "The RenderMan Companion"
 */

surface
clouds (float Kd = 0.8,
              Ka = 0.2) {
    float sum;
    float i, freq;
    color refl;

    sum = 0;
    freq = 4.0;
    for (i = 0; i < 6; i = i + 1) {
        sum = sum + 1 / freq * abs(0.5 - noise(freq * P));
        freq = 2 * freq;
    }
    refl = Cs * sum;
    Ci = refl * (Ka * ambient() + Kd * diffuse(faceforward(normalize(N),I)));
    Oi = 1.0;
}