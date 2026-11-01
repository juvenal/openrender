# ProceduresAbi_MissingSymbol -- characterization of the host's error text for a module that lacks
# an entry point (dloTini / implicitTini). Run as
#   cmake -DMODE=compare|capture -DORENDER=<orender> -DCOMMON=<dir> -DWORKROOT=<dir>
#         -DSCENES="<name>=<rib>|..." -DREF=<data file> -DBUILD_DIR=<build dir> [-DRECAPTURE=ON|OFF]
#         -P this
#
# Renders each scene, takes orender's combined output, replaces the absolute build directory by
# <BUILD> and compares the result with the data file captured on the starting tree. Green on the
# starting tree by design; the later phases must keep the text (the diagnostics may gain nothing
# for a module the host already rejects for another reason).
#
# capture mode writes REF and refuses to overwrite it unless RECAPTURE is ON.
include(${COMMON}/render.cmake)
set(_or_failures 0)

set(_text "")
string(REPLACE "|" ";" _scenes "${SCENES}")
foreach(_entry IN LISTS _scenes)
    string(REPLACE "=" ";" _parts "${_entry}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _rib)
    or_render(ORENDER "${ORENDER}" RIB "${_rib}" WORKDIR "${WORKROOT}/${_name}" LOG "${WORKROOT}/${_name}/fixture.log" RESULT _rc OUTPUT _out)
    or_rc_is_status("${_rc}" _is_status)
    if(NOT _is_status)
        or_fail("${_name}: orender ended with '${_rc}'")
        continue()
    endif()
    string(REPLACE "${BUILD_DIR}" "<BUILD>" _out "${_out}")
    string(STRIP "${_out}" _out)
    string(APPEND _text "== ${_name} (exit ${_rc})\n${_out}\n")
endforeach()

if(_or_failures GREATER 0)
    message(FATAL_ERROR "ProceduresAbi_MissingSymbol: ${_or_failures} failure(s)")
endif()

if(MODE STREQUAL "capture")
    if(EXISTS "${REF}" AND NOT RECAPTURE)
        message(FATAL_ERROR "${REF} exists; refusing to overwrite (configure with -DOR037_RECAPTURE=ON to recapture on purpose)")
    endif()
    file(WRITE "${REF}" "${_text}")
    message("captured ${REF}")
else()
    if(NOT EXISTS "${REF}")
        message(FATAL_ERROR "no reference ${REF}")
    endif()
    file(READ "${REF}" _expected)
    if(NOT "${_text}" STREQUAL "${_expected}")
        message(FATAL_ERROR "the host's error text changed.\n--- expected (${REF})\n${_expected}\n--- actual\n${_text}")
    endif()
endif()
