/* Ray-origin self-hit probe (fix/ray-origin-self-hit).
 *
 * Put on a lone convex surface (a sphere, nothing else in the scene), every
 * ray leaving the surface into the hemisphere around its outward normal must
 * miss: a convex surface can not see itself. Any hit is a self-hit, the ray
 * having started inside the surface.
 *
 * gather() and occlusion() jitter each ray's origin across the shading
 * point's footprint along dPdu/dPdv. At a reyes grid's edge those derivatives
 * are one-sided secants, which dip under a convex surface, so unguarded
 * origins land just inside it and grazing rays re-hit it past the bias.
 * occlusion() also built its hemisphere on dPdu, which tilted its grazing
 * directions into the surface.
 *
 * Each channel counts one built-in's hits, so that a single one saturates
 * the pixel's sample:
 *   red   gather()     self-hits (the hit point is shaded at depth 1: Ci = 1)
 *   green occlusion()  occluded rays
 * The correct image is black.
 *
 * trace() is left out: at grazing incidence the mirror direction, jittered
 * across the ray cone, can point into the sphere, and those are genuine hits.
 */
surface ray_self_hit_probe(float samples = 16)
{
    uniform float depth = 0;
    rayinfo("depth", depth);
    Oi = 1;
    if (depth > 0) {
        Ci = 1;
    } else {
        normal Nn = normalize(N);
        color Chit = 0;
        float hits = 0;
        gather("illuminance", P, Nn, PI * 0.5, samples, "surface:Ci", Chit) {
            hits += comp(Chit, 0);
        }
        float occ = occlusion(P, Nn, samples);
        Ci = color(hits, occ * samples, 0);
    }
}
