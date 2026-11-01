# DisplayAbi_ReleaseAfterFinish_V2 (FR-035, US3-6b). Run as
#   cmake -DORENDER=<orender> -DRIB=<file> -DWORKDIR=<dir> -DLOG=<file> -DCOMMON=<dir> -P this
#
# T017's two-frame ordering with the version 2 fixture: frame 1's finish, then the module's unload, then
# frame 2's start, then its finish. Red until the display host unloads at finishAll.
include(${COMMON}/render.cmake)
set(_or_failures 0)

or_render(ORENDER "${ORENDER}" RIB "${RIB}" WORKDIR "${WORKDIR}" LOG "${LOG}" RESULT _rc OUTPUT _out)
or_log_read("${LOG}" _lines)

or_rc_is_status("${_rc}" _is_status)
if(NOT _is_status OR NOT _rc EQUAL 0)
    or_fail("PRECONDITION: orender exited with '${_rc}'\n${_out}")
endif()
or_log_count(_starts "v2:start " ${_lines})
or_log_count(_finishes "v2:finish" ${_lines})
if(NOT _starts EQUAL 2 OR NOT _finishes EQUAL 2)
    or_fail("PRECONDITION: expected 2 start and 2 finish, got ${_starts} and ${_finishes}")
endif()

or_log_check_order(_why "v2:start ;v2:finish;v2:unload;v2:start ;v2:finish" ${_lines})
if(NOT _why STREQUAL "")
    or_fail("DEFECT FR-035: the version 2 module is not released between the frames: ${_why}")
endif()

if(_or_failures GREATER 0)
    string(REPLACE ";" "\n  " _dump "${_lines}")
    message(FATAL_ERROR "DisplayAbi_ReleaseAfterFinish_V2: ${_or_failures} failure(s). Fixture log:\n  ${_dump}")
endif()
