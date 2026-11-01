# ProceduresAbi_BareNameLocated (FR-032). Run as
#   cmake -DORENDER=<orender> -DRIB=<file> -DWORKDIR=<dir> -DLOG=<file> -DCOMMON=<dir> -P this
#
# Renders Geometry "dlo" "bare_name" with the module only on the procedural search path
# and a working directory that does not contain it. Asserts exit status 0 and that the
# module's log contains dloInit (the module was loaded and initialized).
#
# Red on the starting tree: CRendererContext::RiGeometryV locates the file into `location`
# and then calls osLoadModule(name), the unresolved name, which dlopen cannot find.
include(${COMMON}/render.cmake)
set(_or_failures 0)

or_render(ORENDER "${ORENDER}" RIB "${RIB}" WORKDIR "${WORKDIR}" LOG "${LOG}" RESULT _rc OUTPUT _out)
or_log_read("${LOG}" _lines)

# The module sits only on the search path: if it were also in the working directory this
# check would pass for the wrong reason.
file(GLOB _stray "${WORKDIR}/*.dsply")
if(_stray)
    or_fail("PRECONDITION: the working directory contains a module (${_stray})")
endif()

or_rc_is_status("${_rc}" _is_status)
if(NOT _is_status OR NOT _rc EQUAL 0)
    or_fail("DEFECT FR-032: orender exited with '${_rc}', expected 0")
endif()
or_log_count(_inits "bare_name:dloInit" ${_lines})
if(_inits EQUAL 0)
    or_fail("DEFECT FR-032: the module's log has no dloInit: the located file was not loaded")
endif()

if(_or_failures GREATER 0)
    string(REPLACE ";" "\n  " _dump "${_lines}")
    message(FATAL_ERROR "ProceduresAbi_BareNameLocated: ${_or_failures} failure(s).\norender output:\n${_out}\nFixture log:\n  ${_dump}")
endif()
