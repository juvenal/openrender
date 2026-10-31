# DisplayV1_LegacyLoads (SC-013, US1-6). Run as
#   cmake -DORENDER=<orender> -DTOOL=<plugin_abi_pixel_compare> -DRIB=<legacy tiff8.rib> -DREF=<tiff8.pix>
#         -DWORKDIR=<dir> -DLOG=<unused> -DCOMMON=<dir> -P this
#
# The frozen pre-spec file driver (unversioned, built from the base tree's sources) renders the tiff8 baseline
# scene: the decoded pixels are those of the pre-change reference AND exactly one legacy warning is printed,
# naming the file, 1.0.0 and the release 1.2.0. The pixels half is green on the starting tree
# (DisplayAbi_LegacyPixelsIdentical); the warning is what is missing there.
include(${COMMON}/render.cmake)
set(_or_failures 0)

or_render(ORENDER "${ORENDER}" RIB "${RIB}" WORKDIR "${WORKDIR}" LOG "${LOG}" RESULT _rc OUTPUT _out)
or_rc_is_status("${_rc}" _is_status)
if(NOT _is_status OR NOT _rc EQUAL 0)
    or_fail("PRECONDITION: orender ended with '${_rc}'\n${_out}")
endif()

string(REGEX MATCHALL "the display plug-in" _found "${_out}")
list(LENGTH _found _messages)
if(NOT _messages EQUAL 1)
    or_fail("${_messages} display plug-in diagnostic(s), expected exactly one legacy warning:\n${_out}")
else()
    foreach(_part "file\\.dsply" "1\\.0\\.0" "1\\.2\\.0")
        if(NOT "${_out}" MATCHES "${_part}")
            or_fail("the legacy warning does not match \"${_part}\":\n${_out}")
        endif()
    endforeach()
endif()

execute_process(COMMAND "${TOOL}" compare "${WORKDIR}/tiff8.tif" "${REF}" RESULT_VARIABLE _same OUTPUT_VARIABLE _cmp ERROR_VARIABLE _cmp)
if(NOT _same EQUAL 0)
    or_fail("the pixels differ from the pre-change reference: ${_cmp}")
endif()

if(_or_failures GREATER 0)
    message(FATAL_ERROR "DisplayV1_LegacyLoads: ${_or_failures} failure(s).")
endif()
