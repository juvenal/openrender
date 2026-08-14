local prman = {}

local Ri = {}
Ri.__index = Ri

-- Standard Tokens
Ri.FRAMEBUFFER = "framebuffer"
Ri.FILE = "file"
Ri.RGB = "rgb"
Ri.RGBA = "rgba"
Ri.RGBZ = "rgbz"
Ri.RGBAZ = "rgbaz"
Ri.A = "a"
Ri.Z = "z"
Ri.AZ = "az"
Ri.PERSPECTIVE = "perspective"
Ri.ORTHOGRAPHIC = "orthographic"
Ri.HIDDEN = "hidden"
Ri.PAINT = "paint"
Ri.CONSTANT = "constant"
Ri.SMOOTH = "smooth"
Ri.FLATNESS = "flatness"
Ri.FOV = "fov"
Ri.AMBIENTLIGHT = "ambientlight"
Ri.POINTLIGHT = "pointlight"
Ri.DISTANTLIGHT = "distantlight"
Ri.SPOTLIGHT = "spotlight"
Ri.INTENSITY = "intensity"
Ri.LIGHTCOLOR = "lightcolor"
Ri.FROM = "from"
Ri.TO = "to"
Ri.CONEANGLE = "coneangle"
Ri.CONEDELTAANGLE = "conedeltaangle"
Ri.BEAMDISTRIBUTION = "beamdistribution"
Ri.MATTE = "matte"
Ri.METAL = "metal"
Ri.SHINYMETAL = "shinymetal"
Ri.PLASTIC = "plastic"
Ri.PAINTEDPLASTIC = "paintedplastic"
Ri.KA = "ka"
Ri.KD = "kd"
Ri.KS = "ks"
Ri.ROUGHNESS = "roughness"
Ri.KR = "kr"
Ri.TEXTURENAME = "texturename"
Ri.SPECULARCOLOR = "specularcolor"
Ri.DEPTHCUE = "depthcue"
Ri.FOG = "fog"
Ri.BUMPY = "bumpy"
Ri.MINDISTANCE = "mindistance"
Ri.BACKGROUND = "background"
Ri.DISTANCE = "distance"
Ri.AMPLITUDE = "amplitude"
Ri.RASTER = "raster"
Ri.SCREEN = "screen"
Ri.CAMERA = "camera"
Ri.WORLD = "world"
Ri.OBJECT = "object"
Ri.INSIDE = "inside"
Ri.OUTSIDE = "outside"
Ri.LH = "lh"
Ri.RH = "rh"
Ri.P = "P"
Ri.PZ = "Pz"
Ri.PW = "Pw"
Ri.N = "N"
Ri.NP = "Np"
Ri.CS = "Cs"
Ri.OS = "Os"
Ri.S = "s"
Ri.T = "t"
Ri.ST = "st"
Ri.BILINEAR = "bilinear"
Ri.BICUBIC = "bicubic"
Ri.PRIMITIVE = "primitive"
Ri.INTERSECTION = "intersection"
Ri.UNION = "union"
Ri.DIFFERENCE = "difference"
Ri.PERIODIC = "periodic"
Ri.NOWRAP = "nowrap"
Ri.NONPERIODIC = "nonperiodic"
Ri.CLAMP = "clamp"
Ri.BLACK = "black"
Ri.IGNORE = "ignore"
Ri.PRINT = "print"
Ri.ABORT = "abort"
Ri.HANDLER = "handler"
Ri.ORIGIN = "origin"
Ri.IDENTIFIER = "identifier"
Ri.NAME = "name"
Ri.COMMENT = "comment"
Ri.STRUCTURE = "structure"
Ri.VERBATIM = "verbatim"
Ri.LINEAR = "linear"
Ri.CUBIC = "cubic"
Ri.WIDTH = "width"
Ri.CONSTANTWIDTH = "constantwidth"
Ri.CATMULLCLARK = "catmull-clark"
Ri.HOLE = "hole"
Ri.CREASE = "crease"
Ri.CORNER = "corner"
Ri.INTERPOLATEBOUNDARY = "interpolateboundary"
Ri.CURRENT = "current"
Ri.SHADER = "shader"
Ri.EYE = "eye"
Ri.NDC = "NDC"

-- Filters
Ri.BOXFILTER = "box"
Ri.TRIANGLEFILTER = "triangle"
Ri.GAUSSIANFILTER = "gaussian"
Ri.SINCFILTER = "sinc"
Ri.CATMULLROMFILTER = "catmull-rom"
Ri.BLACKMANHARRISFILTER = "blackman-harris"
Ri.MITCHELLFILTER = "mitchell"
Ri.BESSELFILTER = "bessel"
Ri.DISKFILTER = "disk"

