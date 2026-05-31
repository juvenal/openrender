/**
 *  windowhighlight(): Give a surface a window-shaped specular highlight.
 *
 * From the "The RenderMan Companion"
 */

surface
windowhighlight (point center = point "world" (0, 0, -4),  /* center of the window */
                       in = point "world" (0, 0, 1),       /* normal to the wall */
                       up = point "world" (0, 1, 0);       /* 'up' on the wall */
                 color specularcolor = 1;
                 float Ka = 0.3,
                       Kd = 0.5,
                       xorder = 2,                         /* number of panes horizontally */
                       yorder = 3,                         /* number of panes vertically   */
                       panewidth = 6,                      /* horizontal size of a pane    */
                       paneheight = 6,                     /* vertical size of a pane      */
                       framewidth = 1,                     /* sash width between panes     */
                       fuzz = 0.2)                         /* transition region between pane and sash */
{
    uniform
        point in2,           /* normalized in */
              right,         /* unit vector perpendicular to in2 and up2     */
              up2,           /* normalized up perpendicular to in            */
              corner;        /* location of lower left corner of window      */
    point path,              /* incident vector I reflected about normal N   */
          PtoC,              /* vector from surface point to window corner   */
          PtoF;              /* vector from surface point to wall along path */
    float offset, modulus, yfract, xfract;

    point Nf = faceforward(normalize(N), I);

    /* Set up uniform variables as described above */
    in2 = normalize(in);
    right = up ^ in2;
    up2 = normalize(in2 ^ right);
    right = up2 ^ in2;
    corner = center - right * xorder * panewidth / 2 -
             up2 * yorder * paneheight / 2;

    /* trace source of highlight */
    path = reflect(I, normalize(Nf));
    PtoC = corner - P;

    /* outside the room */
    if (path.PtoC <= 0) {
        xfract = yfract = 0;
    }
    else {
        /*
         *  Make PtoF be a vector from the surface point to the wall
         *	by adjusting the length of the reflected vector path.
         */
        PtoF = path * (PtoC.in2) / (path.in2);
        /*
         *  Calculate the vector from the corner to the intersection point, and
         *  project it onto up2. This length is the vertical offset of the
         *  intersection point within the window.
         */
        offset = (PtoF - PtoC).up2;
        modulus = mod(offset, paneheight);

        /* inside the window */
        if (offset > 0 && offset / paneheight < yorder ) {
            /* symmetry about pane center */
            if (modulus > (paneheight / 2)) {
                modulus = paneheight - modulus;
            }
            /* fuzz at the edge of a pane */
            yfract = smoothstep((framewidth / 2) - (fuzz / 2),
                                (framewidth / 2) + (fuzz / 2),
                                modulus);
        }
        else {
            yfract = 0;
        }

        /* Repeat the process for horizontal offset */
        offset = (PtoF - PtoC).right;
        modulus = mod(offset, panewidth);
        if (offset > 0 && offset / panewidth < xorder ) {
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
    /* specular calculation using the highlight */
    Ci = Cs * (Kd * diffuse(Nf) + Ka * ambient()) +
         yfract * xfract * specularcolor;
}
