#!/usr/bin/env python3
"""Generate the spec 021 Phase 1 test scenes (T005-T012).

Camera convention (verified against a marker render): for a RIB camera block
  Translate tx ty tz / Rotate ax 1 0 0 / Rotate ay 0 1 0
camera-space p_c = T * Rx(ax) * Ry(ay) * p_w with standard right-handed
rotation matrices, and pixel = (w/2 + s*x/z, h/2 - s*y/z), s = min(w,h)/2/tan(fov/2).
"""
import math, os, sys

OUT = sys.argv[1]  # examples/rib/tests
os.makedirs(os.path.join(OUT, "archives"), exist_ok=True)

def rx(a):
    c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
    return [[1, 0, 0], [0, c, -s], [0, s, c]]

def ry(a):
    c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
    return [[c, 0, s], [0, 1, 0], [-s, 0, c]]

def mul(A, B):
    return [[sum(A[i][k] * B[k][j] for k in range(3)) for j in range(3)] for i in range(3)]

class Cam:
    def __init__(self, fov, w, h, t, ax=0.0, ay=0.0):
        self.fov, self.w, self.h, self.t, self.ax, self.ay = fov, w, h, t, ax, ay
        self.R = mul(rx(ax), ry(ay))
        self.s = 0.5 * min(w, h) / math.tan(math.radians(fov / 2))

    def cam(self, p):
        q = [sum(self.R[i][k] * p[k] for k in range(3)) for i in range(3)]
        return [q[0] + self.t[0], q[1] + self.t[1], q[2] + self.t[2]]

    def px(self, p):
        q = self.cam(p)
        return (self.w / 2 + self.s * q[0] / q[2], self.h / 2 - self.s * q[1] / q[2])

    def row_matrix(self):
        """Object-to-camera matrix, RIB row-vector order (16 numbers)."""
        R = self.R
        m = []
        for i in range(3):          # row i = image of basis vector e_i
            m += [R[0][i], R[1][i], R[2][i], 0.0]
        m += [self.t[0], self.t[1], self.t[2], 1.0]
        return " ".join(f"{v:.6f}" for v in m)

    def rib(self):
        lines = [f'Format {self.w} {self.h} 1', f'Projection "perspective" "fov" [{self.fov:g}]',
                 'Clipping 0.1 1000', f'Translate {self.t[0]:g} {self.t[1]:g} {self.t[2]:g}']
        if self.ax:
            lines.append(f'Rotate {self.ax:g} 1 0 0')
        if self.ay:
            lines.append(f'Rotate {self.ay:g} 0 1 0')
        return "\n".join(lines)

def write(name, text):
    with open(os.path.join(OUT, name), "w") as f:
        f.write(text.rstrip() + "\n")

def header(lines):
    return "##RenderMan RIB-Structure 1.0\nversion 3.03\n" + "".join(f"# {l}\n" if l else "#\n" for l in lines)

HIDERS = {"raytrace": 'Hider "raytrace"', "reyes": 'Hider "reyes"', "zbuffer": 'Hider "zbuffer"'}

# ---------------------------------------------------------------------------
# Hero scene (T005, T010-T012)
# ---------------------------------------------------------------------------
HERO_LIGHT = ('    LightSource "raydistant" 2 "intensity" [2.2]\n'
              '        "lightcolor" [1.0 0.82 0.42]\n'
              '        "from" [7 8 -6] "to" [0 0 0]\n')

def hero_cam(w, h):
    return Cam(38, w, h, (0, 0, 9))

def hero_radii(w, h):
    c = hero_cam(w, h)
    zs = math.sqrt(1.5 ** 2 - 0.65 ** 2)
    hole = c.s * 0.65 / (9 + zs)
    seam = c.s * 0.65 / (9 - zs)
    sil = c.s * 1.5 / math.sqrt(81 - 2.25)
    return hole, seam, sil

def hero_header(w, h, extra=()):
    hole, seam, sil = hero_radii(w, h)
    return [f"Analytic image radii at {w}x{h}, centred on ({w/2:g}, {h/2:g}):",
            f"  clear through-hole disc (far rim)  r = {hole:.1f} px",
            f"  near seam ring                     r = {seam:.1f} px",
            f"  sphere silhouette                  r = {sil:.1f} px",
            "Seams: the circles z = +-sqrt(1.5^2 - 0.65^2) = +-1.352 on the cylinder r = 0.65.",
            "Object-to-camera matrix (RIB row-vector order): " + hero_cam(w, h).row_matrix(),
            *extra]

