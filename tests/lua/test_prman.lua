local prman = require("prman")

local function test_helloworld()
    local ri = prman.Ri:new()
    local rendertarget = "helloworld_lua.rib"
    
    print("Generating " .. rendertarget .. "...")
    ri:Begin(rendertarget)
    
    ri:Display('helloworld', 'framebuffer', 'rgb')
    ri:Format(640, 480, 1)
    ri:Projection(ri.PERSPECTIVE, { [ri.FOV] = 90 })
    ri:Translate(0, 0, 10)
    
    ri:WorldBegin()
        ri:Color({1.0, 0, 0})
        ri:Translate(0, -1.5, 0)
        ri:Rotate(-110, 1, 0, 0)
        ri:Sphere(1, -1, 1, 360)
    ri:WorldEnd()
    
    ri:End()
    print("Done.")

    local f = io.open(rendertarget, "r")
    if f then
        local content = f:read("*all")
        f:close()
        print("--- Generated Lua RIB ---")
        print(content)
        print("-------------------------")
        
        assert(content:find('Display "helloworld" "framebuffer" "rgb"'))
        assert(content:find('Format 640 480 1'))
        assert(content:find('Projection "perspective" "fov" %[90%]'))
        assert(content:find('WorldBegin'))
        assert(content:find('  Sphere 1 %-1 1 360'))
        assert(content:find('WorldEnd'))
        print("Verification successful.")
        os.remove(rendertarget)
    else
        print("Error: " .. rendertarget .. " not generated.")
        os.exit(1)
    end
end

local function test_pipe()
    local ri = prman.Ri:new()
    print("Testing pipe to 'sh -c cat > test_pipe_lua.rib'...")
    
    ri:Begin("sh", "-c 'cat > test_pipe_lua.rib'")
    ri:Display("pipe_test", "file", "rgba")
    ri:End()
    
    local f = io.open("test_pipe_lua.rib", "r")
    if f then
        local content = f:read("*all")
        f:close()
        print("--- Pipe Output ---")
        print(content)
        print("-------------------")
        assert(content:find('Display "pipe_test" "file" "rgba"'))
        print("Pipe test successful.")
        os.remove("test_pipe_lua.rib")
    else
        print("Error: test_pipe_lua.rib not generated via pipe.")
        os.exit(1)
    end
end

local function test_comprehensive()
    local ri = prman.Ri:new()
    local rendertarget = "comprehensive_lua.rib"
    ri:Begin(rendertarget)
    
    ri:Option("limits", {["bucketsize"] = {16, 16}, ["gridsize"] = 32})
    ri:Display("test", "file", "rgba")
    ri:Format(1024, 768, 1)
    
    ri:TransformBegin()
    ri:Translate(1, 2, 3)
    ri:Rotate(45, 0, 1, 0)
    
    local handle = ri:ObjectBegin()
    ri:Sphere(1, -1, 1, 360)
    ri:ObjectEnd()
    
    ri:WorldBegin()
        ri:MotionBegin({0.0, 1.0})
        ri:Translate(0, 0, 0)
        ri:Translate(0, 1, 0)
        ri:MotionEnd()
        
        ri:SolidBegin(ri.UNION)
        ri:Sphere(1, -1, 1, 360)
        ri:Cone(2, 1, 360)
        ri:SolidEnd()
        
        ri:ObjectInstance(handle)
        ri:Geometry("teapot")
    ri:WorldEnd()
    ri:TransformEnd()
    ri:End()

    local f = io.open(rendertarget, "r")
    if f then
        local content = f:read("*all")
        f:close()
        print("--- Comprehensive Lua RIB ---")
        print(content)
        print("-----------------------------")
        
        assert(content:find('TransformBegin'))
        assert(content:find('  Translate 1 2 3'))
        assert(content:find('ObjectBegin 1'))
        assert(content:find('ObjectEnd'))
        assert(content:find('MotionBegin %[0.0 1.0%]'))
        assert(content:find('SolidBegin "union"'))
        assert(content:find('ObjectInstance 1'))
        assert(content:find('Geometry "teapot"'))
        print("Comprehensive test successful.")
        os.remove(rendertarget)
    else
        print("Error: " .. rendertarget .. " not generated.")
        os.exit(1)
    end
end

test_helloworld()
test_pipe()
test_comprehensive()
