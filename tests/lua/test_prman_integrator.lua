-- Spec 030 (hider and integrator registry), FR-017: the Lua RIB helper writes the Integrator statement.
--
-- New file; the approved tests/lua/test_prman.lua stays as it is. Registered in CTest (label `helpers`) behind
-- find_program(lua); runs on Lua 5.4 (Linux) and 5.5 (macOS): plain Lua only, no 5.5-only syntax or library
-- function. The helper directory comes from the environment (PRMAN_LUA_DIR, default src/lua under the current
-- directory); the RIB files are written to the working directory and removed afterwards.
--
-- Parameter lists are single-key tables: Lua's `pairs` order is unspecified, so a two-key table would make the
-- written order depend on the interpreter.
--
-- Red until Phase 7: prman.Ri has no Integrator method (attempt to call a nil value).
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

local function has_line(content, line)
    for l in content:gmatch("[^\r\n]+") do
        if (l:gsub("^%s+", "")) == line then
            return true
        end
    end
    return false
end

local function test_integrator_statements()
    local target = "integrator_helper_lua.rib"
    local ri = prman.Ri:new()
    ri:Begin(target)
    ri:Integrator("classic")
    ri:Integrator("classic", { ["int maxdepth"] = 5 })
    ri:Integrator("classic", { ["string mode"] = "fast" })
    ri:Hider("reyes")
    ri:End()
    local content = read_back(target)
    print("--- Generated Lua RIB ---")
    print(content)
    print("-------------------------")

    assert(has_line(content, 'Integrator "classic"'), content)
    assert(has_line(content, 'Integrator "classic" "int maxdepth" [5]'), content)
    assert(has_line(content, 'Integrator "classic" "string mode" ["fast"]'), content)
    assert(has_line(content, 'Hider "reyes"'), content)
end

local function test_existing_hider_unchanged()
    local target = "integrator_helper_lua_hider.rib"
    local ri = prman.Ri:new()
    ri:Begin(target)
    ri:Hider("zbuffer")
    ri:End()
    local content = read_back(target)
    assert(has_line(content, 'Hider "zbuffer"'), content)
end

test_integrator_statements()
test_existing_hider_unchanged()
print("test_prman_integrator: ok")
