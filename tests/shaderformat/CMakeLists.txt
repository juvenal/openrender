# Spec 033: ShaderFormat_Precedence, the characterisation of which backend renders a shader name for every combination
# of the three inputs (the attribute "shade" "shaderformat", the option "shaderformat" "default" and the compile-time
# default) and of the files on disk (research R17, FR-026). Recorded on the unchanged tree and never edited:
# tests/shaderformat/reference/{macos,linux}/precedence.txt (JIT) and precedence-nojit.txt (JIT-less tree).
#
# The fixture pair of one shader name is compiled from two sources that set different constant colours (the .rslo from
# fmtp_red.sl, the .slo from fmtp_green.sl), so the pixel of a 4x4 render tells which backend ran. The driver stages the
# files of each case in a directory of its own, writes a scene, runs orender and records the pixels and the
# diagnostics (run_precedence.cmake). A missing reference is recorded on the first run and never overwritten.

set(SHADERFORMAT_FIX ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
file(MAKE_DIRECTORY ${SHADERFORMAT_FIX})

add_executable(shaderformat_read_pixel read_pixel.cpp)
target_link_libraries(shaderformat_read_pixel PRIVATE TIFF::TIFF)
target_compile_features(shaderformat_read_pixel PRIVATE cxx_std_20)

set(_fix_outputs ${SHADERFORMAT_FIX}/fmtp_red.rslo)
add_custom_command(
    OUTPUT ${SHADERFORMAT_FIX}/fmtp_red.rslo
    COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fmtp_red.sl
            -DOUT=${SHADERFORMAT_FIX}/fmtp_red.rslo -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes
            -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fmtp_red.sl oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
    WORKING_DIRECTORY ${SHADERFORMAT_FIX}
    COMMENT ""
    VERBATIM)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _fix_outputs ${SHADERFORMAT_FIX}/fmtp_green.slo)
    add_custom_command(
        OUTPUT ${SHADERFORMAT_FIX}/fmtp_green.slo
        COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fmtp_green.sl
                -DOUT=${SHADERFORMAT_FIX}/fmtp_green.slo -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes -DJIT=ON
                -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fmtp_green.sl oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        WORKING_DIRECTORY ${SHADERFORMAT_FIX}
        COMMENT ""
        VERBATIM)
endif()
add_custom_target(shaderformat_fixtures ALL DEPENDS ${_fix_outputs})

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_sf_platform macos)
else()
    set(_sf_platform linux)
endif()
if(OPENRENDER_HAVE_LLVM)
    set(_sf_reference precedence.txt)
    set(_sf_jit ON)
else()
    set(_sf_reference precedence-nojit.txt)
    set(_sf_jit OFF)
endif()

# the shadeop fixture module the missing-operand case compiles against (tests/plugins/shadeop, built by ALL)
set(_sf_dso_dir ${CMAKE_BINARY_DIR}/tests/plugins/shadeop/good)

add_test(NAME ShaderFormat_Precedence
    COMMAND ${CMAKE_COMMAND}
        -DORENDER=$<TARGET_FILE:orender> -DOSHADER=$<TARGET_FILE:oshader> -DREAD_PIXEL=$<TARGET_FILE:shaderformat_read_pixel>
        -DFIX=${SHADERFORMAT_FIX} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/work
        -DREF=${CMAKE_CURRENT_SOURCE_DIR}/reference/${_sf_platform}/${_sf_reference}
        -DJIT=${_sf_jit} -DDISPLAYS=$<TARGET_FILE_DIR:file> -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DDSO_PROBE_SL=${CMAKE_SOURCE_DIR}/tests/plugins/shadeop/probe.sl -DDSO_DIR=${_sf_dso_dir}
        -DSHIPPED_SHADERS=${CMAKE_BINARY_DIR}/shaders -DORENDERHOME=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_precedence.cmake)
set_tests_properties(ShaderFormat_Precedence PROPERTIES
    TIMEOUT 900 LABELS "shaderformat;regression" DEPENDS "orender;oshader")
add_dependencies(shaderformat_fixtures oshader)