def hero_solid(indent="    ", sphere_pre="", cyl_pre="", sphere_geom=None, solid_pre=""):
    sphere_geom = sphere_geom or "Sphere 1.50 -1.50 1.50 360"
    i = indent
    s = solid_pre
    s += f'{i}SolidBegin "difference"\n'
    s += f'{i}    SolidBegin "primitive"\n'
    s += "".join(f"{i}        {l}\n" for l in sphere_pre.splitlines() if l)
    s += f"{i}        {sphere_geom}\n"
    s += f'{i}    SolidEnd\n'
    s += f'{i}    SolidBegin "primitive"\n'
    s += "".join(f"{i}        {l}\n" for l in cyl_pre.splitlines() if l)
    s += f'{i}        Cylinder 0.65 -2 2 360\n'
    s += f'{i}    SolidEnd\n'
    s += f'{i}SolidEnd\n'
    return s

def hero_scene(name, hider, w, h, header_lines, options="", world_pre="", solid=None, world_post="",
               pre_world=""):
    body = header(header_lines)
    body += HIDERS[hider] + "\nPixelSamples 4 4\n"
    body += f'Display "{name}.tif" "file" "rgba"\n'
    body += hero_cam(w, h).rib() + "\n"
    body += options
    body += pre_world
    body += "WorldBegin\n" + HERO_LIGHT + world_pre
    body += '    Surface "plastic" "float Ks" [0.88] "float roughness" [0.05]\n'
    body += solid if solid is not None else hero_solid()
    body += world_post + "WorldEnd\n"
    write(name + ".rib", body)

STATS = 'Option "statistics" "endofframe" [2]\n'

# T005
for hider in ("raytrace", "reyes", "zbuffer"):
    for w, h in ((800, 500), (1600, 1000)):
        n = f"csg-hero-{hider}-{w}"
        hero_scene(n, hider, w, h, [f"Spec 021 hero scene (T005): sphere r=1.5 minus the open Cylinder 0.65 -2 2 360,",
                                    "viewed straight down the hole. Each operand has its own colour (gold sphere,",
                                    "blue cylinder), so every seam is a colour step even where the two surfaces",
                                    "shade to the same brightness (measured with --seam r:rgb).", "", *hero_header(w, h)],
                   solid=hero_solid(sphere_pre="Color [1 0.8 0.3]", cyl_pre="Color [0.2 0.35 1]"))

# T010 stats scenes
for w, h in ((800, 500), (1600, 1000)):
    hero_scene(f"csg-hero-stats-{w}-reyes", "reyes", w, h,
               ["Spec 021 (T010): the REYES hero scene with end-of-frame statistics at level 2.", "",
                *hero_header(w, h)], options=STATS)
hero_scene("csg-hero-partial-stats-reyes", "reyes", 800, 500,
           ["Spec 021 (T010): csg-hero-stats-800-reyes.rib with CropWindow 0 0.5 0 1, so only the",
            "left half of the same frame is rendered (same camera and micropolygon size).", "",
            *hero_header(800, 500)], options=STATS + "CropWindow 0 0.5 0 1\n")
hero_scene("csg-hero-single-operand-stats-800-reyes", "reyes", 800, 500,
           ["Spec 021 (T010): the difference block holds only the sphere's primitive block, a",
            "single-operand collapse (passthrough, no region predicate)."],
           options=STATS,
           solid='    SolidBegin "difference"\n        SolidBegin "primitive"\n'
                 '            Sphere 1.50 -1.50 1.50 360\n        SolidEnd\n    SolidEnd\n')
hero_scene("csg-tiny-solid-stats-reyes", "reyes", 800, 500,
           ["Spec 021 (T010): the hero solid scaled by 0.015, so it covers under 4x4 pixels",
            f"(silhouette radius {hero_radii(800, 500)[2] * 0.015:.2f} px)."],
           options=STATS, world_pre="    Scale 0.015 0.015 0.015\n")
hero_scene("csg-hero-instanced-outside-world-reyes", "reyes", 800, 500,
           ["Spec 021 (T010): the hero solid defined in an ObjectBegin placed before WorldBegin,",
            "instanced once inside the world. The CSG counters must still print (the flag is",
            "set through CCSGSolid::instantiate())."],
           options=STATS,
           pre_world='ObjectBegin "hero"\n    Surface "plastic" "float Ks" [0.88] "float roughness" [0.05]\n'
                     + hero_solid() + "ObjectEnd\n",
           solid='    ObjectInstance "hero"\n')
hero_scene("csg-offscreen-stats-raytrace", "raytrace", 800, 500,
           ["Spec 021 (T010): a hero solid placed behind the camera, its sphere operand supplied by a",
            "DelayedReadArchive procedural, plus a plain visible sphere. Nothing may be resolved,",
            "classified, split or expanded for the solid (SC-006)."],
           options=STATS,
           solid='    TransformBegin\n        Translate 0 0 -20\n'
                 + hero_solid(indent="        ", sphere_geom='Procedural "DelayedReadArchive" '
                              '["archives/csg-sphere-operand.rib"] [-1.5 1.5 -1.5 1.5 -1.5 1.5]')
                 + '    TransformEnd\n    Sphere 1 -1 1 360\n')
