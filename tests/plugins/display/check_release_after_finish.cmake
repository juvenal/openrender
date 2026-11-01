# DisplayAbi_ReleaseAfterFinish (FR-035). Run as
#   cmake -DORENDER=<orender> -DRIB=<file> -DWORKDIR=<dir> -DLOG=<file> -DCOMMON=<dir> -P this
#
# Renders a two-frame scene whose displays use the v1_legacy fixture and asserts, from the
# fixture's own call log, that frame 1's displayFinish comes before the module's unload and
# the unload before frame 2's displayStart.
#
# Red on the starting tree: endDisplays() never unloads the module (only dispatch() does, on a
# driver's FALSE return), so the destructor runs at process exit, after frame 2.
include(${COMMON}/render.cmake)
set(_or_failures 0)

or_render(ORENDER "${ORENDER}" RIB "${RIB}" WORKDIR "${WORKDIR}" LOG "${LOG}" RESULT _rc OUTPUT _out)
or_log_read("${LOG}" _lines)

# Preconditions: the scene ran, both frames used the fixture (this is not the defect).
or_rc_is_status("${_rc}" _is_status)
if(NOT _is_status OR NOT _rc EQUAL 0)
    or_fail("PRECONDITION: orender exited with '${_rc}'\n${_out}")
endif()
or_log_count(_starts "v1_legacy:displayStart" ${_lines})
or_log_count(_finishes "v1_legacy:displayFinish" ${_lines})
if(NOT _starts EQUAL 2 OR NOT _finishes EQUAL 2)
    or_fail("PRECONDITION: expected 2 displayStart and 2 displayFinish, got ${_starts} and ${_finishes}")
endif()

# The defect: frame 1 start .. finish, then unload, then frame 2's start.
or_log_check_order(_why "v1_legacy:displayStart;v1_legacy:displayFinish;v1_legacy:unload;v1_legacy:displayStart;v1_legacy:displayFinish" ${_lines})
if(NOT _why STREQUAL "")
    or_fail("DEFECT FR-035: module not released between the frames: ${_why}")
endif()

if(_or_failures GREATER 0)
    string(REPLACE ";" "\n  " _dump "${_lines}")
    message(FATAL_ERROR "DisplayAbi_ReleaseAfterFinish: ${_or_failures} failure(s). Fixture log:\n  ${_dump}")
endif()
