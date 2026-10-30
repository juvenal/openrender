# Characterization of 3D-texture channel parsing (spec 037, US7 / FR-025, T027).
#
#   Texture3dChannels_Session     "_area,Ci" parsed inside a session (what bake3d does), written and read back
#   Texture3dChannels_NoSession   ptcapi create/read with names and types and no session; the string form
#                                 with no session, which reports the unknown-channel error
#   Texture3dChannels_Unknown     an unknown channel name inside a session: the error text
#
# Each compares the program's report with a data file captured on the starting tree
# (texture3d/reference/<mode>.txt), green on the starting tree by design. The channel
# interface the spec adds to Texturing (Phase 6) must reproduce these reports.

add_executable(test_texture3d_channels test_channels.cpp)
target_link_libraries(test_texture3d_channels PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_texture3d_channels PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri
    ${CMAKE_SOURCE_DIR}/src/ri/render
    ${CMAKE_SOURCE_DIR}/src/ri/state
    ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/geometry
    ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/ri/core
    ${CMAKE_BINARY_DIR})
target_compile_features(test_texture3d_channels PRIVATE cxx_std_20)

set(_env ${CMAKE_COMMAND} -E env "SHADERS=${CMAKE_BINARY_DIR}/shaders" "ORENDERHOME=${CMAKE_SOURCE_DIR}")
foreach(_mode session nosession unknown)
    if(_mode STREQUAL "session")
        set(_test Texture3dChannels_Session)
    elseif(_mode STREQUAL "nosession")
        set(_test Texture3dChannels_NoSession)
    else()
        set(_test Texture3dChannels_Unknown)
    endif()
    set(_ref ${CMAKE_CURRENT_SOURCE_DIR}/reference/${_mode}.txt)
    set(_run
        -DEXE=$<TARGET_FILE:test_texture3d_channels>
        -DARG=${_mode}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_text_compare.cmake)

    or_plugin_add_test(${_test} 60
        COMMAND ${CMAKE_COMMAND} -DMODE=compare -DREF=${_ref} -DWORKDIR=${OR_PLUGIN_BIN}/texture3d/work/${_mode} ${_run})
    set_tests_properties(${_test} PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

    or_plugin_capture(${_test}
        OFFICIAL ${_env} ${CMAKE_COMMAND} -DMODE=capture -DRECAPTURE=${OR037_RECAPTURE} -DREF=${_ref}
            -DWORKDIR=${OR_PLUGIN_BIN}/texture3d/capture_work/${_mode} ${_run}
        SCRATCH ${_env} ${CMAKE_COMMAND} -DMODE=capture -DRECAPTURE=ON -DREF=${OR_CAPTURE_SCRATCH}/texture3d_${_mode}.txt
            -DWORKDIR=${OR_PLUGIN_BIN}/texture3d/capture_work_scratch/${_mode} ${_run})
endforeach()
foreach(_target test_texture3d_channels)
    add_dependencies(plugin-abi-capture-baselines ${_target})
    add_dependencies(plugin-abi-capture-baselines-scratch ${_target})
endforeach()
