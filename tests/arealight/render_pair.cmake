# Spec 057 (tasks.md T009): render pairs of scenes and compare their images byte for byte (cmake -P script).
#
#   cmake -DORENDER=<orender> -DPAIRS="<sceneA>|<sceneB>,<sceneA2>|<sceneB2>" -DEXPECT=EQUAL|DIFFERENT
#         [-DTHREADS=<n>] [-DHIDER=<name>] [-DSHADERFORMAT=<rslo|slo>] [-DIMGDIFF=<imgdiff.py>] [-DPYTHON=<python3>]
#         -P render_pair.cmake
#
# Every scene is a RIB whose Display writes "<scene file name without .rib>.tif" into the working directory (the
# one the test runs in). The two scenes of a pair are rendered one after the other (the same scene twice is allowed:
# the first image is moved aside before the second render), then compared with `cmake -E compare_files`.
#   EXPECT=EQUAL      passes when every pair is byte-identical
#   EXPECT=DIFFERENT  passes when every pair differs
# Every pair is rendered and reported before the verdict, so one failing run shows all of them. THREADS defaults to 1
# (-t:1). HIDER and SHADERFORMAT, when given, are applied to a copy of each scene, never to the tracked scene: the
# scene's Hider name is replaced, and Option "shaderformat" "default" is added after the version line. The environment
# (SHADERS, ORENDERHOME, DISPLAYS) comes from the test. This script calls no git (tests never do).

foreach(_v ORENDER PAIRS EXPECT)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "render_pair.cmake: ${_v} is not set")
    endif()
endforeach()
if(NOT EXPECT STREQUAL "EQUAL" AND NOT EXPECT STREQUAL "DIFFERENT")
    message(FATAL_ERROR "render_pair.cmake: EXPECT must be EQUAL or DIFFERENT, not '${EXPECT}'")
endif()
if(NOT DEFINED THREADS)
    set(THREADS 1)
endif()

# render_one(<scene> <tag> <out_var>): renders the scene, moves the image to <scene stem>.<tag>.tif, returns its path
function(render_one scene tag out_var)
    get_filename_component(_stem "${scene}" NAME_WE)
    set(_src "${scene}")
    if(DEFINED HIDER OR DEFINED SHADERFORMAT)
        file(READ "${scene}" _body)
        if(DEFINED HIDER)
            # the scene's own Hider statement keeps its parameters; only the hider name changes
            string(REGEX REPLACE "\nHider \"[A-Za-z]+\"" "\nHider \"${HIDER}\"" _body "${_body}")
        endif()
        if(DEFINED SHADERFORMAT)
            string(REGEX REPLACE "(\nversion [^\n]*\n)" "\\1Option \"shaderformat\" \"default\" [\"${SHADERFORMAT}\"]\n" _body "${_body}")
        endif()
        set(_src "${CMAKE_CURRENT_BINARY_DIR}/${_stem}.${tag}.rib")
        file(WRITE "${_src}" "${_body}")
    endif()
    file(REMOVE "${_stem}.tif")
    execute_process(COMMAND "${ORENDER}" -t:${THREADS} "${_src}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "orender failed (${_rc}) on ${scene}\n${_out}${_err}")
    endif()
    if(NOT EXISTS "${_stem}.tif")
        message(FATAL_ERROR "orender wrote no ${_stem}.tif for ${scene}\n${_out}${_err}")
    endif()
    file(RENAME "${_stem}.tif" "${_stem}.${tag}.tif")
    set(${out_var} "${CMAKE_CURRENT_BINARY_DIR}/${_stem}.${tag}.tif" PARENT_SCOPE)
endfunction()

set(_failed 0)
set(_npairs 0)
string(REPLACE "," ";" _pair_list "${PAIRS}")
foreach(_pair IN LISTS _pair_list)
    string(REPLACE "|" ";" _p "${_pair}")
    list(GET _p 0 _a)
    list(GET _p 1 _b)
    math(EXPR _npairs "${_npairs} + 1")
    render_one("${_a}" "a${_npairs}" _imga)
    render_one("${_b}" "b${_npairs}" _imgb)
    execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${_imga}" "${_imgb}" RESULT_VARIABLE _same)
    get_filename_component(_na "${_a}" NAME)
    get_filename_component(_nb "${_b}" NAME)
    if(_same EQUAL 0)
        set(_state "identical")
    else()
        set(_state "DIFFERENT")
        if(DEFINED IMGDIFF AND DEFINED PYTHON)
            execute_process(COMMAND "${PYTHON}" -I "${IMGDIFF}" "${_imga}" "${_imgb}"
                OUTPUT_VARIABLE _detail OUTPUT_STRIP_TRAILING_WHITESPACE)
            string(APPEND _state " (${_detail})")
        endif()
    endif()
    message("pair ${_npairs}: ${_na} | ${_nb}: ${_state}")
    if((EXPECT STREQUAL "EQUAL" AND NOT _same EQUAL 0) OR (EXPECT STREQUAL "DIFFERENT" AND _same EQUAL 0))
        math(EXPR _failed "${_failed} + 1")
    endif()
endforeach()

if(_failed GREATER 0)
    message(FATAL_ERROR "${_failed} of ${_npairs} pairs not ${EXPECT}")
endif()
message("all ${_npairs} pairs ${EXPECT}")
