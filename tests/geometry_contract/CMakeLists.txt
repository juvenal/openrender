# Contract tests of spec 031 (geometry, hider-agnostic): `ctest --test-dir build -L geometry_contract`.
#
# Each program is its own executable (a case that cannot compile must not break an approved one). They were defined
# only while the temporary option OPENRENDER_031_RED_TESTS was ON (red on the starting tree, each joining the normal
# build in the phase that made it green); the option went with the lens phase (tasks T128) and all five are enabled.
#
#   Geometry_DiceThroughDicer, Geometry_IntersectThroughRayContext, Geometry_ExpansionThroughHost
#       link libRIBVector and libRIBCommon only: a program with no hider and no Shading
#   Geometry_ObjectSink, Geometry_FrameDefault
#       renderer build (CSG solid instances, renderer.h's camera2pixels, a raytrace hider's constructors)

set(GEOMETRY_CONTRACT_INCLUDES
    ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/render
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_SOURCE_DIR}/src/ri/geometry
    ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests/unit/csg
    ${CMAKE_CURRENT_SOURCE_DIR})

# contract_program(<name> <source> <ctest name> <link: vector|render>)
function(contract_program name source test_name flavour)
    add_executable(${name} ${source})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_include_directories(${name} PRIVATE ${GEOMETRY_CONTRACT_INCLUDES})
    if(flavour STREQUAL "vector")
        target_link_libraries(${name} PRIVATE ribVector openrendercommon)
    else()
        target_link_libraries(${name} PRIVATE ribRender openrendercommon)
    endif()
    add_test(NAME ${test_name} COMMAND ${name})
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 60 LABELS "geometry_contract;unit"
        ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")
endfunction()

# Enabled in Phase 3 (the interfaces exist from there on)
contract_program(test_geometry_object_sink test_object_sink.cpp Geometry_ObjectSink render)
contract_program(test_geometry_frame_default test_frame_default.cpp Geometry_FrameDefault render)

# Enabled in Phase 6 (libRIBVector holds the real Geometry: no stub, no hider)
contract_program(test_geometry_dice_through_dicer test_dice_through_dicer.cpp Geometry_DiceThroughDicer vector)
contract_program(test_geometry_intersect_through_ray_context test_intersect_through_ray_context.cpp Geometry_IntersectThroughRayContext vector)
contract_program(test_geometry_expansion_through_host test_expansion_through_host.cpp Geometry_ExpansionThroughHost vector)

# Every test of this file is enabled.