-- Other constants
Ri.RI_FALSE = 0
Ri.RI_TRUE = 1
Ri.RI_INFINITY = 1.0e38
Ri.RI_EPSILON = 1.0e-10
Ri.RI_NULL = nil
Ri.VERSION = "1.0.0"

function Ri:new()
    local obj = {
        _stream = nil,
        _is_pipe = false,
        _indent = 0,
        _light_count = 0,
        _object_count = 0
    }
    setmetatable(obj, self)
    return obj
end

local function is_array(t)
    if type(t) ~= "table" then return false end
    local i = 0
    for _ in pairs(t) do
        i = i + 1
        if t[i] == nil then return false end
    end
    return i > 0
end

function Ri:_to_rib(arg)
    if arg == nil then
        return ""
    end
    if type(arg) == "string" then
        return '"' .. arg .. '"'
    end
    if type(arg) == "number" then
        return tostring(arg)
    end
    if type(arg) == "table" then
        if is_array(arg) then
            local res = {}
            for _, v in ipairs(arg) do
                table.insert(res, tostring(v))
            end
            return "[" .. table.concat(res, " ") .. "]"
        else
            -- Dictionary (parameter list)
            local res = {}
            for k, v in pairs(arg) do
                table.insert(res, '"' .. k .. '"')
                local val = self:_to_rib(v)
                if val:sub(1, 1) ~= "[" then
                    val = "[" .. val .. "]"
                end
                table.insert(res, val)
            end
            return table.concat(res, " ")
        end
    end
    return tostring(arg)
end

function Ri:_write(cmd, ...)
    if not self._stream then return end
    local args = {...}
    local rib_args = {}
    for _, a in ipairs(args) do
        local r = self:_to_rib(a)
        if r ~= "" then
            table.insert(rib_args, r)
        end
    end
    local line = string.rep("  ", self._indent) .. cmd .. " " .. table.concat(rib_args, " ")
    self._stream:write(line:gsub("%s+$", "") .. "\n")
    self._stream:flush()
end

-- 4.1.1 Graphics State
local function write_header(stream, version)
    stream:write("##RenderMan RIB-Structure 1.1\n")
    stream:write("##Creator openRender " .. version .. "\n")
    stream:write("##CreationDate " .. os.date("%a %b %d %H:%M:%S %Y") .. "\n")
end

function Ri:Begin(name, options)
    if name == nil or name == "" or name == "render" then
        self._stream = io.stdout
    elseif name:match("%.rib$") then
        self._stream = io.open(name, "w")
    else
        -- Command mode
        local cmd = name
        if options and options ~= "" then
            cmd = cmd .. " " .. options
        end
        self._stream = io.popen(cmd, "w")
        self._is_pipe = true
    end
    write_header(self._stream, self.VERSION)
end

function Ri:End()
    if self._stream and self._stream ~= io.stdout then
        self._stream:close()
        self._stream = nil
        self._is_pipe = false
    end
end

function Ri:FrameBegin(number)
    self:_write("FrameBegin", number)
    self._indent = self._indent + 1
end

function Ri:FrameEnd()
    self._indent = self._indent - 1
    self:_write("FrameEnd")
end

function Ri:WorldBegin()
    self:_write("WorldBegin")
    self._indent = self._indent + 1
end

function Ri:WorldEnd()
    self._indent = self._indent - 1
    self:_write("WorldEnd")
end

-- 4.1.1 Camera Options
function Ri:Format(xres, yres, aspect)
    self:_write("Format", xres, yres, aspect)
end

function Ri:FrameAspectRatio(aspect)
    self:_write("FrameAspectRatio", aspect)
end

function Ri:ScreenWindow(left, right, bottom, top)
    self:_write("ScreenWindow", left, right, bottom, top)
end

function Ri:CropWindow(xmin, xmax, ymin, ymax)
    self:_write("CropWindow", {xmin, xmax, ymin, ymax})
end

function Ri:Projection(name, parameterlist)
    self:_write("Projection", name, parameterlist)
end

function Ri:Clipping(near, far)
    self:_write("Clipping", near, far)
end

function Ri:ClippingPlane(x, y, z, nx, ny, nz)
    self:_write("ClippingPlane", x, y, z, nx, ny, nz)
end

function Ri:DepthOfField(fstop, focallength, focaldistance)
    self:_write("DepthOfField", fstop, focallength, focaldistance)
end

function Ri:Shutter(smin, smax)
    self:_write("Shutter", smin, smax)
end

