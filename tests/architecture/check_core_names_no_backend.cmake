# Shading_CoreNoBackendNames (spec 033, FR-003, SC-002, contracts/checks-and-tests.md): the renderer core and the other
# domains name no shader backend. No file under src/ri/, src/display/, src/shadeop/, src/procedures/ or src/common/ names, in code
# or in a comment, any of
#   parseShader parseSloShader codeArea constantEntries codeEntryPoint initEntryPoint jitEntry jitInitEntry TShaderJitEntry
#   CLLVMJitEngine CProgrammableShaderInstance CRslPayload CRsloPayload CSloPayload jitSetInitXform jitSetInitCoordinateResolver
# as a whole identifier, and no file under src/ri/ names OPENRENDER_HAVE_LLVM.
#
#   cmake -DSOURCE_DIR=<a tree holding src/...> -P check_core_names_no_backend.cmake
#
# Every hit is printed as "VIOLATION <file>:<line>: <identifier>". Red on the starting tree: 35 lines (31 identifiers: init.cpp 19,
# rendererFiles.cpp 6, rendererContext.cpp 2, rendererContext.h 1, rendererFileSearch.cpp 2, src/ri/CMakeLists.txt 1; and 4
# OPENRENDER_HAVE_LLVM: init.cpp 2, rendererFiles.cpp 2), listed in specs/033-shading-system-backends/tools/scan-base.txt.
# The fixture run (fixtures/033-core-names) plants one identifier and must fail (the mutant check).
if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "check_core_names_no_backend: -DSOURCE_DIR= is required")
endif()
include(${CMAKE_CURRENT_LIST_DIR}/scan_033_common.cmake)

set(_identifiers
    parseShader parseSloShader codeArea constantEntries codeEntryPoint initEntryPoint jitEntry jitInitEntry TShaderJitEntry
    CLLVMJitEngine CProgrammableShaderInstance CRslPayload CRsloPayload CSloPayload jitSetInitXform jitSetInitCoordinateResolver)

set(_hits "")
foreach(_dir src/ri src/display src/shadeop src/procedures src/common)
    scan_033_tree("${SOURCE_DIR}" "${_dir}" "${_identifiers}" _hits)
endforeach()
scan_033_tree("${SOURCE_DIR}" src/ri "OPENRENDER_HAVE_LLVM" _hits)

scan_033_report("${_hits}" _count)
if(_count GREATER 0)
    message(FATAL_ERROR "Shading_CoreNoBackendNames: ${_count} backend name(s) in the core and the other domains")
endif()
message(STATUS "Shading_CoreNoBackendNames: the core and the other domains name no shader backend")
