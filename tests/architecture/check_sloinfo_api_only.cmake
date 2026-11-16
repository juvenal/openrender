# Sloinfo_MetadataApiOnly (spec 033, FR-017, SC-006, contracts/checks-and-tests.md): sloinfo reads shaders through the metadata
# API only. No file under src/sloinfo/
#   includes a header of the shading or runtime libraries: any #include of a path under libshader/ other than
#   libshader/include/openrender/ShaderMetadata.h, or of the inspection parser's header "rslo.h";
#   names rsloGet rsloDelete CLLVMJitEngine SLOShaderInfo TRSLObjectShader (whole identifiers, code and comments).
#
#   cmake -DSOURCE_DIR=<a tree holding src/...> -P check_sloinfo_api_only.cmake
#
# Every hit is printed as "VIOLATION <file>:<line>: <what>". Red on the starting tree: the three includes (libshader/shading/llvmJit.h,
# libshader/shading/sloMetadata.h, rslo.h) and the parser names (specs/033-shading-system-backends/tools/scan-base.txt). The fixture run
# (fixtures/033-sloinfo) plants an include and must fail.
if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "check_sloinfo_api_only: -DSOURCE_DIR= is required")
endif()
include(${CMAKE_CURRENT_LIST_DIR}/scan_033_common.cmake)

set(_hits "")
scan_033_tree("${SOURCE_DIR}" src/sloinfo "rsloGet;rsloDelete;CLLVMJitEngine;SLOShaderInfo;TRSLObjectShader" _hits)

file(GLOB_RECURSE _files LIST_DIRECTORIES false "${SOURCE_DIR}/src/sloinfo/*")
foreach(_f IN LISTS _files)
    file(RELATIVE_PATH _rel "${SOURCE_DIR}" "${_f}")
    scan_033_lines("${_f}" _lines)
    set(_n 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _n "${_n} + 1")
        if(_line MATCHES "^[ \t]*#[ \t]*include[ \t]+[\"<]([^\">]*)[\">]")
            set(_path "${CMAKE_MATCH_1}")
            if(_path MATCHES "^libshader/" AND NOT _path STREQUAL "libshader/include/openrender/ShaderMetadata.h")
                list(APPEND _hits "${_rel}:${_n}: #include \"${_path}\"")
            elseif(_path STREQUAL "rslo.h")
                list(APPEND _hits "${_rel}:${_n}: #include \"rslo.h\" (the inspection parser's header)")
            endif()
        endif()
    endforeach()
endforeach()

scan_033_report("${_hits}" _count)
if(_count GREATER 0)
    message(FATAL_ERROR "Sloinfo_MetadataApiOnly: ${_count} use(s) of the readers' internals in src/sloinfo")
endif()
message(STATUS "Sloinfo_MetadataApiOnly: sloinfo reads shaders through the metadata API only")
