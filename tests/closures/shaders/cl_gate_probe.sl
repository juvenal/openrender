/*
 * Spec 035, Phase 3 (T055): the gate probe of ClosureMode_Gate_{interp,jit}, run through the delegating cldbl backend (the real interpreter and
 * the real JIT) so the real funnels of the lighting gate are exercised. Each result is the first channel of a builtin's value written into
 * its own `output` variable (the test reads them: cldbl_backend.h, outputs()):
 *
 *   gated under `closures`            r_ambient r_diffuse r_specular r_phong r_illum (illuminance, no category) r_illumcat (with a category)
 *                                     r_lightsource (lightsource() in a loop) r_occlusion r_indirect r_gather (the gather() sample count)
 *   not gated                         r_trace r_transmission r_visibility r_environment ("raytrace") r_shadow ("raytrace") r_texture
 *                                     (when `texname` is given) r_specbrdf
 *
 * Only the illuminance(P, N, angle) forms: GitHub #126 says a .slo surface that uses illuminance(P) alone gathers no light.
 * The test lights carry a float parameter "intensity" and the category "gate".
 */
surface cl_gate_probe(uniform float samples = 4;
                      uniform string texname = "";
                      output varying float r_ambient = 0;
                      output varying float r_diffuse = 0;
                      output varying float r_specular = 0;
                      output varying float r_phong = 0;
                      output varying float r_illum = 0;
                      output varying float r_illumcat = 0;
                      output varying float r_lightsource = 0;
                      output varying float r_occlusion = 0;
                      output varying float r_indirect = 0;
                      output varying float r_gather = 0;
                      output varying float r_trace = 0;
                      output varying float r_transmission = 0;
                      output varying float r_visibility = 0;
                      output varying float r_environment = 0;
                      output varying float r_shadow = 0;
                      output varying float r_texture = 0;
                      output varying float r_specbrdf = 0)
{
    normal Nf = faceforward(normalize(N), I);
    vector V = -normalize(I);
    vector R = reflect(normalize(I), Nf);
    point Pe = P + Nf * 4;

    r_ambient = comp(ambient(), 0);
    r_diffuse = comp(diffuse(Nf), 0);
    r_specular = comp(specular(Nf, V, 0.2), 0);
    r_phong = comp(phong(Nf, V, 20), 0);

    color acc = 0;
    illuminance(P, Nf, PI / 2) {
        acc += Cl;
    }
    r_illum = comp(acc, 0);

    acc = 0;
    illuminance("gate", P, Nf, PI / 2) {
        acc += Cl;
    }
    r_illumcat = comp(acc, 0);

    float found = 0;
    float intensity = 0;
    illuminance(P, Nf, PI / 2) {
        found += lightsource("intensity", intensity);
    }
    r_lightsource = found;

    r_occlusion = occlusion(P, Nf, samples);
    r_indirect = comp(indirectdiffuse(P, Nf, samples), 0);

    float gathered = 0;
    gather("illuminance", P, Nf, PI / 2, samples) {
        gathered += 1;
    }
    else {
        gathered += 0;
    }
    r_gather = gathered;

    r_trace = comp(trace(P, R), 0);
    r_transmission = comp(transmission(P, Pe), 0);
    r_visibility = comp(visibility(P, Pe), 0);
    r_environment = comp(environment("raytrace", R), 0);
    r_shadow = shadow("raytrace", P);
    r_texture = comp(texture(texname, "fill", 0.5), 0);
    r_specbrdf = comp(specularbrdf(V, Nf, V, 0.2), 0);

    Ci = Cs * Os;
    Oi = Os;
}
