# ShadeopAbi_MissingExec (FR-033). Run as
#   cmake -DORENDER=<orender> -DRIB=<file> -DWORKDIR=<dir> -DLOG=<file> -DCOMMON=<dir> -P this
#
# probe.rslo was compiled against directory A (table without exec) and B (full operand); the
# scene searches A then B. FR-033: a table entry whose exec symbol does not resolve must be recognised
# as unresolved, skipped *with a diagnostic naming the symbol*, and the scan must go on.
# Asserts exit status 0, that has_exec's exec ran, that missing_exec's never did, and that a
# diagnostic names the unresolved symbol (probe_add).
#
# MEASURED on the starting tree (the research and tasks text say the null exec is called; it is not):
#   * dsoLoadCallback tests `exec != NULL` on the address of the output slot (never null), so the
#     first module whose table has the prototype ends the scan of ITS directory, resolved or not.
#     getDSO's own test is on the value (null), so an unresolved exec makes getDSO return "not found".
#   * With A and B in different directories the later directory's module overwrites the slots and the
#     operand is found: the first three assertions above are GREEN on the starting tree. With only A
#     the shader fails with the generic "Failed to locate DSO function"; nothing is called.
#   * No diagnostic names the unresolved symbol in either case.
# So the assertion that can fail on the starting tree is the diagnostic one; the others characterize
# the behaviour that must be kept (they pass before and after). Reported at the Phase 1 approval stop.
include(${COMMON}/render.cmake)
set(_or_failures 0)

or_render(ORENDER "${ORENDER}" RIB "${RIB}" WORKDIR "${WORKDIR}" LOG "${LOG}" RESULT _rc OUTPUT _out)
or_log_read("${LOG}" _lines)

# A crash is reported by cmake as text (for example "Segmentation fault"), not a number.
or_rc_is_status("${_rc}" _is_status)
if(NOT _is_status)
    message("SIGNAL: orender ended with '${_rc}'")
    or_fail("DEFECT FR-033: orender crashed ('${_rc}')")
elseif(NOT _rc EQUAL 0)
    or_fail("DEFECT FR-033: orender exited with status ${_rc}, expected 0")
endif()

or_log_count(_good_exec "has_exec:exec" ${_lines})
or_log_count(_bad_exec "missing_exec:exec" ${_lines})
if(_good_exec EQUAL 0)
    or_fail("DEFECT FR-033: the full operand in the second directory never ran (has_exec has no exec call)")
endif()
if(NOT _bad_exec EQUAL 0)
    or_fail("DEFECT FR-033: the module without an exec symbol ran its exec (${_bad_exec} call(s))")
endif()

# FR-033: the skip is announced, and the announcement names the symbol.
if(NOT "${_out}" MATCHES "probe_add")
    or_fail("DEFECT FR-033: no diagnostic names the unresolved exec symbol (probe_add); the render printed:\n${_out}")
endif()

if(_or_failures GREATER 0)
    string(REPLACE ";" "\n  " _dump "${_lines}")
    message(FATAL_ERROR "ShadeopAbi_MissingExec: ${_or_failures} failure(s).\norender output:\n${_out}\nFixture log:\n  ${_dump}")
endif()