os.replace(os.path.join(OUT, "csg-offscreen-stats-raytrace.rib"), os.path.join(OUT, "csg-offscreen-stats-raytrace.rib"))
src = open(os.path.join(OUT, "csg-offscreen-stats-raytrace.rib")).read()
write("csg-offscreen-stats-reyes.rib", src.replace('Hider "raytrace"', 'Hider "reyes"')
      .replace('csg-offscreen-stats-raytrace.tif', 'csg-offscreen-stats-reyes.tif'))

hero_scene("csg-tolerance-inert-raytrace", "raytrace", 800, 500,
           ["Spec 021 (T010): the hero scene with the retired tessellation tolerance set three times",
            "on different operands. It must render exactly like csg-hero-raytrace-800.rib and print",
            "exactly one notice (FR-023)."],
           world_pre='    Attribute "solid" "float tessellationtolerance" [0.01]\n',
           solid=hero_solid(sphere_pre='Color [1 0.8 0.3]\nAttribute "solid" "float tessellationtolerance" [0.01]',
                            cyl_pre='Color [0.2 0.35 1]\nAttribute "solid" "float tessellationtolerance" [0.01]'))

# T011 two shaders
for hider in ("raytrace", "reyes"):
    hero_scene(f"csg-hero-two-shaders-{hider}", hider, 800, 500,
               ["Spec 021 (T011): gold plastic sphere operand, blue matte cylinder operand (FR-016).",
                "Hole wall annulus 45.6-61.7 px: blue; outer annulus 62-122 px: gold.", "",
                *hero_header(800, 500)],
               solid=hero_solid(sphere_pre='Color [1 0.8 0.3]\nSurface "plastic"',
                                cyl_pre='Color [0.2 0.35 1]\nSurface "matte"'))

# T012 displaced / translucent
dz = 0.05
for hider in ("raytrace", "reyes"):
    hole, seam, sil = hero_radii(800, 500)
    pxu = hero_cam(800, 500).s / (9 - math.sqrt(1.5 ** 2 - 0.65 ** 2))
    hero_scene(f"csg-hero-displaced-{hider}", hider, 800, 500,
               ["Spec 021 (T012): bump_lowfreq (P = P - N*noise*Km, inward, at most Km) on the sphere.",
                "dented does not qualify: it only rewrites N.",
                f"Projected maximum displacement at the near seam depth: Km x px/unit = {dz} x {pxu:.1f}"
                f" = {dz * pxu:.1f} px.", "", *hero_header(800, 500)],
               solid=hero_solid(sphere_pre='Displacement "bump_lowfreq" "float Km" [0.05]\n'
                                'Attribute "displacementbound" "float sphere" [0.05] '
                                '"string coordinatesystem" ["object"]'))
    hero_scene(f"csg-hero-translucent-{hider}", hider, 800, 500,
               ["Spec 021 (T012): the sphere operand at Opacity 0.5, so the hole walls show through it.",
                "", *hero_header(800, 500)],
               solid=hero_solid(sphere_pre='Opacity [0.5 0.5 0.5]'))

# ---------------------------------------------------------------------------
# T009 procedural scenes and archives
# ---------------------------------------------------------------------------
write("archives/csg-sphere-operand.rib", "# Spec 021 (T009): hero sphere operand, for DelayedReadArchive\n"
      "Sphere 1.50 -1.50 1.50 360")
hero_scene("csg-procedural-operand-raytrace", "raytrace", 800, 500,
           ["Spec 021 (T009): the hero scene with its sphere supplied by a DelayedReadArchive",
            "procedural (FR-024). Must match csg-procedural-operand-inline-raytrace.rib (SC-010).",
            "The archive path is relative: the test copies examples/rib/tests/archives into its",
            "scratch directory."],
           solid=hero_solid(sphere_geom='Procedural "DelayedReadArchive" ["archives/csg-sphere-operand.rib"] '
                                        '[-1.5 1.5 -1.5 1.5 -1.5 1.5]'))
hero_scene("csg-procedural-operand-inline-raytrace", "raytrace", 800, 500,
           ["Spec 021 (T009): inline twin of csg-procedural-operand-raytrace.rib."])

# ---------------------------------------------------------------------------
# Sphere-cube (T006) and box helpers
# ---------------------------------------------------------------------------
UNIT_CUBE = [
    "Patch \"bilinear\" \"P\" [-0.5 -0.5 0.5 -0.5 -0.5 -0.5 0.5 -0.5 0.5 0.5 -0.5 -0.5]",
    "Patch \"bilinear\" \"P\" [-0.5 -0.5 0.5 -0.5 0.5 0.5 -0.5 -0.5 -0.5 -0.5 0.5 -0.5]",
    "Patch \"bilinear\" \"P\" [0.5 0.5 -0.5 -0.5 0.5 -0.5 0.5 0.5 0.5 -0.5 0.5 0.5]",
    "Patch \"bilinear\" \"P\" [0.5 0.5 -0.5 0.5 0.5 0.5 0.5 -0.5 -0.5 0.5 -0.5 0.5]",
    "Patch \"bilinear\" \"P\" [0.5 -0.5 0.5 0.5 0.5 0.5 -0.5 -0.5 0.5 -0.5 0.5 0.5]",
    "Patch \"bilinear\" \"P\" [-0.5 0.5 -0.5 0.5 0.5 -0.5 -0.5 -0.5 -0.5 0.5 -0.5 -0.5]",
]

