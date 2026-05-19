#!/usr/bin/env python3
import sys
import os

# Add src/python to sys.path to find prman.py
script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.abspath(os.path.join(script_dir, "..", "..", ".."))
sys.path.append(os.path.join(project_root, "src", "python"))

from prman import Ri

NFRAMES = 100
NSPHERES = 4
FRAMEROT = 15.0
XRES = 1280
YRES = 720

def color_spheres(ri, n, s):
    if n <= 0:
        return

    ri.AttributeBegin()

    ri.Translate(-0.5, -0.5, -0.5)
    ri.Scale(1.0/n, 1.0/n, 1.0/n)

    for x in range(n):
        for y in range(n):
            for z in range(n):
                color = [
                    float(x + 1) / n,
                    float(y + 1) / n,
                    float(z + 1) / n
                ]

                ri.Color(color)
                ri.TransformBegin()
                ri.Translate(x + 0.5, y + 0.5, z + 0.5)
                ri.Scale(s, s, s)
                ri.Sphere(0.5, -0.5, 0.5, 360.0)
                ri.TransformEnd()

    ri.AttributeEnd()

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} ribFile.rib|#|orender")
        sys.exit(2)

    renderer = sys.argv[1]
    ri = Ri()
    ri.Begin(renderer)

    for frame in range(NFRAMES + 1):
        filename = f"colorSpheres.{frame:03d}.tif"
        ri.FrameBegin(frame)

        ri.Projection(ri.PERSPECTIVE)
        ri.Translate(0.0, 0.0, 1.5)
        ri.Rotate(40.0, -1.0, 1.0, 0.0)

        ri.Display(filename, ri.FRAMEBUFFER, ri.RGBA)
        ri.Format(XRES, YRES, -1.0)
        ri.ShadingRate(1.0)

        ri.WorldBegin()

        ri.LightSource(ri.DISTANTLIGHT)

        ri.Sides(1)

        scale = float(NFRAMES - (frame - 1)) / float(NFRAMES)
        
        ri.Rotate(FRAMEROT * frame, 0.0, 0.0, 1.0)
        ri.Surface(ri.PLASTIC)
        color_spheres(ri, NSPHERES, scale)
        
        ri.WorldEnd()
        ri.FrameEnd()

    ri.End()

if __name__ == "__main__":
    main()
