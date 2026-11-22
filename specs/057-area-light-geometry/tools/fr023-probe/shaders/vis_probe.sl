light vis_probe(point from = point "shader" (0, 4, 0); float intensity = 1) {
    illuminate(from) {
        Cl = intensity * visibility(Ps, from);
    }
}