def box(indent, cx, cy, cz, sx, sy, sz):
    lines = ["TransformBegin", f"    Translate {cx:g} {cy:g} {cz:g}", f"    Scale {sx:g} {sy:g} {sz:g}"]
    lines += ["    " + p for p in UNIT_CUBE] + ["TransformEnd"]
    return "".join(f"{indent}{l}\n" for l in lines)

SC_LIGHTS = ('    LightSource "ambientlight" 1 "intensity" [0.15]\n'
             '    LightSource "distantlight" 2 "intensity" [0.9] "from" [5 5 -5] "to" [0 0 0]\n')

def sc_cam(w, h):
    return Cam(40, w, h, (0, 0, 5), -25, 30)

for hider in ("raytrace", "reyes", "zbuffer"):
    for w, h in ((800, 500), (1600, 1000)):
        c = sc_cam(w, h)
        n = f"csg-sphere-cube-difference-{hider}-{w}"
        r3 = math.sqrt(0.75)
        arcs = []
        for plane, centre, normal in (("y = +0.5", "0,0.5,0", "0,1,0"), ("y = -0.5", "0,-0.5,0", "0,1,0"),
                                      ("z = +0.5", "0,0,0.5", "0,0,1"), ("z = -0.5", "0,0,-0.5", "0,0,1")):
            arcs.append(f"  --circle {centre},{normal},{r3:.6f} --clip-box 0.1,-0.5,-0.5,1.1,0.5,0.5 "
                        f"--facing 0,0,0     ({plane})")
        body = header([
            "Spec 021 (T006): csg-sphere-cube-difference geometry and camera at a seam-measurable",
            f"resolution ({w}x{h}).",
            "",
            "--projected-curve data, in the solid's object space:",
            f"  --camera 40 {w} {h} {c.row_matrix()}",
            *arcs,
            "The arcs meet at x = sqrt(0.5). The cube's x = 0.1 face lies entirely inside the sphere and",
            "the x = 1.1 plane misses it, so neither carries a seam.",
            "The cube operand is orange, the sphere lavender: every seam is a colour step even where",
            "the two surfaces shade to the same brightness (measured with --edge-mode rgb).",
        ])
        body += HIDERS[hider] + "\nPixelSamples 4 4\n" + f'Display "{n}.tif" "file" "rgba"\n' + c.rib() + "\n"
        body += "WorldBegin\n" + SC_LIGHTS + '    Color [0.7 0.7 0.8]\n    Surface "matte"\n'
        body += '    SolidBegin "difference"\n        SolidBegin "primitive"\n            Sphere 1 -1 1 360\n'
        body += '        SolidEnd\n        SolidBegin "primitive"\n            Color [0.9 0.5 0.2]\n'
        body += box("            ", 0.6, 0, 0, 1, 1, 1)
        body += '        SolidEnd\n    SolidEnd\nWorldEnd\n'
        write(n + ".rib", body)

# ---------------------------------------------------------------------------
# T007 coincident faces
# ---------------------------------------------------------------------------
def coincident(n, hider, solid, expected=False):
    c = Cam(40, 400, 300, (0, 0, 5), -25, 30)
    body = header([
        "Spec 021 (T007): coincident operand faces (FR-022).",
        "union: boxes [-1,0]x[-0.5,0.5]^2 and [0,1]x[-0.5,0.5]^2 share the face x = 0;",
        "  expected result: one box [-1,1]x[-0.5,0.5]^2.",
        "difference: box [-0.5,0.5]^3 minus [0.2,0.5]x[-0.7,0.7]^2, flush with the face x = 0.5;",
        "  expected result: the box [-0.5,0.2]x[-0.5,0.5]^2.",
        "Compare against csg-coincident-{union,difference}-expected-raytrace.rib.",
    ])
    body += HIDERS[hider] + "\nPixelSamples 4 4\n" + f'Display "{n}.tif" "file" "rgba"\n' + c.rib() + "\n"
    body += "WorldBegin\n" + SC_LIGHTS + '    Color [0.7 0.7 0.8]\n    Surface "matte"\n' + solid + "WorldEnd\n"
    write(n + ".rib", body)

