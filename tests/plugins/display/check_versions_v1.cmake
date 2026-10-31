# DisplayAbi_Versions_V1 (SC-002). Run as
#   cmake -DORENDER=<orender> -DRIBDIR=<dir of <case>.rib> -DWORKROOT=<dir> -DCOMMON=<dir> -P this
#
# Version 1 drivers load: v1_legacy (no version symbol) prints ONE legacy warning across two displays of
# the same module, naming the file, 1.0.0 and the release 1.2.0; v1_explicit (1.0.0) prints nothing;
# v1_newer (1.4.0) prints one newer-MINOR warning naming 1.4.0. Every display starts and finishes. Red on the
# starting tree for the two warnings: no version check exists.
set(_or_failures 0)
include(${CMAKE_CURRENT_LIST_DIR}/display_cases.cmake)

run_display_case(v1_legacy RIB "${RIBDIR}/v1_legacy.rib" TAG v1_legacy STARTS 2 MESSAGES 1 EXIT_ZERO
    PARTS "v1_legacy\\.dsply" "1\\.0\\.0" "1\\.2\\.0")
run_display_case(v1_explicit RIB "${RIBDIR}/v1_explicit.rib" TAG v1_explicit STARTS 1 MESSAGES 0 EXIT_ZERO)
run_display_case(v1_newer RIB "${RIBDIR}/v1_newer.rib" TAG v1_newer STARTS 1 MESSAGES 1 EXIT_ZERO
    PARTS "v1_newer\\.dsply" "1\\.4\\.0")

if(_or_failures GREATER 0)
    message(FATAL_ERROR "DisplayAbi_Versions_V1: ${_or_failures} failure(s).")
endif()
