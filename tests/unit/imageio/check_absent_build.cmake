# ImageIO_AbsentBuild (spec 048, T033, FR-003): a build without OIIO carries no OIIO code and no OIIO driver: no
# libRIBImageIO* and no oiio.dsply anywhere in the build tree, and `nm -a` over every executable and library of the
# tree finds no OpenImageIO symbol. Registered only without OIIO. A cmake -P script, no git.
#
#   cmake -DBUILD_DIR=<build tree> -DNM=<nm> -P check_absent_build.cmake
if(NOT BUILD_DIR OR NOT IS_DIRECTORY "${BUILD_DIR}")
    message(FATAL_ERROR "ImageIO_AbsentBuild: build tree not found: '${BUILD_DIR}'")
endif()
if(NOT NM)
    find_program(NM nm)
endif()
if(NOT NM)
    message(FATAL_ERROR "ImageIO_AbsentBuild: no nm available")
endif()
set(failures 0)

# 1. No library or driver of the OIIO backend anywhere in the tree.
file(GLOB_RECURSE leftovers "${BUILD_DIR}/libRIBImageIO*" "${BUILD_DIR}/oiio.dsply" "${BUILD_DIR}/oiio.so" "${BUILD_DIR}/*/oiio.dsply")
foreach(f IN LISTS leftovers)
    message(STATUS "FAIL: found ${f}")
    math(EXPR failures "${failures}+1")
endforeach()
if(NOT leftovers)
    message(STATUS "ok:   no libRIBImageIO* and no oiio.dsply in the build tree")
endif()

# 2. No OpenImageIO symbol in any executable or library of the tree (object files and CMake's own files excluded).
execute_process(
    COMMAND find "${BUILD_DIR}" -path "*/CMakeFiles" -prune -o -type f
            ( -name "*.dylib" -o -name "*.so" -o -name "*.so.*" -o -name "*.dsply" -o -name "*.a" -o -perm -u+x )
            -print
    OUTPUT_VARIABLE listing RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "ImageIO_AbsentBuild: find failed (${rc})")
endif()
string(REPLACE "\n" ";" files "${listing}")
set(inspected 0)
foreach(f IN LISTS files)
    if(f STREQUAL "" OR f MATCHES "/vcpkg_installed/" OR f MATCHES "\\.(sh|cmake|py|txt|log)$")
        continue()
    endif()
    execute_process(COMMAND "${NM}" -a "${f}" OUTPUT_VARIABLE symbols ERROR_QUIET RESULT_VARIABLE nmrc)
    if(NOT nmrc EQUAL 0)
        continue()                                              # not an object file (a script, say)
    endif()
    math(EXPR inspected "${inspected}+1")
    string(FIND "${symbols}" "OpenImageIO" pos)
    if(NOT pos EQUAL -1)
        message(STATUS "FAIL: ${f} references an OpenImageIO symbol")
        math(EXPR failures "${failures}+1")
    endif()
endforeach()
if(inspected EQUAL 0)
    message(STATUS "FAIL: no executable or library was inspected (is the tree built?)")
    math(EXPR failures "${failures}+1")
else()
    message(STATUS "ok:   ${inspected} executable(s)/library(ies) inspected, no OpenImageIO symbol")
endif()

if(failures GREATER 0)
    message(FATAL_ERROR "ImageIO_AbsentBuild: ${failures} check(s) failed")
endif()