UNION = ('    SolidBegin "union"\n        SolidBegin "primitive"\n' + box("            ", -0.5, 0, 0, 1, 1, 1)
         + '        SolidEnd\n        SolidBegin "primitive"\n' + box("            ", 0.5, 0, 0, 1, 1, 1)
         + '        SolidEnd\n    SolidEnd\n')
DIFF = ('    SolidBegin "difference"\n        SolidBegin "primitive"\n' + box("            ", 0, 0, 0, 1, 1, 1)
        + '        SolidEnd\n        SolidBegin "primitive"\n' + box("            ", 0.35, 0, 0, 0.3, 1.4, 1.4)
        + '        SolidEnd\n    SolidEnd\n')
for hider in ("raytrace", "reyes", "zbuffer"):
    coincident(f"csg-coincident-union-{hider}", hider, UNION)
    coincident(f"csg-coincident-difference-{hider}", hider, DIFF)
coincident("csg-coincident-union-expected-raytrace", "raytrace", box("    ", 0, 0, 0, 2, 1, 1))
coincident("csg-coincident-difference-expected-raytrace", "raytrace", box("    ", -0.15, 0, 0, 0.7, 1, 1))

# ---------------------------------------------------------------------------
# T008 implied caps; T009 open-cylinder procedural
# ---------------------------------------------------------------------------
CAPS_CAM = Cam(40, 600, 450, (0, 0, 4.2), 35, 25)
CYL = "Cylinder 1.2 -0.5 0.5 180"

def caps_regions(c):
    """Image discs inside each visible cap: the z = -0.5 half-disc and the y = 0 face."""
    rz = math.sqrt(1 - 0.25)
    capz = [(rz * math.cos(t), rz * math.sin(t), -0.5) for t in [math.pi * i / 64 for i in range(65)]]
    flat = []  # boundary of the y = 0 face: x^2 + z^2 <= 1, |z| <= 0.5
    for i in range(65):
        z = -0.5 + i / 64
        flat.append((math.sqrt(1 - z * z), 0, z))
    for i in range(65):
        z = 0.5 - i / 64
        flat.append((-math.sqrt(1 - z * z), 0, z))
    out = []
    for name, poly, centroid in (("end cap z = -0.5", capz, (0, 4 * rz / (3 * math.pi), -0.5)),
                                 ("flat face y = 0", flat, (0, 0, 0))):
        pts = [c.px(p) for p in poly]
        cx, cy = c.px(centroid)
        def segdist(a, b):
            ax, ay = a; bx, by = b
            dx, dy = bx - ax, by - ay
            t = max(0, min(1, ((cx - ax) * dx + (cy - ay) * dy) / (dx * dx + dy * dy or 1)))
            return math.hypot(cx - ax - t * dx, cy - ay - t * dy)
        dmin = min(segdist(pts[i], pts[(i + 1) % len(pts)]) for i in range(len(pts)))
        out.append((name, cx, cy, 0.6 * dmin))
    return out

def caps_scene(n, hider, cyl_geom, lines):
    c = CAPS_CAM
    regions = caps_regions(c)
    body = header(lines + [
        "",
        "Visible cap regions (image discs well inside each cap, for --region-chroma):",
        *[f"  {name}: disc:{x:.1f},{y:.1f},{r:.1f}" for name, x, y, r in regions],
        "A cap carries the cylinder operand's colour (blue); a missing cap would show the gold inside",
        "of the sphere band instead.",
    ])
    body += HIDERS[hider] + "\nPixelSamples 4 4\n" + f'Display "{n}.tif" "file" "rgba"\n' + c.rib() + "\n"
    body += "WorldBegin\n" + SC_LIGHTS + '    Surface "matte"\n    Sides 2\n'
    body += '    SolidBegin "intersection"\n        SolidBegin "primitive"\n            Color [1 0.8 0.3]\n'
    body += '            Sphere 1 -1 1 360\n        SolidEnd\n        SolidBegin "primitive"\n'
    body += '            Color [0.2 0.35 1]\n' + f"            {cyl_geom}\n"
    body += '        SolidEnd\n    SolidEnd\nWorldEnd\n'
    write(n + ".rib", body)

CAPS_LINES = ["Spec 021 (T008): sphere r=1 intersected with the open half-sweep Cylinder 1.2 -0.5 0.5 180,",
              "a single open quadric operand (FR-021). The result is the band |z| <= 0.5, y >= 0 of the",
              "sphere, closed by the implied caps: the end caps z = +-0.5 and the flat sweep-gap face",
              "y = 0. The flat face and the z = -0.5 end cap face the camera."]
for hider in ("raytrace", "reyes", "zbuffer"):
    caps_scene(f"csg-open-quadric-caps-{hider}", hider, CYL, CAPS_LINES)
write("archives/csg-open-cylinder-operand.rib",
      "# Spec 021 (T009): the T008 open half-sweep cylinder operand, for DelayedReadArchive\n" + CYL)
