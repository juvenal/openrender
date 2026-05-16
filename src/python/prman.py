import sys
import subprocess
import shlex

class Ri:
    # Standard Tokens
    FRAMEBUFFER = "framebuffer"
    FILE = "file"
    RGB = "rgb"
    RGBA = "rgba"
    RGBZ = "rgbz"
    RGBAZ = "rgbaz"
    A = "a"
    Z = "z"
    AZ = "az"
    PERSPECTIVE = "perspective"
    ORTHOGRAPHIC = "orthographic"
    HIDDEN = "hidden"
    PAINT = "paint"
    CONSTANT = "constant"
    SMOOTH = "smooth"
    FLATNESS = "flatness"
    FOV = "fov"
    AMBIENTLIGHT = "ambientlight"
    POINTLIGHT = "pointlight"
    DISTANTLIGHT = "distantlight"
    SPOTLIGHT = "spotlight"
    INTENSITY = "intensity"
    LIGHTCOLOR = "lightcolor"
    FROM = "from"
    TO = "to"
    CONEANGLE = "coneangle"
    CONEDELTAANGLE = "conedeltaangle"
    BEAMDISTRIBUTION = "beamdistribution"
    MATTE = "matte"
    METAL = "metal"
    SHINYMETAL = "shinymetal"
    PLASTIC = "plastic"
    PAINTEDPLASTIC = "paintedplastic"
    KA = "ka"
    KD = "kd"
    KS = "ks"
    ROUGHNESS = "roughness"
    KR = "kr"
    TEXTURENAME = "texturename"
    SPECULARCOLOR = "specularcolor"
    DEPTHCUE = "depthcue"
    FOG = "fog"
    BUMPY = "bumpy"
    MINDISTANCE = "mindistance"
    BACKGROUND = "background"
    DISTANCE = "distance"
    AMPLITUDE = "amplitude"
    RASTER = "raster"
    SCREEN = "screen"
    CAMERA = "camera"
    WORLD = "world"
    OBJECT = "object"
    INSIDE = "inside"
    OUTSIDE = "outside"
    LH = "lh"
    RH = "rh"
    P = "P"
    PZ = "Pz"
    PW = "Pw"
    N = "N"
    NP = "Np"
    CS = "Cs"
    OS = "Os"
    S = "s"
    T = "t"
    ST = "st"
    BILINEAR = "bilinear"
    BICUBIC = "bicubic"
    PRIMITIVE = "primitive"
    INTERSECTION = "intersection"
    UNION = "union"
    DIFFERENCE = "difference"
    PERIODIC = "periodic"
    NOWRAP = "nowrap"
    NONPERIODIC = "nonperiodic"
    CLAMP = "clamp"
    BLACK = "black"
    IGNORE = "ignore"
    PRINT = "print"
    ABORT = "abort"
    HANDLER = "handler"
    ORIGIN = "origin"
    IDENTIFIER = "identifier"
    NAME = "name"
    COMMENT = "comment"
    STRUCTURE = "structure"
    VERBATIM = "verbatim"
    LINEAR = "linear"
    CUBIC = "cubic"
    WIDTH = "width"
    CONSTANTWIDTH = "constantwidth"
    CATMULLCLARK = "catmull-clark"
    HOLE = "hole"
    CREASE = "crease"
    CORNER = "corner"
    INTERPOLATEBOUNDARY = "interpolateboundary"
    CURRENT = "current"
    SHADER = "shader"
    EYE = "eye"
    NDC = "NDC"

    # Filters
    BOXFILTER = "box"
    TRIANGLEFILTER = "triangle"
    GAUSSIANFILTER = "gaussian"
    SINCFILTER = "sinc"
    CATMULLROMFILTER = "catmull-rom"
    BLACKMANHARRISFILTER = "blackman-harris"
    MITCHELLFILTER = "mitchell"
    BESSELFILTER = "bessel"
    DISKFILTER = "disk"

    # Other constants
    RI_FALSE = 0
    RI_TRUE = 1
    RI_INFINITY = 1.0e38
    RI_EPSILON = 1.0e-10
    RI_NULL = None

    def __init__(self):
        self._stream = None
        self._proc = None
        self._indent = 0
        self._light_count = 0
        self._object_count = 0

    def _to_rib(self, arg):
        if arg is None:
            return ""
        if isinstance(arg, str):
            # Tokens should be quoted if they are names/paths, but some keywords might not be.
            # However, RI C Bind passes strings for tokens. RIB usually quotes them.
            return f'"{arg}"'
        if isinstance(arg, (int, float)):
            return str(arg)
        if isinstance(arg, (list, tuple)):
            return "[" + " ".join(str(x) for x in arg) + "]"
        if isinstance(arg, dict):
            res = []
            for k, v in arg.items():
                res.append(f'"{k}"')
                val = self._to_rib(v)
                if not val.startswith("["):
                    val = f"[{val}]"
                res.append(val)
            return " ".join(res)
        return str(arg)

    def _write(self, cmd, *args):
        if not self._stream:
            return
        
        rib_args = [self._to_rib(a) for a in args if a is not None]
        line = "  " * self._indent + cmd + " " + " ".join(rib_args)
        self._stream.write(line.rstrip() + "\n")
        if hasattr(self._stream, "flush"):
            self._stream.flush()

    # 4.1.1 Graphics State
    def Begin(self, name=None, options=""):
        if name is None or name == "" or name == "render":
            self._stream = sys.stdout
        elif name.endswith(".rib"):
            self._stream = open(name, "w")
        else:
            # Command mode (e.g., "orender", "prman")
            cmd = name
            if options:
                cmd += " " + options
            self._proc = subprocess.Popen(
                shlex.split(cmd),
                stdin=subprocess.PIPE,
                text=True,
                bufsize=1
            )
            self._stream = self._proc.stdin

    def End(self):
        if self._proc:
            if self._proc.stdin:
                self._proc.stdin.close()
            self._proc.wait()
            self._proc = None
            self._stream = None
        elif self._stream and self._stream != sys.stdout:
            self._stream.close()
            self._stream = None

    def FrameBegin(self, number):
        self._write("FrameBegin", number)
        self._indent += 1

    def FrameEnd(self):
        self._indent -= 1
        self._write("FrameEnd")

    def WorldBegin(self):
        self._write("WorldBegin")
        self._indent += 1

    def WorldEnd(self):
        self._indent -= 1
        self._write("WorldEnd")

    # 4.1.1 Camera Options
    def Format(self, xres, yres, aspect):
        self._write("Format", xres, yres, aspect)

    def FrameAspectRatio(self, aspect):
        self._write("FrameAspectRatio", aspect)

    def ScreenWindow(self, left, right, bottom, top):
        self._write("ScreenWindow", left, right, bottom, top)

    def CropWindow(self, xmin, xmax, ymin, ymax):
        self._write("CropWindow", [xmin, xmax, ymin, ymax])

    def Projection(self, name, parameterlist=None):
        self._write("Projection", name, parameterlist)

    def Clipping(self, near, far):
        self._write("Clipping", near, far)

    def ClippingPlane(self, x, y, z, nx, ny, nz):
        self._write("ClippingPlane", x, y, z, nx, ny, nz)

    def DepthOfField(self, fstop, focallength, focaldistance):
        self._write("DepthOfField", fstop, focallength, focaldistance)

    def Shutter(self, smin, smax):
        self._write("Shutter", smin, smax)

    # 4.1.2 Display Options
    def Display(self, name, type, mode, parameterlist=None):
        self._write("Display", name, type, mode, parameterlist)

    def PixelVariance(self, variation):
        self._write("PixelVariance", variation)

    def PixelSamples(self, xsamples, ysamples):
        self._write("PixelSamples", xsamples, ysamples)

    def PixelFilter(self, filterfunc, xwidth, ywidth):
        self._write("PixelFilter", filterfunc, xwidth, ywidth)

    def Exposure(self, gain, gamma):
        self._write("Exposure", gain, gamma)

    def Imager(self, name, parameterlist=None):
        self._write("Imager", name, parameterlist)

    def Quantize(self, type, one, qmin, qmax, ampl):
        self._write("Quantize", type, one, qmin, qmax, ampl)

    def DisplayChannel(self, channel, parameterlist=None):
        self._write("DisplayChannel", channel, parameterlist)

    # 4.1.3 Additional Options
    def Hider(self, type, parameterlist=None):
        self._write("Hider", type, parameterlist)

    def ColorSamples(self, nRGB, RGBn):
        self._write("ColorSamples", nRGB, RGBn)

    def RelativeDetail(self, relativedetail):
        self._write("RelativeDetail", relativedetail)

    def Option(self, name, parameterlist=None):
        self._write("Option", name, parameterlist)

    # 4.2 Attributes
    def AttributeBegin(self):
        self._write("AttributeBegin")
        self._indent += 1

    def AttributeEnd(self):
        self._indent -= 1
        self._write("AttributeEnd")

    def Color(self, Cs):
        self._write("Color", Cs)

    def Opacity(self, Os):
        self._write("Opacity", Os)

    def TextureCoordinates(self, s1, t1, s2, t2, s3, t3, s4, t4):
        self._write("TextureCoordinates", [s1, t1, s2, t2, s3, t3, s4, t4])

    # 4.2.3 Light Sources
    def LightSource(self, name, parameterlist=None):
        self._light_count += 1
        self._write("LightSource", name, self._light_count, parameterlist)
        return self._light_count

    def AreaLightSource(self, name, parameterlist=None):
        self._light_count += 1
        self._write("AreaLightSource", name, self._light_count, parameterlist)
        return self._light_count

    def Illuminate(self, light, onoff):
        self._write("Illuminate", light, onoff)

    # 4.2.4 Surface Shading
    def Surface(self, name, parameterlist=None):
        self._write("Surface", name, parameterlist)

    def Atmosphere(self, name, parameterlist=None):
        self._write("Atmosphere", name, parameterlist)

    def Interior(self, name, parameterlist=None):
        self._write("Interior", name, parameterlist)

    def Exterior(self, name, parameterlist=None):
        self._write("Exterior", name, parameterlist)

    def ShadingRate(self, size):
        self._write("ShadingRate", size)

    def ShadingInterpolation(self, type):
        self._write("ShadingInterpolation", type)

    def Matte(self, onoff):
        self._write("Matte", onoff)

    # 4.2.10 Bound
    def Bound(self, bound):
        self._write("Bound", bound)

    def Detail(self, bound):
        self._write("Detail", bound)

    def DetailRange(self, minvis, lowtran, uptran, maxvis):
        self._write("DetailRange", [minvis, lowtran, uptran, maxvis])

    def GeometricApproximation(self, type, value):
        self._write("GeometricApproximation", type, value)

    def Orientation(self, orientation):
        self._write("Orientation", orientation)

    def ReverseOrientation(self):
        self._write("ReverseOrientation")

    def Sides(self, nsides):
        self._write("Sides", nsides)

    # 4.3 Transformations
    def Identity(self):
        self._write("Identity")

    def Transform(self, transform):
        self._write("Transform", transform)

    def ConcatTransform(self, transform):
        self._write("ConcatTransform", transform)

    def Perspective(self, fov):
        self._write("Perspective", fov)

    def Translate(self, dx, dy, dz):
        self._write("Translate", dx, dy, dz)

    def Rotate(self, angle, dx, dy, dz):
        self._write("Rotate", angle, dx, dy, dz)

    def Scale(self, sx, sy, sz):
        self._write("Scale", sx, sy, sz)

    def Skew(self, angle, dx1, dy1, dz1, dx2, dy2, dz2):
        self._write("Skew", angle, dx1, dy1, dz1, dx2, dy2, dz2)

    def Displacement(self, name, parameterlist=None):
        self._write("Displacement", name, parameterlist)

    def CoordinateSystem(self, space):
        self._write("CoordinateSystem", space)

    def CoordSysTransform(self, space):
        self._write("CoordSysTransform", space)

    def TransformBegin(self):
        self._write("TransformBegin")
        self._indent += 1

    def TransformEnd(self):
        self._indent -= 1
        self._write("TransformEnd")

    # 4.4 Implementation-specific Attributes
    def Attribute(self, name, parameterlist=None):
        self._write("Attribute", name, parameterlist)

    # 5 Geometric Primitives
    def Polygon(self, parameterlist=None):
        self._write("Polygon", parameterlist)

    def GeneralPolygon(self, nvertices, parameterlist=None):
        self._write("GeneralPolygon", nvertices, parameterlist)

    def PointsPolygons(self, nvertices, vertices, parameterlist=None):
        self._write("PointsPolygons", nvertices, vertices, parameterlist)

    def PointsGeneralPolygons(self, nloops, nvertices, vertices, parameterlist=None):
        self._write("PointsGeneralPolygons", nloops, nvertices, vertices, parameterlist)

    def Basis(self, ubasis, ustep, vbasis, vstep):
        self._write("Basis", ubasis, ustep, vbasis, vstep)

    def Patch(self, type, parameterlist=None):
        self._write("Patch", type, parameterlist)

    def PatchMesh(self, type, nu, uwrap, nv, vwrap, parameterlist=None):
        self._write("PatchMesh", type, nu, uwrap, nv, vwrap, parameterlist)

    def NuPatch(self, nu, uorder, uknot, umin, umax, nv, vorder, vknot, vmin, vmax, parameterlist=None):
        self._write("NuPatch", nu, uorder, uknot, umin, umax, nv, vorder, vknot, vmin, vmax, parameterlist)

    def TrimCurve(self, ncurves, order, knot, min, max, n, u, v, w):
        self._write("TrimCurve", ncurves, order, knot, min, max, n, u, v, w)

    def Sphere(self, radius, zmin, zmax, thetamax, parameterlist=None):
        self._write("Sphere", radius, zmin, zmax, thetamax, parameterlist)

    def Cone(self, height, radius, thetamax, parameterlist=None):
        self._write("Cone", height, radius, thetamax, parameterlist)

    def Cylinder(self, radius, zmin, zmax, thetamax, parameterlist=None):
        self._write("Cylinder", radius, zmin, zmax, thetamax, parameterlist)

    def Hyperboloid(self, point1, point2, thetamax, parameterlist=None):
        self._write("Hyperboloid", point1, point2, thetamax, parameterlist)

    def Paraboloid(self, rmax, zmin, zmax, thetamax, parameterlist=None):
        self._write("Paraboloid", rmax, zmin, zmax, thetamax, parameterlist)

    def Disk(self, height, radius, thetamax, parameterlist=None):
        self._write("Disk", height, radius, thetamax, parameterlist)

    def Torus(self, majorrad, minorrad, phimin, phimax, thetamax, parameterlist=None):
        self._write("Torus", majorrad, minorrad, phimin, phimax, thetamax, parameterlist)

    def Points(self, parameterlist=None):
        self._write("Points", parameterlist)

    def Curves(self, type, nvertices, wrap, parameterlist=None):
        self._write("Curves", type, nvertices, wrap, parameterlist)

    def SubdivisionMesh(self, scheme, nvertices, vertices, tags=None, nargs=None, intargs=None, floatargs=None, parameterlist=None):
        if tags is None: tags = []
        if nargs is None: nargs = []
        if intargs is None: intargs = []
        if floatargs is None: floatargs = []
        self._write("SubdivisionMesh", scheme, nvertices, vertices, tags, nargs, intargs, floatargs, parameterlist)

    def Blobby(self, nleaf, code, floats, strings, parameterlist=None):
        self._write("Blobby", nleaf, code, floats, strings, parameterlist)

    def Procedural(self, procname, args, bound):
        self._write("Procedural", procname, args, bound)

    def Geometry(self, name, parameterlist=None):
        self._write("Geometry", name, parameterlist)

    def SolidBegin(self, operation):
        self._write("SolidBegin", operation)
        self._indent += 1

    def SolidEnd(self):
        self._indent -= 1
        self._write("SolidEnd")

    def ObjectBegin(self):
        self._object_count += 1
        self._write("ObjectBegin", self._object_count)
        self._indent += 1
        return self._object_count

    def ObjectEnd(self):
        self._indent -= 1
        self._write("ObjectEnd")

    def ObjectInstance(self, handle):
        self._write("ObjectInstance", handle)

    # 6 Motion
    def MotionBegin(self, times):
        self._write("MotionBegin", times)
        self._indent += 1

    def MotionEnd(self):
        self._indent -= 1
        self._write("MotionEnd")

    # 7 External Resources
    def MakeTexture(self, picturename, texturename, swrap, twrap, filterfunc, swidth, twidth, parameterlist=None):
        self._write("MakeTexture", picturename, texturename, swrap, twrap, filterfunc, swidth, twidth, parameterlist)

    def MakeLatLongEnvironment(self, picturename, texturename, filterfunc, swidth, twidth, parameterlist=None):
        self._write("MakeLatLongEnvironment", picturename, texturename, filterfunc, swidth, twidth, parameterlist)

    def MakeCubeFaceEnvironment(self, px, nx, py, ny, pz, nz, texturename, fov, filterfunc, swidth, twidth, parameterlist=None):
        self._write("MakeCubeFaceEnvironment", px, nx, py, ny, pz, nz, texturename, fov, filterfunc, swidth, twidth, parameterlist)

    def MakeShadow(self, picturename, texturename, parameterlist=None):
        self._write("MakeShadow", picturename, texturename, parameterlist)

    def MakeBrickMap(self, src, dest, parameterlist=None):
        self._write("MakeBrickMap", src, dest, parameterlist)

    def ErrorHandler(self, handler):
        self._write("ErrorHandler", handler)

    def ReadArchive(self, name, callback=None, parameterlist=None):
        # Callback is handled on the C side, for RIB we just write the command
        self._write("ReadArchive", name, parameterlist)

    def ArchiveBegin(self, name, parameterlist=None):
        self._write("ArchiveBegin", name, parameterlist)
        self._indent += 1

    def ArchiveEnd(self):
        self._indent -= 1
        self._write("ArchiveEnd")

    def ArchiveRecord(self, type, message):
        if type == self.COMMENT:
            self._write("#", message)
        elif type == self.VERBATIM:
            if self._stream:
                self._stream.write(message + "\n")
        else:
            self._write("ArchiveRecord", type, message)

    def IfBegin(self, expr, parameterlist=None):
        self._write("IfBegin", expr, parameterlist)
        self._indent += 1

    def ElseIf(self, expr, parameterlist=None):
        self._indent -= 1
        self._write("ElseIf", expr, parameterlist)
        self._indent += 1

    def Else(self):
        self._indent -= 1
        self._write("Else")
        self._indent += 1

    def IfEnd(self):
        self._indent -= 1
        self._write("IfEnd")

    def Resource(self, handle, type, parameterlist=None):
        self._write("Resource", handle, type, parameterlist)

    def ResourceBegin(self):
        self._write("ResourceBegin")
        self._indent += 1

    def ResourceEnd(self):
        self._indent -= 1
        self._write("ResourceEnd")

    # Helper for token declaration
    def Declare(self, name, declaration):
        self._write("Declare", name, declaration)
        return name
