# T-L1 Link_ShadingSelfContained (spec 029, FR-001, SC-001).
#
# libshader_shading imports nothing from the renderer: the library links with undefined symbols as an error, so it
# has no flat-namespace bind (macOS) and no undefined symbol that libRIBRender defines (Linux).
#
#   -DLIB=<libshader_shading>  -DRENDER=<libRIBRender>  -DSYSTEM=<CMAKE_SYSTEM_NAME>
#
# macOS: `dyld_info -imports LIB` (FR-001 names -fixups, which crashes on this library: see below) lists no
# `(from <flat-namespace>)` import and `nm -m LIB` no "(dynamically looked up)" import.
# Linux: `nm -D --undefined-only LIB` minus the dynamic exports of RENDER (`nm -D --defined-only`) is the same set (no
# symbol of the library is defined by libRIBRender), and `readelf -d LIB` shows no libRIBRender NEEDED entry.
# Red on the starting tree: the 4 imports (CGatherBundle::CGatherBundle(), vtable for CTraceBundle, vtable for
# CTransmissionBundle, CPhotonMap::lookup(float*, float const*, int)).

foreach(_v LIB RENDER SYSTEM)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "check_shading_self_contained: -D${_v}= is required")
    endif()
endforeach()
if(NOT EXISTS "${LIB}")
    message(FATAL_ERROR "check_shading_self_contained: ${LIB} does not exist")
endif()

set(_failures "")

if(SYSTEM STREQUAL "Darwin")
    find_program(DYLD_INFO dyld_info)
    find_program(NM nm)
    if(NOT DYLD_INFO OR NOT NM)
        message(FATAL_ERROR "check_shading_self_contained: dyld_info and nm are required on macOS")
    endif()
    # `dyld_info -imports` rather than `-fixups`: the latter exits with status 133 after a truncated listing on large
    # libraries (measured on this tree's libshader_shading, Xcode 27), the former lists the same flat-namespace imports
    execute_process(COMMAND ${DYLD_INFO} -imports "${LIB}" OUTPUT_VARIABLE _imports RESULT_VARIABLE _rc ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "dyld_info -imports failed (${_rc}): ${_err}")
    endif()
    string(REPLACE "\n" ";" _lines "${_imports}")
    foreach(_l IN LISTS _lines)
        if(_l MATCHES "^ +([^ ]+) +\\(from <flat-namespace>\\)")
            list(APPEND _failures "flat-namespace import: ${CMAKE_MATCH_1}")
        endif()
    endforeach()
    execute_process(COMMAND ${NM} -m "${LIB}" OUTPUT_VARIABLE _nm RESULT_VARIABLE _rc2)
    string(REPLACE "\n" ";" _nmlines "${_nm}")
    foreach(_l IN LISTS _nmlines)
        if(_l MATCHES "\\(dynamically looked up\\)")
            string(REGEX REPLACE "^ *\\(undefined\\) external " "" _sym "${_l}")
            list(APPEND _failures "dynamically looked up import: ${_sym}")
        endif()
    endforeach()
else()
    find_program(NM nm)
    find_program(READELF readelf)
    if(NOT NM OR NOT READELF)
        message(FATAL_ERROR "check_shading_self_contained: nm and readelf are required on ${SYSTEM}")
    endif()
    execute_process(COMMAND ${NM} -D --undefined-only "${LIB}" OUTPUT_VARIABLE _undef RESULT_VARIABLE _rc)
    execute_process(COMMAND ${NM} -D --defined-only "${RENDER}" OUTPUT_VARIABLE _def RESULT_VARIABLE _rc2)
    if(NOT _rc EQUAL 0 OR NOT _rc2 EQUAL 0)
        message(FATAL_ERROR "nm -D failed")
    endif()
    # names only, version suffix dropped
    function(_symbol_names TEXT OUTVAR)
        string(REPLACE "\n" ";" _ls "${TEXT}")
        set(_names "")
        foreach(_l IN LISTS _ls)
            string(STRIP "${_l}" _l)
            if(_l MATCHES "^([0-9a-fA-F]+ )?[A-Za-z] ([^ @]+)")
                list(APPEND _names "${CMAKE_MATCH_2}")
            endif()
        endforeach()
        set(${OUTVAR} "${_names}" PARENT_SCOPE)
    endfunction()
    _symbol_names("${_undef}" _undef_names)
    _symbol_names("${_def}" _def_names)
    # a set lookup: map each defined name to a CMake variable-free membership test through a sorted list
    list(REMOVE_DUPLICATES _def_names)
    foreach(_n IN LISTS _def_names)
        set(_def_${_n} 1)
    endforeach()
    foreach(_n IN LISTS _undef_names)
        if(_def_${_n})
            list(APPEND _failures "imports a symbol libRIBRender defines: ${_n}")
        endif()
    endforeach()
    execute_process(COMMAND ${READELF} -d "${LIB}" OUTPUT_VARIABLE _dyn)
    string(REPLACE "\n" ";" _dynlines "${_dyn}")
    foreach(_l IN LISTS _dynlines)
        if(_l MATCHES "NEEDED.*RIBRender")
            list(APPEND _failures "NEEDED libRIBRender: ${_l}")
        endif()
    endforeach()
endif()

list(LENGTH _failures _n)
if(_n GREATER 0)
    foreach(_f IN LISTS _failures)
        message(STATUS "FAIL: ${_f}")
    endforeach()
    message(FATAL_ERROR "Link_ShadingSelfContained: ${_n} dependency(ies) of libshader_shading on the renderer")
endif()
message(STATUS "Link_ShadingSelfContained: libshader_shading is self-contained")