caps_scene("csg-procedural-open-cylinder-raytrace", "raytrace",
           'Procedural "DelayedReadArchive" ["archives/csg-open-cylinder-operand.rib"] '
           '[-1.2 1.2 -1.2 1.2 -0.5 0.5]',
           ["Spec 021 (T009): the T008 implied-cap scene with the half-sweep cylinder supplied by a",
            "DelayedReadArchive procedural. Its caps are visible, so the comparison with",
            "csg-procedural-open-cylinder-inline-raytrace.rib observes them (SC-010)."])
caps_scene("csg-procedural-open-cylinder-inline-raytrace", "raytrace", CYL,
           ["Spec 021 (T009): inline twin of csg-procedural-open-cylinder-raytrace.rib (identical to",
            "csg-open-quadric-caps-raytrace.rib apart from the output name)."])

# ---------------------------------------------------------------------------
# T010 free-form and mesh operand scenes
# ---------------------------------------------------------------------------
FF_CAM_LINES = 'Translate 0 0 5\nRotate -25 1 0 0\nRotate 30 0 1 0'

def ff_scene(n, hider, w, h, header_lines, world, cam=None):
    c = cam or Cam(40, w, h, (0, 0, 5), -25, 30)
    body = header(header_lines)
    body += HIDERS[hider] + "\nPixelSamples 4 4\n" + f'Display "{n}.tif" "file" "rgba"\n' + c.rib() + "\n"
    body += ("WorldBegin\n" + SC_LIGHTS + '    Color [0.7 0.7 0.8]\n    Surface "plastic"\n    Sides 2\n'
             + world + "WorldEnd\n")
    write(n + ".rib", body)

def rational_nurbs_sphere(indent, r):
    s2 = math.sqrt(2) / 2
    cu = [(1, 0, 1), (1, 1, s2), (0, 1, 1), (-1, 1, s2), (-1, 0, 1), (-1, -1, s2), (0, -1, 1), (1, -1, s2), (1, 0, 1)]
    cv = [(0, -r, 1), (r, -r, s2), (r, 0, 1), (r, r, s2), (0, r, 1)]
    pts = []
    for rho, z, wv in cv:
        for x, y, wu in cu:
            w = wu * wv
            pts.append(f"{rho * x * w:.6f} {rho * y * w:.6f} {z * w:.6f} {w:.6f}")
    lines = ["NuPatch 9 3 [0 0 0 0.25 0.25 0.5 0.5 0.75 0.75 1 1 1] 0 1",
             "        5 3 [0 0 0 0.5 0.5 1 1 1] 0 1",
             '        "Pw" ['] + ["            " + p for p in pts] + ["        ]"]
    return "".join(f"{indent}{l}\n" for l in lines)

NC = (0.45, 0.0, -0.35)
d = math.sqrt(sum(v * v for v in NC))
a = (d * d + 0.7 ** 2 - 0.5 ** 2) / (2 * d)
cr = math.sqrt(0.7 ** 2 - a * a)
nrm = [v / d for v in NC]
cc = [v * a for v in nrm]
for hider in ("raytrace", "reyes"):
    c = Cam(40, 400, 400, (0, 0, 5), -25, 30)
    ff_scene(f"csg-nurbs-rational-sphere-union-{hider}", hider, 400, 400, [
        "Spec 021 (T010): Sphere 0.7 unioned with a closed rational quadratic NURBS sphere of radius",
        "0.5 centred at (0.45, 0, -0.35). The NURBS sphere represents a sphere exactly (FR-007a).",
        "",
        "--projected-curve data (object space):",
        f"  --camera 40 400 400 {c.row_matrix()}",
        f"  --circle {cc[0]:.6f},{cc[1]:.6f},{cc[2]:.6f},{nrm[0]:.6f},{nrm[1]:.6f},{nrm[2]:.6f},{cr:.6f}"
        f" --facing 0,0,0 --facing {NC[0]},{NC[1]},{NC[2]}",
        "Untouched NURBS area for --include: mask:examples/rib/tests/masks/csg-nurbs-rational-untouched.png.",
    ], '    SolidBegin "union"\n        SolidBegin "primitive"\n            Sphere 0.7 -0.7 0.7 360\n'
       '        SolidEnd\n        SolidBegin "primitive"\n            TransformBegin\n'
       f'                Translate {NC[0]} {NC[1]} {NC[2]}\n'
       + rational_nurbs_sphere("                ", 0.5)
       + '            TransformEnd\n        SolidEnd\n    SolidEnd\n')
ff_scene("csg-nurbs-rational-sphere-alone-raytrace", "raytrace", 400, 400, [
    "Spec 021 (T010): the rational NURBS sphere of csg-nurbs-rational-sphere-union-*.rib alone,",
    "same camera. Untouched area (the operand's opaque area in this render, eroded 3 px, minus the",
    "projected Sphere 0.7 disc plus 6 px): mask:examples/rib/tests/masks/csg-nurbs-rational-untouched.png",
], '    TransformBegin\n' f'        Translate {NC[0]} {NC[1]} {NC[2]}\n'
   + rational_nurbs_sphere("        ", 0.5) + '    TransformEnd\n')

