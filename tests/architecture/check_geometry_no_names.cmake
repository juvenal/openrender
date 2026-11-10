# Geometry_NoHiderShadingNames (spec 031, FR-001, SC-003, US2 AS1): no file under src/ri/geometry/ names a hider, the
# shading context, the renderer core or the Scene Interface context class, in code or in a comment.
#
#   cmake -DGEOMETRY_DIR=<src/ri/geometry> -P check_geometry_no_names.cmake
#
# The banned identifiers are matched as whole identifiers (so `CRendererContext` is not a `CRenderer` hit):
#   CReyes CStochastic CZbuffer CRaytracer CPhotonHider CHider CShadingContext IShadingProducer CRenderer
#   CRendererContext CRiInterface
# One exception, keyed by file and token count so that a third use fails: blobbyRepeller.cpp may contain exactly the
# two `CRenderer` tokens of its CRenderer::locateFile(..., CRenderer::texturePath) call (a later file-search and
# texture-path service, spec 031 FR-016). Every offending file:line is printed. Red on the starting tree: about 300
# lines in 44 files, most of them comments (tasks T084 rewrites those).
if(NOT GEOMETRY_DIR OR NOT IS_DIRECTORY "${GEOMETRY_DIR}")
    message(FATAL_ERROR "GEOMETRY_DIR is not a directory: '${GEOMETRY_DIR}'")
endif()

set(_banned CReyes CStochastic CZbuffer CRaytracer CPhotonHider CHider CShadingContext IShadingProducer CRenderer CRendererContext CRiInterface)
set(_allowed_file "blobbyRepeller.cpp")
set(_allowed_token "CRenderer")
set(_allowed_count 2)

file(GLOB_RECURSE _files "${GEOMETRY_DIR}/*")
list(SORT _files)

set(_offences "")
set(_offending_lines 0)
set(_allowed_seen 0)
foreach(_f IN LISTS _files)
    if(IS_DIRECTORY "${_f}")
        continue()
    endif()
    get_filename_component(_name "${_f}" NAME)
    file(STRINGS "${_f}" _lines)
    set(_n 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _n "${_n} + 1")
        string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*" _words "${_line}")
        foreach(_w IN LISTS _words)
            list(FIND _banned "${_w}" _i)
            if(_i EQUAL -1)
                continue()
            endif()
            if(_name STREQUAL _allowed_file AND _w STREQUAL _allowed_token)
                math(EXPR _allowed_seen "${_allowed_seen} + 1")
                if(_allowed_seen LESS_EQUAL _allowed_count)
                    continue()
                endif()
            endif()
            list(APPEND _offences "${_f}:${_n}: ${_w}")
            math(EXPR _offending_lines "${_offending_lines} + 1")
        endforeach()
    endforeach()
endforeach()

if(_offences)
    list(LENGTH _offences _count)
    foreach(_o IN LISTS _offences)
        message(STATUS "VIOLATION ${_o}")
    endforeach()
    message(FATAL_ERROR "Geometry_NoHiderShadingNames: ${_count} banned identifier(s) under ${GEOMETRY_DIR}")
endif()
if(NOT _allowed_seen EQUAL _allowed_count)
    message(FATAL_ERROR "Geometry_NoHiderShadingNames: ${_allowed_file} holds ${_allowed_seen} `${_allowed_token}` token(s), the exception is exactly ${_allowed_count}: update the exception, never widen it")
endif()
message(STATUS "Geometry_NoHiderShadingNames: no banned identifier under ${GEOMETRY_DIR}")
