#!/usr/bin/env lua

-- Add src/lua to package.path to find prman.lua
local script_path = debug.getinfo(1, "S").source:sub(2)
local script_dir = script_path:match("(.*[/\\])") or "./"
package.path = script_dir .. "../../../src/lua/?.lua;" .. package.path

local prman = require("prman")
local Ri = prman.Ri

local NFRAMES = 100
local NSPHERES = 4
local FRAMEROT = 15.0
local XRES = 1280
local YRES = 720

local function ColorSpheres(ri, n, s)
    if n <= 0 then
        return
    end

    ri:AttributeBegin()

    ri:Translate(-0.5, -0.5, -0.5)
    ri:Scale(1.0 / n, 1.0 / n, 1.0 / n)

    for x = 0, n - 1 do
        for y = 0, n - 1 do
            for z = 0, n - 1 do
                local color = {
                    (x + 1) / n,
                    (y + 1) / n,
                    (z + 1) / n
                }

                ri:Color(color)
                ri:TransformBegin()
                ri:Translate(x + 0.5, y + 0.5, z + 0.5)
                ri:Scale(s, s, s)
                ri:Sphere(0.5, -0.5, 0.5, 360.0)
                ri:TransformEnd()
            end
        end
    end

    ri:AttributeEnd()
end

local function main()
    if #arg ~= 1 then
        io.stderr:write(string.format("Usage: %s ribFile.rib|#|orender\n\n", arg[0] or "colorcircles.lua"))
        os.exit(2)
    end

    local renderer = arg[1]
    local ri = Ri:new()

    ri:Begin(renderer)

    for frame = 0, NFRAMES do
        local filename = string.format("colorSpheres.%03d.tif", frame)
        ri:FrameBegin(frame)

        ri:Projection(ri.PERSPECTIVE)
        ri:Translate(0.0, 0.0, 1.5)
        ri:Rotate(40.0, -1.0, 1.0, 0.0)

        ri:Display(filename, ri.FRAMEBUFFER, ri.RGBA)
        ri:Format(XRES, YRES, -1.0)
        ri:ShadingRate(1.0)

        ri:WorldBegin()

        ri:LightSource(ri.DISTANTLIGHT)

        ri:Sides(1)

        -- C code: scale = (float)(NFRAMES - (frame - 1)) / (float)NFRAMES;
        local scale = (NFRAMES - (frame - 1)) / NFRAMES
        
        ri:Rotate(FRAMEROT * frame, 0.0, 0.0, 1.0)
        ri:Surface(ri.PLASTIC)
        ColorSpheres(ri, NSPHERES, scale)
        
        ri:WorldEnd()
        ri:FrameEnd()
    end

    ri:End()
end

main()