# Subdivision twins: copy the existing raytrace scene's world block
sub = open(os.path.join(OUT, "csg-subdivision-sphere-union-raytrace.rib")).read()
write("csg-subdivision-sphere-union-reyes.rib",
      sub.replace('Hider "raytrace"', 'Hider "reyes"')
         .replace("csg-subdivision-sphere-union-raytrace.tif", "csg-subdivision-sphere-union-reyes.tif")
         .replace("version 3.03\n", "version 3.03\n# Spec 021 (T010): REYES twin of csg-subdivision-sphere-union-raytrace.rib.\n", 1))
start = sub.index('        SolidBegin "primitive"\n            TransformBegin')
end = sub.index('            TransformEnd\n', start) + len('            TransformEnd\n')
subop = sub[start:end].replace('        SolidBegin "primitive"\n', '')
alone = sub[:sub.index('    SolidBegin "union"')] + subop.replace("            ", "    ", 1) + "WorldEnd\n"
alone = (alone.replace("csg-subdivision-sphere-union-raytrace.tif", "csg-subdivision-sphere-alone-raytrace.tif")
         .replace("version 3.03\n", "version 3.03\n# Spec 021 (T010): the subdivision operand of csg-subdivision-sphere-union-raytrace.rib\n"
                  "# alone, same camera. Untouched area (the operand's opaque area in this render, eroded 3 px, minus\n"
                  "# the projected Sphere 0.7 disc plus 6 px): mask:examples/rib/tests/masks/csg-subdivision-untouched.png\n", 1))
write("csg-subdivision-sphere-alone-raytrace.rib", alone)

# NURBS-sheet expected and instanced
nu = open(os.path.join(OUT, "csg-nurbs-sphere-union-raytrace.rib")).read()
pre = nu[:nu.index('    SolidBegin "union"')]
write("csg-nurbs-sheet-expected-raytrace.rib",
      pre.replace("csg-nurbs-sphere-union-raytrace.tif", "csg-nurbs-sheet-expected-raytrace.tif")
         .replace("version 3.03\n", "version 3.03\n# Spec 021 (T010): csg-nurbs-sphere-union-raytrace.rib with its open NURBS sheet removed and\n"
                  "# no solid block: what that scene must render once the open operand contributes nothing.\n", 1)
      + "    Sphere 0.7 -0.7 0.7 360\nWorldEnd\n")
solid = nu[nu.index('    SolidBegin "union"'):nu.index("WorldEnd")]
inst = (pre.replace("csg-nurbs-sphere-union-raytrace.tif", "csg-nurbs-sheet-instanced-raytrace.tif")
           .replace("Format 200 200 1", "Format 400 200 1")
           .replace("version 3.03\n", "version 3.03\n# Spec 021 (T010): the solid of csg-nurbs-sphere-union-raytrace.rib (whose NURBS operand is\n"
                    "# an open sheet) inside ObjectBegin/ObjectEnd, instanced twice: exactly one \"operand is\n"
                    "# not closed\" warning per frame (FR-021).\n", 1)
        + '    ObjectBegin "sheetSolid"\n' + "".join("    " + l + "\n" for l in solid.rstrip("\n").split("\n"))
        + '    ObjectEnd\n    TransformBegin\n        Translate -0.9 0 0\n        ObjectInstance "sheetSolid"\n'
        '    TransformEnd\n    TransformBegin\n        Translate 0.9 0 0\n        ObjectInstance "sheetSolid"\n'
        '    TransformEnd\nWorldEnd\n')
write("csg-nurbs-sheet-instanced-raytrace.rib", inst)

# Polygon-mesh operands
CUBE_P = "-0.5 -0.5 -0.5  0.5 -0.5 -0.5  0.5 0.5 -0.5  -0.5 0.5 -0.5  -0.5 -0.5 0.5  0.5 -0.5 0.5  0.5 0.5 0.5  -0.5 0.5 0.5"
FACES = ["0 3 2 1", "4 5 6 7", "0 1 5 4", "2 3 7 6", "1 2 6 5", "0 4 7 3"]
def pmesh(indent, faces):
    return (f'{indent}PointsPolygons [{" ".join("4" for _ in faces)}]\n'
            f'{indent}    [{"  ".join(faces)}]\n{indent}    "P" [{CUBE_P}]\n')
