# PluginAbi_ShadeopSoversion (FR-048, SC-019). Run as
#   cmake -DEXPECTED=<OPENRENDER_COMPAT_SOVERSION> -DSHADEOP_SOVERSION=<libRIBShadeop's SOVERSION property>
#         -DSHADEOP_SONAME=<its soname file name> -DCOMMON_SOVERSION=<libRIBCommon's> -DCOMMON_SONAME=<its> -P this
#
# libRIBShadeop carries OPENRENDER_COMPAT_SOVERSION like the other libRIB* libraries, and the soname file name
# says so. The plug-in interface versions (display 2.0.0, shadeop 1.1.0, ...) are independent of it: adding the
# library and the display 2 MAJOR did not change the SOVERSION.
set(_failures "")
if(NOT "${SHADEOP_SOVERSION}" STREQUAL "${EXPECTED}")
    list(APPEND _failures "libRIBShadeop's SOVERSION is '${SHADEOP_SOVERSION}', OPENRENDER_COMPAT_SOVERSION is '${EXPECTED}'")
endif()
if(NOT "${COMMON_SOVERSION}" STREQUAL "${EXPECTED}")
    list(APPEND _failures "libRIBCommon's SOVERSION is '${COMMON_SOVERSION}', OPENRENDER_COMPAT_SOVERSION is '${EXPECTED}'")
endif()
if(NOT SHADEOP_SONAME MATCHES "(^|[.])${EXPECTED}([.]|$)")
    list(APPEND _failures "the soname file ${SHADEOP_SONAME} does not carry the SOVERSION ${EXPECTED}")
endif()
string(REPLACE "RIBShadeop" "RIBCommon" _as_common "${SHADEOP_SONAME}")
if(NOT "${_as_common}" STREQUAL "${COMMON_SONAME}")
    list(APPEND _failures "the soname file names differ in more than the library name: ${SHADEOP_SONAME} and ${COMMON_SONAME}")
endif()
if(_failures)
    string(REPLACE ";" "\n  " _report "${_failures}")
    message(FATAL_ERROR "DEFECT SC-019:\n  ${_report}")
endif()
message("libRIBShadeop: SOVERSION ${SHADEOP_SOVERSION}, ${SHADEOP_SONAME}")
