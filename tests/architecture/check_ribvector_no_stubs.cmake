# Geometry_RibVectorNoStubs (spec 031, FR-008, FR-009, FR-011, SC-004, US3): ribVector compiles no renderer-side stub
# and its objects reference no hider or shading-context symbol.
#
#   cmake -DSOURCES=<$<TARGET_PROPERTY:ribVector_obj,SOURCES>> -DOBJECT_DIR=<build/src/ri/CMakeFiles/ribVector_obj.dir>
#         -P check_ribvector_no_stubs.cmake
#
# Fails when
#   1. ribVector_obj compiles a vector/*Stub.cpp other than irradianceDispatchStub.cpp (spec 032's), or
#   2. `nm -u` over the built objects names a CShadingContext, CReyes, CHider or IShadingProducer symbol. The mangled
#      name is matched by its length-prefixed identifier (15CShadingContext, 6CReyes, 6CHider, 16IShadingProducer), which
#      is exact: a longer name has another length. (c++filt is not used: execute_process truncates its large input.)
# Red on the starting tree: geometryDispatchStub.cpp, rendererDeclarationsDispatchStub.cpp and
# surfaceCrossingDispatchStub.cpp are compiled, and the objects reference CShadingContext/CReyes symbols.
if(NOT SOURCES)
    message(FATAL_ERROR "SOURCES is empty")
endif()

set(_stubs "")
foreach(_s IN LISTS SOURCES)
    if(_s MATCHES "(^|/)vector/[A-Za-z0-9_]*Stub\\.cpp$" AND NOT _s MATCHES "(^|/)irradianceDispatchStub\\.cpp$")
        list(APPEND _stubs "${_s}")
    endif()
endforeach()

set(_failed FALSE)
foreach(_s IN LISTS _stubs)
    message(STATUS "VIOLATION ribVector_obj compiles ${_s}")
    set(_failed TRUE)
endforeach()

find_program(NM_EXECUTABLE nm)
if(NOT NM_EXECUTABLE)
    message(FATAL_ERROR "nm is required")
endif()
if(NOT IS_DIRECTORY "${OBJECT_DIR}")
    message(FATAL_ERROR "OBJECT_DIR is not a directory (an incomplete build tree): '${OBJECT_DIR}'")
endif()

file(GLOB_RECURSE _objects "${OBJECT_DIR}/*.o")
list(LENGTH _objects _nobjects)
if(_nobjects EQUAL 0)
    message(FATAL_ERROR "no objects under ${OBJECT_DIR} (an incomplete build tree)")
endif()
list(SORT _objects)

execute_process(COMMAND ${NM_EXECUTABLE} -u ${_objects}
    OUTPUT_VARIABLE _undefined RESULT_VARIABLE _rc ERROR_QUIET)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "nm -u failed (${_rc})")
endif()
string(REPLACE "\n" ";" _lines "${_undefined}")
set(_hits 0)
foreach(_l IN LISTS _lines)
    if(_l MATCHES "(15CShadingContext|6CReyes|6CHider|16IShadingProducer)")
        string(STRIP "${_l}" _l)
        message(STATUS "VIOLATION undefined reference: ${_l}")
        math(EXPR _hits "${_hits} + 1")
        set(_failed TRUE)
    endif()
endforeach()

if(_failed)
    message(FATAL_ERROR "Geometry_RibVectorNoStubs: ${_hits} undefined hider/shading symbol(s), stubs compiled: ${_stubs}")
endif()
message(STATUS "Geometry_RibVectorNoStubs: ${_nobjects} objects, no stub, no hider or shading symbol")
