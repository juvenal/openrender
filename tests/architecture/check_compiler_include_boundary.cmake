# Compiler_IncludeBoundary (spec 038, FR-018, SC-007, decision Q10; contracts/compiler-tables.md): the RSL
# compiler includes nothing of the shading engine, the Scene Interface or the renderer core.
#
#   cmake -DINCLUDE_DIRS=<compiler target include directories, then those of the targets it links>
#         -DCOMPILER_DIR=<src/libshader/compiler> -DCOMPILER_BINARY_DIR=<build/src/libshader/compiler>
#         -DSOURCE_DIR=<repo> -DBUILD_DIR=<build> -P check_compiler_include_boundary.cmake
#
# Fails if
#   1. an include directory is src/ri, src/libshader/shading or anything below them;
#   2. an include of a compiler file (every .h/.c/.cpp/.y/.l under src/libshader/compiler, and the generated
#      parser and lexer in the build tree), resolved as the compiler does (the file's own directory first for a
#      quoted include, then INCLUDE_DIRS in order), lands outside the allowed roots: src/libshader/compiler,
#      the compiler's build directory, src/common, src/include, src/shadeop, src/libshader/include, the
#      top-level build directory itself (config.h, version_config.h), and anything outside the repository's
#      source and build trees (LLVM and the system).
# Red on the starting tree: rule 1 (the compiler CMakeLists adds src/ri, its eight subdirectories and
# src/libshader/shading) and rule 2 (llvmEmitter.cpp includes scriptFunctions.h and scriptOpcodes.h).
include(${CMAKE_CURRENT_LIST_DIR}/domain_classify.cmake)
or_check_begin()
get_filename_component(_src "${SOURCE_DIR}" REALPATH)
get_filename_component(_bld "${BUILD_DIR}" REALPATH)
get_filename_component(_cdir "${COMPILER_DIR}" REALPATH)
get_filename_component(_cbld "${COMPILER_BINARY_DIR}" REALPATH)

# 1. directories (the property and the linked targets' usage requirements repeat some of them)
set(_dirs "")
list(REMOVE_DUPLICATES INCLUDE_DIRS)
foreach(_d IN LISTS INCLUDE_DIRS)
    if(_d STREQUAL "" OR _d MATCHES "^\\$<")
        continue()
    endif()
    get_filename_component(_rd "${_d}" REALPATH)
    list(APPEND _dirs "${_rd}")
    foreach(_bad "${_src}/src/ri" "${_src}/src/libshader/shading")
        if("${_rd}" STREQUAL "${_bad}" OR "${_rd}" MATCHES "^${_bad}/")
            string(REPLACE "${_src}/" "" _show "${_rd}")
            or_check_fail("the compiler's include path contains ${_show} (Scene Interface / renderer / shading engine)")
        endif()
    endforeach()
endforeach()
list(LENGTH _dirs _nd)
if(_nd EQUAL 0)
    or_check_fail("INCLUDE_DIRS is empty: the target property was not passed")
endif()

# 2. includes
set(_roots
    "${_cdir}" "${_cbld}"
    "${_src}/src/common" "${_src}/src/include" "${_src}/src/shadeop" "${_src}/src/libshader/include")
set(_files "")
file(GLOB_RECURSE _found "${_cdir}/*")
foreach(_f IN LISTS _found)
    if("${_f}" MATCHES "\\.(h|hpp|c|cc|cpp|y|l)$")
        list(APPEND _files "${_f}")
    endif()
endforeach()
file(GLOB _gen "${_cbld}/*.cpp" "${_cbld}/*.h" "${_cbld}/*.hpp")
list(APPEND _files ${_gen})
set(_n 0)
set(_reported "")
foreach(_f IN LISTS _files)
    or_file_includes("${_f}" _names _quoted)
    set(_i 0)
    foreach(_name IN LISTS _names)
        list(GET _quoted ${_i} _q)
        math(EXPR _i "${_i} + 1")
        or_resolve_include("${_f}" "${_name}" ${_q} "${_dirs}" _abs)
        if(_abs STREQUAL "")
            continue()
        endif()
        get_filename_component(_ra "${_abs}" REALPATH)
        math(EXPR _n "${_n} + 1")
        set(_ok FALSE)
        foreach(_r IN LISTS _roots)
            if("${_ra}" MATCHES "^${_r}/")
                set(_ok TRUE)
            endif()
        endforeach()
        get_filename_component(_ra_dir "${_ra}" DIRECTORY)
        if("${_ra_dir}" STREQUAL "${_bld}")
            set(_ok TRUE) # config.h, version_config.h
        endif()
        if(NOT "${_ra}" MATCHES "^${_src}/" AND NOT "${_ra}" MATCHES "^${_bld}/")
            set(_ok TRUE) # LLVM, the system
        endif()
        if(NOT _ok)
            string(REPLACE "${_src}/" "" _show "${_ra}")
            string(REPLACE "${_src}/" "" _from "${_f}")
            string(REPLACE "${_bld}/" "@build/" _from "${_from}")
            list(FIND _reported "${_from}|${_name}" _seen)
            if(_seen EQUAL -1)
                list(APPEND _reported "${_from}|${_name}")
                or_check_fail("${_from} includes \"${_name}\", which resolves to ${_show} (outside the compiler's allowed roots)")
            endif()
        endif()
    endforeach()
endforeach()
list(LENGTH _files _nf)
message("${_nf} compiler file(s) scanned, ${_n} include(s) resolved")
or_check_end(Compiler_IncludeBoundary)
