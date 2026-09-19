# src/display/CMakeLists.txt
#
# Display-driver plugins -- dynamically loaded (MODULE) libraries, all built
# against openrendercommon only (none of them link ribRender/ribVector/ribOut).
# Grouped here as a sibling of src/ri, mirroring the src/libshader/ pattern.
#
#   file/         -- TIFF/PNG file-format display plugin, plus the shared
#                     openrenderfilebase scanline pipeline used by openexr/rgbe
#   framebuffer/  -- interactive framebuffer display driver (IPC to a native
#                     viewer helper on macOS/Linux, native GDI on Windows)
#   openexr/      -- OpenEXR file-format display plugin
#   rgbe/         -- Radiance RGBE file-format display plugin
#
# dsply.h (the public Display-driver ABI contract) lives directly in this
# directory so third-party plugin authors can build against it standalone.

# No cmake_minimum_required() here on purpose: this file is only ever reached
# through add_subdirectory(), and a subdirectory call would reset the policy
# stack, overriding the tiered floor set in the top-level CMakeLists.txt.

add_subdirectory(file)
add_subdirectory(framebuffer)
add_subdirectory(openexr)
add_subdirectory(rgbe)