-- 4.1.2 Display Options
function Ri:Display(name, type, mode, parameterlist)
    self:_write("Display", name, type, mode, parameterlist)
end

function Ri:PixelVariance(variation)
    self:_write("PixelVariance", variation)
end

function Ri:PixelSamples(xsamples, ysamples)
    self:_write("PixelSamples", xsamples, ysamples)
end

function Ri:PixelFilter(filterfunc, xwidth, ywidth)
    self:_write("PixelFilter", filterfunc, xwidth, ywidth)
end

function Ri:Exposure(gain, gamma)
    self:_write("Exposure", gain, gamma)
end

function Ri:Imager(name, parameterlist)
    self:_write("Imager", name, parameterlist)
end

function Ri:Quantize(type_str, one, qmin, qmax, ampl)
    self:_write("Quantize", type_str, one, qmin, qmax, ampl)
end

function Ri:DisplayChannel(channel, parameterlist)
    self:_write("DisplayChannel", channel, parameterlist)
end

-- 4.1.3 Additional Options
function Ri:Hider(type_str, parameterlist)
    self:_write("Hider", type_str, parameterlist)
end

function Ri:ColorSamples(nRGB, RGBn)
    self:_write("ColorSamples", nRGB, RGBn)
end

function Ri:RelativeDetail(relativedetail)
    self:_write("RelativeDetail", relativedetail)
end

function Ri:Option(name, parameterlist)
    self:_write("Option", name, parameterlist)
end

-- 4.2 Attributes
function Ri:AttributeBegin()
    self:_write("AttributeBegin")
    self._indent = self._indent + 1
end

function Ri:AttributeEnd()
    self._indent = self._indent - 1
    self:_write("AttributeEnd")
end

function Ri:Color(Cs)
    self:_write("Color", Cs)
end

function Ri:Opacity(Os)
    self:_write("Opacity", Os)
end

function Ri:TextureCoordinates(s1, t1, s2, t2, s3, t3, s4, t4)
    self:_write("TextureCoordinates", {s1, t1, s2, t2, s3, t3, s4, t4})
end

-- 4.2.3 Light Sources
function Ri:LightSource(name, parameterlist)
    self._light_count = self._light_count + 1
    self:_write("LightSource", name, self._light_count, parameterlist)
    return self._light_count
end

function Ri:AreaLightSource(name, parameterlist)
    self._light_count = self._light_count + 1
    self:_write("AreaLightSource", name, self._light_count, parameterlist)
    return self._light_count
end

function Ri:Illuminate(light, onoff)
    self:_write("Illuminate", light, onoff)
end

-- 4.2.4 Surface Shading
function Ri:Surface(name, parameterlist)
    self:_write("Surface", name, parameterlist)
end

function Ri:Atmosphere(name, parameterlist)
    self:_write("Atmosphere", name, parameterlist)
end

function Ri:Interior(name, parameterlist)
    self:_write("Interior", name, parameterlist)
end

function Ri:Exterior(name, parameterlist)
    self:_write("Exterior", name, parameterlist)
end

function Ri:ShadingRate(size)
    self:_write("ShadingRate", size)
end

function Ri:ShadingInterpolation(type_str)
    self:_write("ShadingInterpolation", type_str)
end

function Ri:Matte(onoff)
    self:_write("Matte", onoff)
end

-- 4.2.10 Bound
function Ri:Bound(bound)
    self:_write("Bound", bound)
end

function Ri:Detail(bound)
    self:_write("Detail", bound)
end

function Ri:DetailRange(minvis, lowtran, uptran, maxvis)
    self:_write("DetailRange", {minvis, lowtran, uptran, maxvis})
end

function Ri:GeometricApproximation(type_str, value)
    self:_write("GeometricApproximation", type_str, value)
end

function Ri:Orientation(orientation)
    self:_write("Orientation", orientation)
end

function Ri:ReverseOrientation()
    self:_write("ReverseOrientation")
end

function Ri:Sides(nsides)
    self:_write("Sides", nsides)
end

-- 4.3 Transformations
function Ri:Identity()
    self:_write("Identity")
end

function Ri:Transform(transform)
    self:_write("Transform", transform)
end

function Ri:ConcatTransform(transform)
    self:_write("ConcatTransform", transform)
end

function Ri:Perspective(fov)
    self:_write("Perspective", fov)
end

function Ri:Translate(dx, dy, dz)
    self:_write("Translate", dx, dy, dz)
end

function Ri:Rotate(angle, dx, dy, dz)
    self:_write("Rotate", angle, dx, dy, dz)
end

function Ri:Scale(sx, sy, sz)
    self:_write("Scale", sx, sy, sz)
