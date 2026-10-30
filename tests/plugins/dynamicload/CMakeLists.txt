# DynamicLoad fixtures and the FR-034 check (spec 037).
#
#   plain    ConvertParameters / Subdivide / Free; Subdivide emits one sphere
#   nested   Subdivide emits a procedural whose subdivide and free functions live in the module
#
# Both modules call back into the host (RiSphere, RiProcedural). They link nothing: the symbols
# resolve against the process that loads them, so the renderer's process-wide state stays one
# copy (CLAUDE.md gotcha 22). The generator name is loaded exactly as written (research Q4).

function(or_dynload_module target variant)
    or_plugin_test_module(${target} dynamicload ${variant} dynload_fixture.c ${ARGN})
    if(APPLE)
        target_link_options(${target} PRIVATE -Wl,-undefined,dynamic_lookup)
    endif()
endfunction()

or_dynload_module(plugin_dynamicload_plain plain)
or_dynload_module(plugin_dynamicload_nested nested DEFS TEST_NESTED)

set(DYNLOAD_NESTED_FILE ${OR_PLUGIN_BIN}/dynamicload/nested/nested.dsply)
or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/two_frames.rib.in ${OR_PLUGIN_BIN}/dynamicload/two_frames.rib)

# FR-034 -- two frames, each expanding the nested generator; frame 1's module is unloaded
# before frame 2's first entry point.
or_plugin_add_test(ProceduresAbi_DynamicLoadRelease 120
    COMMAND ${CMAKE_COMMAND}
        -DORENDER=$<TARGET_FILE:orender>
        -DRIB=${OR_PLUGIN_BIN}/dynamicload/two_frames.rib
        -DWORKDIR=${OR_PLUGIN_BIN}/dynamicload/work/two_frames
        -DLOG=${OR_PLUGIN_BIN}/dynamicload/work/two_frames.log
        -DCOMMON=${OR_PLUGIN_COMMON_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_release.cmake)
set_tests_properties(ProceduresAbi_DynamicLoadRelease PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")