for hider in ("raytrace", "reyes"):
    ff_scene(f"csg-polymesh-box-difference-{hider}", hider, 400, 400, [
        "Spec 021 (T010): a PointsPolygons unit cube minus a sphere of radius 0.6 centred on its",
        "corner (0.5, 0.5, -0.5): a mesh-type operand (FR-018)."],
        '    SolidBegin "difference"\n        SolidBegin "primitive"\n' + pmesh("            ", FACES)
        + '        SolidEnd\n        SolidBegin "primitive"\n            TransformBegin\n'
        '                Translate 0.5 0.5 -0.5\n                Sphere 0.6 -0.6 0.6 360\n'
        '            TransformEnd\n        SolidEnd\n    SolidEnd\n')
ff_scene("csg-polymesh-open-raytrace", "raytrace", 400, 400, [
    "Spec 021 (T010): a PointsPolygons box with its +x face missing (an open mesh) as the second",
    "operand of a difference with a sphere. The open mesh is an empty operand with one \"operand",
    "is not closed\" warning, so the sphere renders unchanged (FR-021, FR-025a)."],
    '    SolidBegin "difference"\n        SolidBegin "primitive"\n            Sphere 0.8 -0.8 0.8 360\n'
    '        SolidEnd\n        SolidBegin "primitive"\n            TransformBegin\n'
    '                Translate 0.5 0 0\n' + pmesh("                ", [f for f in FACES if f != "1 2 6 5"])
    + '            TransformEnd\n        SolidEnd\n    SolidEnd\n')

# ---------------------------------------------------------------------------
# T010 secondary rays: the hero solid seen only through a 45-degree mirror
# ---------------------------------------------------------------------------
def secondary(n, hider):
    w, h = 800, 500
    c = Cam(38, w, h, (0, 0, 0))  # camera at the world origin: the mirror geometry is in world space
    hole, seam, sil = hero_radii(w, h)
    # The camera looks along +z at a mirror (plane x = z - 3); it reflects the view to +x,
    # where the hero solid sits at (6, 0, 3) with its hole along x: total path 3 + 6 = 9,
    # so the reflected solid appears exactly as the hero view, mirrored. A back plane at
    # x = 11 (5 behind the solid) is lit by a distant light travelling +x along the hole axis.
    spot = c.s * 0.65 / (9 + 5)             # lit spot through the hole, on the back plane
    body = header([
        "Spec 021 (T010): secondary rays (FR-013). The camera sees the hero solid only through a",
        "45-degree mirror; behind the solid, a back plane is lit by a distant light that travels",
        "along the hole axis, so the solid's shadow on it has a lit spot where the hole lets light",
        "through. Reflection rays must respect the hole; shadow (transmission) rays must too.",
        "",
        f"Image regions at {w}x{h}, centred on ({w/2:g}, {h/2:g}) (mirror image of the hero view):",
        f"  shadow-hole spot (lit back plane through the hole)   disc:400,250,{spot - 3:.1f}",
        f"  shadow body (shadowed back plane seen through hole)  annulus:400,250,{spot + 3:.1f},{hole - 3:.1f}",
        f"  open lit ground (back plane beside the solid)        annulus:400,250,{sil + 15:.1f},{sil + 60:.1f}",
        f"  reflected hole (through-hole, shadow ring)           annulus:400,250,{spot + 3:.1f},{hole - 3:.1f}",
        f"  reflected sphere ring (outer sphere surface)         annulus:400,250,{seam + 4:.1f},{sil - 4:.1f}",
    ])
    body += HIDERS[hider] + "\nPixelSamples 4 4\n" + f'Display "{n}.tif" "file" "rgba"\n' + c.rib() + "\n"
    body += "WorldBegin\n"
    body += '    LightSource "ambientlight" 1 "intensity" [0.05]\n'
    body += '    LightSource "raydistant" 2 "intensity" [1.2] "from" [-100 0 0] "to" [0 0 0]\n'
    body += '    Attribute "visibility" "int specular" [1] "int transmission" [1]\n'
    body += '    AttributeBegin\n        # the mirror: invisible to shadow rays, so it never blocks the light\n'
    body += '        Attribute "visibility" "int transmission" [0]\n'
    body += '        Surface "mirror" "float Ka" [0] "float Ks" [0] "float Kr" [1]\n'
    body += '        Polygon "P" [-8 -8 -5  -8 8 -5  14 8 17  14 -8 17]\n    AttributeEnd\n'
    body += '    AttributeBegin\n        # the back plane x = 11\n        Color [0.9 0.9 0.9]\n        Surface "matte"\n'
    body += '        Polygon "P" [11 -12 -9  11 12 -9  11 12 15  11 -12 15]\n    AttributeEnd\n'
    body += '    AttributeBegin\n        Color [0.8 0.6 0.4]\n        Surface "plastic"\n'
    body += '        Translate 6 0 3\n        Rotate 90 0 1 0\n'
    body += hero_solid(indent="        ")
    body += '    AttributeEnd\nWorldEnd\n'
    write(n + ".rib", body)

for hider in ("raytrace", "reyes"):
    secondary(f"csg-secondary-rays-{hider}", hider)
print("scenes written")
