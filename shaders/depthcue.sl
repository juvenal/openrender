/**
 * depthcue(): darken objects according to their depth. The mindistance
 *             and maxdistance cover the range between clipping planes.
 *
 * From the "The RenderMan Companion"
 */

volume
depthcue (float mindistance = 0,
                maxdistance = 1;
          color background = 0) {

    float d = clamp((depth(P) - mindistance) / (maxdistance - mindistance), 0, 1);
    Ci = mix(Ci, background, d);
}
