# run_scenes.cmake -- renders a list of scenes and compares (or captures) their decoded pixels.
# Run as
#   cmake -DMODE=compare|capture -DORENDER=<orender> -DTOOL=<plugin_abi_pixel_compare>
#         -DSCENES="<name>=<output file>|..." -DRIB_DIR=<dir of <name>.rib> -DWORKROOT=<dir>
#         -DREF_ROOT=<reference dir holding macos/ and linux/> -DPLATFORM=macos|linux
#         [-DGAMMA_SCENES="<name>|..."] [-DRECAPTURE=ON|OFF] [-DREF_DIR=<capture target dir>]
#         [-DLABEL=<name>] -P run_scenes.cmake
#
# compare   Renders each scene and requires the decoded pixels to equal <ref>/<name>.pix, with zero
#           differing samples. <ref> is REF_ROOT/PLATFORM; when that directory does not exist the
#           macOS references are used and the output says so (a Linux reference exists only if the
#           macOS one differed, T031e). A GAMMA_SCENES output is a PNG whose gAMA chunk must equal the
#           recorded value in <ref>/<name>.gamma.
# capture   Renders each scene and writes <ref>/<name>.pix (and .gamma). <ref> is REF_DIR when given,
#           else REF_ROOT/PLATFORM. It REFUSES to overwrite an existing reference unless RECAPTURE is
#           ON (configure with -DOR037_RECAPTURE=ON), so no ctest run can turn a post-change output
#           into its own baseline. It is run by the plugin-abi-capture-baselines build target only.
#
# Every scene runs even after an earlier one failed, so one run reports all failures.
include(${CMAKE_CURRENT_LIST_DIR}/../common/render.cmake)
set(_or_failures 0)
if(NOT LABEL)
    set(LABEL scenes)
endif()

if(MODE STREQUAL "capture")
    if(REF_DIR)
        set(_ref "${REF_DIR}")
    else()
        set(_ref "${REF_ROOT}/${PLATFORM}")
    endif()
else()
    set(_ref "${REF_ROOT}/${PLATFORM}")
    if(NOT EXISTS "${_ref}")
        set(_ref "${REF_ROOT}/macos")
        message("${LABEL}: no reference/${PLATFORM}; comparing against reference/macos")
    else()
        message("${LABEL}: comparing against reference/${PLATFORM}")
    endif()
endif()
file(MAKE_DIRECTORY "${_ref}")

string(REPLACE "|" ";" _scenes "${SCENES}")
string(REPLACE "|" ";" _gamma_scenes "${GAMMA_SCENES}")

foreach(_entry IN LISTS _scenes)
    string(REPLACE "=" ";" _parts "${_entry}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _file)
    set(_work "${WORKROOT}/${_name}")
    file(REMOVE_RECURSE "${_work}")

    or_render(ORENDER "${ORENDER}" RIB "${RIB_DIR}/${_name}.rib" WORKDIR "${_work}" LOG "${_work}/fixture.log" RESULT _rc OUTPUT _out)
    or_rc_is_status("${_rc}" _is_status)
    if(NOT _is_status OR NOT _rc EQUAL 0)
        or_fail("${_name}: orender exited with '${_rc}'\n${_out}")
        continue()
    endif()
    if(NOT EXISTS "${_work}/${_file}")
        or_fail("${_name}: the scene did not write ${_file}\n${_out}")
        continue()
    endif()

    if(MODE STREQUAL "capture")
        if(EXISTS "${_ref}/${_name}.pix" AND NOT RECAPTURE)
            or_fail("${_name}: ${_ref}/${_name}.pix exists; refusing to overwrite (configure with -DOR037_RECAPTURE=ON to recapture on purpose)")
            continue()
        endif()
        execute_process(COMMAND "${TOOL}" decode "${_work}/${_file}" "${_ref}/${_name}.pix" RESULT_VARIABLE _rc OUTPUT_VARIABLE _tool_out ERROR_VARIABLE _tool_out)
        if(NOT _rc EQUAL 0)
            or_fail("${_name}: cannot decode ${_file}: ${_tool_out}")
            continue()
        endif()
        message("${LABEL}: captured ${_name}")
    else()
        if(NOT EXISTS "${_ref}/${_name}.pix")
            or_fail("${_name}: no reference ${_ref}/${_name}.pix")
            continue()
        endif()
        execute_process(COMMAND "${TOOL}" compare "${_work}/${_file}" "${_ref}/${_name}.pix" RESULT_VARIABLE _rc OUTPUT_VARIABLE _tool_out ERROR_VARIABLE _tool_out)
        string(STRIP "${_tool_out}" _tool_out)
        if(NOT _rc EQUAL 0)
            or_fail("${_name}: ${_tool_out}")
        else()
            message("${LABEL}: ${_name}: ${_tool_out}")
        endif()
    endif()

    list(FIND _gamma_scenes "${_name}" _is_gamma)
    if(NOT _is_gamma EQUAL -1)
        execute_process(COMMAND "${TOOL}" png-gamma "${_work}/${_file}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _gamma ERROR_VARIABLE _gamma)
        string(STRIP "${_gamma}" _gamma)
        if(NOT _rc EQUAL 0)
            or_fail("${_name}: cannot read the gAMA chunk: ${_gamma}")
        elseif(MODE STREQUAL "capture")
            file(WRITE "${_ref}/${_name}.gamma" "${_gamma}\n")
        else()
            file(STRINGS "${_ref}/${_name}.gamma" _expected LIMIT_COUNT 1)
            if(NOT "${_gamma}" STREQUAL "${_expected}")
                or_fail("${_name}: PNG gAMA chunk is ${_gamma}, reference ${_expected}")
            else()
                message("${LABEL}: ${_name}: gAMA ${_gamma}")
            endif()
        endif()
    endif()
endforeach()

if(_or_failures GREATER 0)
    message(FATAL_ERROR "${LABEL}: ${_or_failures} scene(s) failed")
endif()
