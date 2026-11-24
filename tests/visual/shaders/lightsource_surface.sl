/* GitHub #115: Ci is the sum of k over the lights of the illuminance loop, each read with lightsource() from the light being
 * examined (RISpec 3.2, section 15.8), so the centre pixel tells which light each iteration read. */
surface lightsource_surface()
{
    float sum = 0;
    float kk = 0;
    illuminance(P) { lightsource("k", kk); sum += kk; }
    Ci = color(sum, 0, 0);
    Oi = 1;
}
