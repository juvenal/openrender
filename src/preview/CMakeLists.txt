# No cmake_minimum_required() here on purpose: this file is only ever reached
# through add_subdirectory(), and a subdirectory call would reset the policy
# stack, overriding the tiered floor set in the top-level CMakeLists.txt.

# libribpreview — platform-neutral geometry extraction static library
add_subdirectory(libribpreview)

# Platform-specific viewer binaries
#
# The Linux viewer needs gtk4 >= 4.10 (GtkFileDialog) and libadwaita-1 >= 1.4
# (AdwToolbarView), and its own CMakeLists.txt requests those with
# pkg_check_modules(REQUIRED), which aborts the configure when they are missing.
# That turns "no wireframe viewer on this distribution" into "openRender does not
# build on this distribution" -- which is what happened on Ubuntu 22.04 (gtk4
# 4.6.9, libadwaita 1.1.0). Ubuntu 24.04 (gtk4 4.14, libadwaita 1.5) clears both.
#
# So probe first, non-fatally, and only descend when the stack can actually
# satisfy the viewer. Everything else in the tree builds either way. Same
# degrade-rather-than-abort rule the LLVM and OpenEXR blocks follow.
if(APPLE)
    add_subdirectory(orender-wire-macos)
elseif(UNIX)
    set(_wire_missing "")
    find_package(PkgConfig QUIET)
    if(NOT PkgConfig_FOUND)
        list(APPEND _wire_missing "pkg-config")
    else()
        pkg_check_modules(_WIRE_GTK4    QUIET gtk4>=4.10)
        pkg_check_modules(_WIRE_ADWAITA QUIET libadwaita-1>=1.4)
        pkg_check_modules(_WIRE_EPOXY   QUIET epoxy)
        if(NOT _WIRE_GTK4_FOUND)
            list(APPEND _wire_missing "gtk4>=4.10")
        endif()
        if(NOT _WIRE_ADWAITA_FOUND)
            list(APPEND _wire_missing "libadwaita-1>=1.4")
        endif()
        if(NOT _WIRE_EPOXY_FOUND)
            list(APPEND _wire_missing "epoxy")
        endif()
    endif()
    find_package(OpenGL QUIET)
    if(NOT OpenGL_FOUND)
        list(APPEND _wire_missing "OpenGL")
    endif()

    if(_wire_missing)
        string(REPLACE ";" ", " _wire_missing "${_wire_missing}")
        message(STATUS
            "  - orender-wire (Linux viewer) SKIPPED -- unavailable: ${_wire_missing}")
    else()
        add_subdirectory(orender-wire-linux)
    endif()
    unset(_wire_missing)
endif()

message(STATUS "  - preview subsystem (libribpreview + orender-wire)")