end

function Ri:Skew(angle, dx1, dy1, dz1, dx2, dy2, dz2)
    self:_write("Skew", angle, dx1, dy1, dz1, dx2, dy2, dz2)
end

function Ri:Displacement(name, parameterlist)
    self:_write("Displacement", name, parameterlist)
end

function Ri:CoordinateSystem(space)
    self:_write("CoordinateSystem", space)
end

function Ri:CoordSysTransform(space)
    self:_write("CoordSysTransform", space)
end

function Ri:TransformBegin()
    self:_write("TransformBegin")
    self._indent = self._indent + 1
end

function Ri:TransformEnd()
    self._indent = self._indent - 1
    self:_write("TransformEnd")
end

-- 4.4 Implementation-specific Attributes
function Ri:Attribute(name, parameterlist)
    self:_write("Attribute", name, parameterlist)
end

-- 5 Geometric Primitives
function Ri:Polygon(parameterlist)
    self:_write("Polygon", parameterlist)
end

function Ri:GeneralPolygon(nvertices, parameterlist)
    self:_write("GeneralPolygon", nvertices, parameterlist)
end

function Ri:PointsPolygons(nvertices, vertices, parameterlist)
    self:_write("PointsPolygons", nvertices, vertices, parameterlist)
end

function Ri:PointsGeneralPolygons(nloops, nvertices, vertices, parameterlist)
    self:_write("PointsGeneralPolygons", nloops, nvertices, vertices, parameterlist)
end

function Ri:Basis(ubasis, ustep, vbasis, vstep)
    self:_write("Basis", ubasis, ustep, vbasis, vstep)
end

function Ri:Patch(type_str, parameterlist)
    self:_write("Patch", type_str, parameterlist)
end

function Ri:PatchMesh(type_str, nu, uwrap, nv, vwrap, parameterlist)
    self:_write("PatchMesh", type_str, nu, uwrap, nv, vwrap, parameterlist)
end

function Ri:NuPatch(nu, uorder, uknot, umin, umax, nv, vorder, vknot, vmin, vmax, parameterlist)
    self:_write("NuPatch", nu, uorder, uknot, umin, umax, nv, vorder, vknot, vmin, vmax, parameterlist)
end

function Ri:TrimCurve(ncurves, order, knot, min, max, n, u, v, w)
    self:_write("TrimCurve", ncurves, order, knot, min, max, n, u, v, w)
end

function Ri:Sphere(radius, zmin, zmax, thetamax, parameterlist)
    self:_write("Sphere", radius, zmin, zmax, thetamax, parameterlist)
end

function Ri:Cone(height, radius, thetamax, parameterlist)
    self:_write("Cone", height, radius, thetamax, parameterlist)
end

function Ri:Cylinder(radius, zmin, zmax, thetamax, parameterlist)
    self:_write("Cylinder", radius, zmin, zmax, thetamax, parameterlist)
end

function Ri:Hyperboloid(point1, point2, thetamax, parameterlist)
    self:_write("Hyperboloid", point1, point2, thetamax, parameterlist)
end

function Ri:Paraboloid(rmax, zmin, zmax, thetamax, parameterlist)
    self:_write("Paraboloid", rmax, zmin, zmax, thetamax, parameterlist)
end

function Ri:Disk(height, radius, thetamax, parameterlist)
    self:_write("Disk", height, radius, thetamax, parameterlist)
end

function Ri:Torus(majorrad, minorrad, phimin, phimax, thetamax, parameterlist)
    self:_write("Torus", majorrad, minorrad, phimin, phimax, thetamax, parameterlist)
end

function Ri:Points(parameterlist)
    self:_write("Points", parameterlist)
end

function Ri:Curves(type_str, nvertices, wrap, parameterlist)
    self:_write("Curves", type_str, nvertices, wrap, parameterlist)
end

function Ri:SubdivisionMesh(scheme, nvertices, vertices, tags, nargs, intargs, floatargs, parameterlist)
    tags = tags or {}
    nargs = nargs or {}
    intargs = intargs or {}
    floatargs = floatargs or {}
    self:_write("SubdivisionMesh", scheme, nvertices, vertices, tags, nargs, intargs, floatargs, parameterlist)
end

