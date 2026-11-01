# The frozen pre-spec file driver (spec 037, SC-013 / T008).
#
# A display driver built before this spec: copies of the base tree's file.cpp, file_tiff.*,
# file_png.*, file_base.* and file.h, and of dsply.h renamed dsply_v1_frozen.h, with only the
# #include "dsply.h" lines changed. DisplayAbi_LegacyPixelsIdentical (display_baseline/) renders
# the baseline scenes through it and requires the decoded pixels to match the pre-change
# references: the proof that an unversioned driver still produces identical output through the
# v1 adapter.
#
# ribRender's PUBLIC include directories contain src/display/, which will hold the new 2.0 dsply.h.
# This target therefore sees only src/ (for common/*.h) and its own directory, and includes the
# frozen header by relative path. It links openrendercommon like the live module (which links
# openrenderfilebase -> openrendercommon) but never the live openrenderfilebase: file_base.cpp is
# compiled in from the frozen copy.
#
# The module is named "file", so a display of type "file" resolves to it when Option "searchpath"
# "display" names this directory.

or_plugin_test_module(plugin_display_legacy_file display legacy_v1
    file.cpp file_tiff.cpp file_png.cpp file_base.cpp
    OUTPUT_NAME file
    DEFS HAVE_LIBPNG
    INCLUDES ${CMAKE_SOURCE_DIR}/src
    LIBS ${openrendertiff_libs} openrendercommon
    NO_HEADER_DIRS)
if(UNIX)
    target_link_libraries(plugin_display_legacy_file PRIVATE z)
endif()
