/**
 *
 *
 *
 *
 *
 *
 */

light
windowlight (point from       = point "world" (0, 0, -20),
                   to         = point "world" (0, 0, 0),
                   center     = point "world" (0, 0, -4),
                   in         = point "world" (0, 0, 1),
                   up         = point "world" (0, 1, 0);
             color lightcolor = color (1, 0.9, 0.6),
                   darkcolor  = color (0.05, 0.2, 0.1);
             float intensity  = 1,
                   xorder     = 2,
                   yorder     = 3,
                   panewidth  = 6,
                   paneheight = 6,
                   framewidth = 1,
                   fuzz       = 0.1)

{
    uniform
        point in2,
              right,
              up2,
              corner,
              path;

    point PtoC,
          PtoF;

    float offset,
          modulus,
          yfract,
          xfract;

    point Nf = faceforward(N, I);

    path   = (from - to);
    in2    = faceforward(normalize(in), path);
    right  = up ^ in2;
    up2    = normalize(in2 ^ right);
    right  = up2 ^ in2;
    corner = center - right * xorder * panewidth / 2 -
                      up2 * yorder * paneheight / 2;

    solar (-path, 0.0) {
        PtoC = corner - Ps;
        if (path . PtoC <= 0) {
            xfract = yfract = 1;
        }
        else {
            PtoF = path * (PtoC . in2) / (path . in2);

            offset = (PtoF - PtoC) . up2;
            modulus = mod (offset, paneheight);
            if (offset > 0 && offset / paneheight < yorder) {
                if (modulus > (paneheight / 2)) {
                    modulus = paneheight - modulus;
                }
                yfract = smoothstep((framewidth / 2) - (fuzz / 2),
                                    (framewidth / 2) + (fuzz / 2),
                                    modulus);
            }
            else {
                yfract = 0;
            }

            offset = (PtoF - PtoC) . right;
            modulus = mod(offset, panewidth);
            if (offset > 0 && offset / panewidth < xorder) {
                if (modulus > (panewidth / 2)) {
                    modulus = panewidth - modulus;
                }
                xfract = smoothstep((framewidth / 2) - (fuzz / 2),
                                    (framewidth / 2) + (fuzz / 2),
                                    modulus);
            }
            else {
                xfract = 0;
            }
        }
        Cl = intensity * mix (darkcolor, lightcolor, yfract * xfract);
    }
}