function Ri:HierarchicalSubdivisionMesh(scheme, nvertices, vertices, tags, nargs, intargs, floatargs, overrideFaceIndex, overrideLevel, overrideTags, overrideValues, parameterlist)
    tags = tags or {}
    nargs = nargs or {}
    intargs = intargs or {}
    floatargs = floatargs or {}
    overrideFaceIndex = overrideFaceIndex or {}
    overrideLevel = overrideLevel or {}
    overrideTags = overrideTags or {}
    overrideValues = overrideValues or {}
    self:_write("HierarchicalSubdivisionMesh", scheme, nvertices, vertices, tags, nargs, intargs, floatargs, overrideFaceIndex, overrideLevel, overrideTags, overrideValues, parameterlist)
end

function Ri:Blobby(nleaf, code, floats, strings, parameterlist)
    self:_write("Blobby", nleaf, code, floats, strings, parameterlist)
end

function Ri:Procedural(procname, args, bound)
    self:_write("Procedural", procname, args, bound)
end

function Ri:Geometry(name, parameterlist)
    self:_write("Geometry", name, parameterlist)
end

function Ri:SolidBegin(operation)
    self:_write("SolidBegin", operation)
    self._indent = self._indent + 1
end

function Ri:SolidEnd()
    self._indent = self._indent - 1
    self:_write("SolidEnd")
end

function Ri:ObjectBegin()
    self._object_count = self._object_count + 1
    self:_write("ObjectBegin", self._object_count)
    self._indent = self._indent + 1
    return self._object_count
end

function Ri:ObjectEnd()
    self._indent = self._indent - 1
    self:_write("ObjectEnd")
end

function Ri:ObjectInstance(handle)
    self:_write("ObjectInstance", handle)
end

-- 6 Motion
function Ri:MotionBegin(times)
    self:_write("MotionBegin", times)
    self._indent = self._indent + 1
end

function Ri:MotionEnd()
    self._indent = self._indent - 1
    self:_write("MotionEnd")
end

-- 7 External Resources
function Ri:MakeTexture(picturename, texturename, swrap, twrap, filterfunc, swidth, twidth, parameterlist)
    self:_write("MakeTexture", picturename, texturename, swrap, twrap, filterfunc, swidth, twidth, parameterlist)
end

function Ri:MakeLatLongEnvironment(picturename, texturename, filterfunc, swidth, twidth, parameterlist)
    self:_write("MakeLatLongEnvironment", picturename, texturename, filterfunc, swidth, twidth, parameterlist)
end

function Ri:MakeCubeFaceEnvironment(px, nx, py, ny, pz, nz, texturename, fov, filterfunc, swidth, twidth, parameterlist)
    self:_write("MakeCubeFaceEnvironment", px, nx, py, ny, pz, nz, texturename, fov, filterfunc, swidth, twidth, parameterlist)
end

function Ri:MakeShadow(picturename, texturename, parameterlist)
    self:_write("MakeShadow", picturename, texturename, parameterlist)
end

function Ri:MakeBrickMap(src, dest, parameterlist)
    self:_write("MakeBrickMap", src, dest, parameterlist)
end

function Ri:ErrorHandler(handler)
    self:_write("ErrorHandler", handler)
end

function Ri:ReadArchive(name, callback, parameterlist)
    self:_write("ReadArchive", name, parameterlist)
end

function Ri:ArchiveBegin(name, parameterlist)
    self:_write("ArchiveBegin", name, parameterlist)
    self._indent = self._indent + 1
end

function Ri:ArchiveEnd()
    self._indent = self._indent - 1
    self:_write("ArchiveEnd")
end

function Ri:ArchiveRecord(type_str, message)
    if type_str == self.COMMENT then
        self:_write("#", message)
    elseif type_str == self.VERBATIM then
        if self._stream then
            self._stream:write(message .. "\n")
        end
    else
        self:_write("ArchiveRecord", type_str, message)
    end
end

function Ri:IfBegin(expr, parameterlist)
    self:_write("IfBegin", expr, parameterlist)
    self._indent = self._indent + 1
end

function Ri:ElseIf(expr, parameterlist)
    self._indent = self._indent - 1
    self:_write("ElseIf", expr, parameterlist)
    self._indent = self._indent + 1
end

function Ri:Else()
    self._indent = self._indent - 1
    self:_write("Else")
    self._indent = self._indent + 1
end

function Ri:IfEnd()
    self._indent = self._indent - 1
    self:_write("IfEnd")
end

function Ri:Resource(handle, type_str, parameterlist)
    self:_write("Resource", handle, type_str, parameterlist)
end

function Ri:ResourceBegin()
    self:_write("ResourceBegin")
    self._indent = self._indent + 1
end

function Ri:ResourceEnd()
    self._indent = self._indent - 1
    self:_write("ResourceEnd")
end

function Ri:Declare(name, declaration)
    self:_write("Declare", name, declaration)
    return name
end

prman.Ri = Ri
return prman
