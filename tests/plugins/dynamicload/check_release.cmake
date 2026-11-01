# ProceduresAbi_DynamicLoadRelease (FR-034). Run as
#   cmake -DORENDER=<orender> -DRIB=<file> -DWORKDIR=<dir> -DLOG=<file> -DCOMMON=<dir> -P this
#
# Two frames, each expanding the nested generator. Asserts, from the fixture's log, that frame 1's
# module is unloaded before frame 2's first entry point (ConvertParameters). "An unload line exists"
# proves nothing: a dlopen'd image runs its destructors at process exit anyway.
#
# Red on the starting tree: RiProcDynamicLoad's osUnloadModule is commented out (ri.cpp), so the
# destructor runs at exit, after frame 2.
include(${COMMON}/render.cmake)
set(_or_failures 0)

or_render(ORENDER "${ORENDER}" RIB "${RIB}" WORKDIR "${WORKDIR}" LOG "${LOG}" RESULT _rc OUTPUT _out)
or_log_read("${LOG}" _lines)

or_rc_is_status("${_rc}" _is_status)
if(NOT _is_status OR NOT _rc EQUAL 0)
    or_fail("PRECONDITION: orender exited with '${_rc}'")
endif()
or_log_count(_converts "nested:ConvertParameters" ${_lines})
or_log_count(_nested "nested:nestedSubdivide" ${_lines})
if(NOT _converts EQUAL 2)
    or_fail("PRECONDITION: expected the generator to be expanded twice (ConvertParameters x2), got ${_converts}")
endif()
if(NOT _nested EQUAL 2)
    or_fail("PRECONDITION: expected the nested procedural to run twice (nestedSubdivide x2), got ${_nested}")
endif()

or_log_check_order(_why "nested:ConvertParameters;nested:unload;nested:ConvertParameters" ${_lines})
if(NOT _why STREQUAL "")
    or_fail("DEFECT FR-034: frame 1's module is not unloaded before frame 2 uses it again: ${_why}")
endif()

if(_or_failures GREATER 0)
    string(REPLACE ";" "\n  " _dump "${_lines}")
    message(FATAL_ERROR "ProceduresAbi_DynamicLoadRelease: ${_or_failures} failure(s).\norender output:\n${_out}\nFixture log:\n  ${_dump}")
endif()
