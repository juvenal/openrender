-- Spec 035 (closure tree and BSDF), Phase 4 (T093), Bxdf_Bindings_lua (SC-008): the Lua RIB helper writes the Bxdf statement as the RIB
-- hand-written statement reads and as ribOut writes it: `Bxdf "name" "handle" <parameters>`, the handle omitted when nil, and its place
-- relative to Surface kept (the call order is the file order).
--
-- New file; the approved tests/lua/test_prman.lua stays as it is. Registered in CTest (labels `helpers`, `bxdf`) behind find_program(lua); runs
-- on Lua 5.4 (Linux) and 5.5 (macOS): plain Lua only. The helper directory comes from the environment (PRMAN_LUA_DIR, default src/lua under the
-- current directory); the RIB files are written to the working directory and removed afterwards.
--
-- Parameter lists are single-key tables: Lua's `pairs` order is unspecified, so a two-key table would make the written order depend on the
-- interpreter.
--
-- Red until Phase 4's helper task: prman.Ri has no Bxdf method (attempt to call a nil value).
package.path = (os.getenv("PRMAN_LUA_DIR") or "src/lua") .. "/?.lua;" .. package.path
local prman = require("prman")

local function read_back(path)
    local f = io.open(path, "r")
    assert(f, "not generated: " .. path)
    local content = f:read("*all")
    f:close()
    os.remove(path)
    return content
end

local function line_index(content, line)
    local n = 0
    for l in content:gmatch("[^\r\n]+") do
        n = n + 1
        if (l:gsub("^%s+", "")) == line then
            return n
        end
    end
    return nil
end

local function test_bxdf_statements()
    local target = "bxdf_helper_lua.rib"
    local ri = prman.Ri:new()
    ri:Begin(target)
    ri:Bxdf("diffuse")
    ri:Bxdf("plastic", "h1")
    ri:Bxdf("plastic", nil, { ["float Kd"] = 0.25 })
    ri:Bxdf("conductor", "m2", { ["float Ks"] = 0.5 })
    ri:Bxdf("diffuse", nil, { ["string note"] = "x" })
    ri:Surface("matte")
    ri:Bxdf("dielectric")
    ri:End()
    local content = read_back(target)
    print("--- Generated Lua RIB ---")
    print(content)
    print("-------------------------")

    assert(line_index(content, 'Bxdf "diffuse"'), content)
    assert(line_index(content, 'Bxdf "plastic" "h1"'), content)
    assert(line_index(content, 'Bxdf "plastic" "float Kd" [0.25]'), content)
    assert(line_index(content, 'Bxdf "conductor" "m2" "float Ks" [0.5]'), content)
    assert(line_index(content, 'Bxdf "diffuse" "string note" ["x"]'), content)
    assert(line_index(content, 'Bxdf "diffuse"') < line_index(content, 'Surface "matte"'), content)
    assert(line_index(content, 'Surface "matte"') < line_index(content, 'Bxdf "dielectric"'), content)
end

test_bxdf_statements()
print("test_prman_bxdf: ok")
