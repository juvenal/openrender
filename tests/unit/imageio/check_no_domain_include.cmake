# ImageIO_NoDomainInclude (spec 048, T034, US5, FR-004/FR-005): every #include of src/imageio/*.{h,cpp} is a
# standard header, the library's own published header or another file of its own directory, common/, or an OIIO
# header; and the published header orImageIO.h includes standard headers only, and none of OpenImageIO, OpenColorIO,
# Imath or tiff. Registered only in a build with OIIO. A cmake -P script, no git.
#
#   cmake -DIMAGEIO_DIR=<src/imageio> -P check_no_domain_include.cmake
if(NOT IMAGEIO_DIR OR NOT IS_DIRECTORY "${IMAGEIO_DIR}")
    message(FATAL_ERROR "ImageIO_NoDomainInclude: src/imageio does not exist: '${IMAGEIO_DIR}'")
endif()
file(GLOB sources "${IMAGEIO_DIR}/*.h" "${IMAGEIO_DIR}/*.cpp")
list(LENGTH sources count)
if(count EQUAL 0)
    message(FATAL_ERROR "ImageIO_NoDomainInclude: no source under ${IMAGEIO_DIR}")
endif()

set(failures 0)
foreach(file IN LISTS sources)
    get_filename_component(base "${file}" NAME)
    file(STRINGS "${file}" lines REGEX "^[ \t]*#[ \t]*include")
    foreach(line IN LISTS lines)
        if(NOT line MATCHES "#[ \t]*include[ \t]*([<\"])([^>\"]+)[>\"]")
            continue()
        endif()
        set(open "${CMAKE_MATCH_1}")
        set(target "${CMAKE_MATCH_2}")
        set(ok FALSE)
        if(base STREQUAL "orImageIO.h")
            # the published header: standard headers only (no slash, not a library header)
            if(open STREQUAL "<" AND NOT target MATCHES "/" AND NOT target MATCHES "^(tiff|png|jpeglib|zlib)")
                set(ok TRUE)
            endif()
        else()
            if(open STREQUAL "<" AND NOT target MATCHES "/")
                set(ok TRUE)                                   # standard or C library header
            elseif(target MATCHES "^OpenImageIO/")
                set(ok TRUE)                                   # the wrapped library's own header
            elseif(target MATCHES "^common/")
                set(ok TRUE)                                   # Infra
            elseif(open STREQUAL "\"" AND EXISTS "${IMAGEIO_DIR}/${target}")
                set(ok TRUE)                                   # its own published or private header
            endif()
        endif()
        if(NOT ok)
            string(STRIP "${line}" shown)
            message(STATUS "FAIL: ${base}: ${shown}")
            math(EXPR failures "${failures}+1")
        endif()
    endforeach()
endforeach()
if(failures GREATER 0)
    message(FATAL_ERROR "ImageIO_NoDomainInclude: ${failures} include(s) outside the allowed set")
endif()
message(STATUS "ImageIO_NoDomainInclude: ${count} file(s), every include is allowed")
