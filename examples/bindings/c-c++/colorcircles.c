/**
 *
 *
 *
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ri.h>

#define NFRAMES 100
#define NSPHERES  4
#define FRAMEROT 15.0f
#define XRES 1280
#define YRES 720


void ColorSpheres(int n, float s) {
    int x, y, z;
    RtColor color;

    if(n <= 0) {
        return;
    }

    RiAttributeBegin();

    RiTranslate(-0.5f, -0.5f, -0.5f );
    RiScale(1.0f/n, 1.0f/n, 1.0f/n);

    for (x = 0; x < n; x++) {
        for (y = 0; y < n; y++) {
            for (z = 0; z < n; z++) {
                color[0] = ((float) x+1) / ((float) n);
                color[1] = ((float) y+1) / ((float) n);
                color[2] = ((float) z+1) / ((float) n);

                RiColor(color);
                RiTransformBegin();
                RiTranslate(x+.5f, y+.5f, z+.5f);
                RiScale(s, s, s);
                RiSphere(0.5f, -0.5f, 0.5f, 360.0f, RI_NULL);
                RiTransformEnd();
            }
        }
    }

    RiAttributeEnd();

    return;
}



int main(int argc, char *argv[]) {
    RtInt  frame;
    float  scale;
    char   filename[64];
    char*  renderer = RI_NULL;

    if (argc != 2) {
        fprintf(stderr,
            "Usage: %s ribFile.rib|#|orender\n\n",
            argv[0]);
        exit (2);
    }

    renderer = argv[1];

    /* if the variable renderer is "#" or "orender", RiBegin() will
    drive the Ri Calls directly into openRender's rendering internals.
    Since RiDisplay is set to put its output to the
    framebuffer, that renderer will attempt to open the framebuffer
    and render directly to it.  If the variable renderer is set to
    some other value, RiBegin() will open that as a file and put the
    RIB commands in it.
    */

    RiBegin(renderer);

    for (frame = 0; frame <= NFRAMES; frame++) {
        sprintf(filename, "colorSpheres.%03d.tif", frame );
        RiFrameBegin(frame);

        RiProjection("perspective", RI_NULL);
        RiTranslate(0.0f, 0.0f, 1.5f);
        RiRotate(40.0f, -1.0f, 1.0f, 0.0f);

        RiDisplay(filename, RI_FRAMEBUFFER, RI_RGBA, RI_NULL);
        RiFormat((RtInt)XRES, (RtInt)YRES, -1.0f);
        RiShadingRate(1.0);

        RiWorldBegin();

        RiLightSource("distantlight", RI_NULL);

        RiSides((RtInt)1);

        scale = (float)(NFRAMES - (frame - 1)) / (float)NFRAMES;
        RiRotate(FRAMEROT*frame, 0.0f, 0.0f, 1.0f);
        RiSurface("plastic", RI_NULL);
        ColorSpheres(NSPHERES, scale);
        RiWorldEnd();
        RiFrameEnd();
    }

    RiEnd();

    return EXIT_SUCCESS;
}
