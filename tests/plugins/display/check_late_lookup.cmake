# DisplayV1_LateLookup (FR-039). Run as
#   cmake -DORENDER=<orender> -DRIB=<file> -DWORKDIR=<dir> -DLOG=<file> -DCOMMON=<dir> -P this
#
# Renders a scene whose display line names the v1_late_lookup fixture, which looks up
# quantize, dither, near and type from displayData (after displayStart returned) and logs
# NULL or non-NULL for each. A lookup outside displayStart has no current display, so the
# documented answer is "not found": all four must be NULL.
#
# Red on the starting tree: findParameter answers from CRenderer's statics and from the stale
# "current display" it kept, so all four come back non-NULL.
include(${COMMON}/render.cmake)
set(_or_failures 0)

or_render(ORENDER "${ORENDER}" RIB "${RIB}" WORKDIR "${WORKDIR}" LOG "${LOG}" RESULT _rc OUTPUT _out)
or_log_read("${LOG}" _lines)

or_rc_is_status("${_rc}" _is_status)
if(NOT _is_status OR NOT _rc EQUAL 0)
    or_fail("PRECONDITION: orender exited with '${_rc}'\n${_out}")
endif()
foreach(_name quantize dither near type)
    or_log_count(_any "v1_late_lookup:late_${_name}=" ${_lines})
    if(_any EQUAL 0)
        or_fail("PRECONDITION: the fixture logged no late lookup of '${_name}'")
    endif()
endforeach()

foreach(_name quantize dither near type)
    or_log_count(_non_null "v1_late_lookup:late_${_name}=non-NULL" ${_lines})
    if(NOT _non_null EQUAL 0)
        or_fail("DEFECT FR-039: late lookup of '${_name}' returned non-NULL (${_non_null} time(s)); expected NULL")
    endif()
endforeach()

if(_or_failures GREATER 0)
    string(REPLACE ";" "\n  " _dump "${_lines}")
    message(FATAL_ERROR "DisplayV1_LateLookup: ${_or_failures} failure(s). Fixture log:\n  ${_dump}")
endif()
