# copy_bundles.cmake — called POST_BUILD to copy SwiftPM resource bundles
# into the main build output directory alongside the orender-fb-macos binary.
#
# Called with:
#   cmake -DSRC_DIR=<swift-build/release> -DDST_DIR=<cmake-binary-dir> -P copy_bundles.cmake

file(GLOB _bundles "${SRC_DIR}/*.bundle")
foreach(_b ${_bundles})
    if(IS_DIRECTORY "${_b}")
        file(COPY "${_b}" DESTINATION "${DST_DIR}")
    endif()
endforeach()
