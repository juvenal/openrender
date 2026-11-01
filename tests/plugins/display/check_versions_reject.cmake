# DisplayAbi_Versions_Reject (SC-001, SC-013). Run as
#   cmake -DORENDER=<orender> -DRIBDIR=<dir of <case>.rib> -DWORKROOT=<dir> -DCOMMON=<dir> -P this
#
# A display module whose interface MAJOR the host does not support is refused with one message that names
# the file, the interface, the module's version, the host's version and the remedy, and none of its entry
# points is entered: v3 (3.0.0), v0 (MAJOR 0) and garbage (0xFFFFFFFF). The fixtures log displayStart,
# displayData, displayRawData, displayFinish and orDisplayPlugin; only the unload may appear. Red on the
# starting tree: no version is read, the module is called as a v1 driver.
set(_or_failures 0)
include(${CMAKE_CURRENT_LIST_DIR}/display_cases.cmake)

foreach(_case "v3|3\\.0\\.0" "v0|0\\.9\\.0" "garbage|65535\\.255\\.255")
    string(REPLACE "|" ";" _fields "${_case}")
    list(GET _fields 0 _name)
    list(GET _fields 1 _version)
    run_display_case(${_name} RIB "${RIBDIR}/${_name}.rib" TAG ${_name} STARTS 0 MESSAGES 1
        PARTS "${_name}\\.dsply" "display" "${_version}" "2\\.0\\.0" "rebuild against the installed header")
    or_log_count(_entered "${_name}:(displayStart|displayData|displayRawData|displayFinish|orDisplayPlugin)" ${_dc_lines})
    if(NOT _entered EQUAL 0)
        or_fail("${_name}: ${_entered} entry point call(s) of a rejected module (SC-001)")
    endif()
endforeach()

if(_or_failures GREATER 0)
    message(FATAL_ERROR "DisplayAbi_Versions_Reject: ${_or_failures} failure(s).")
endif()
