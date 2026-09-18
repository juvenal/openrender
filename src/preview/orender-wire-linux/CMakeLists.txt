# No cmake_minimum_required() here on purpose: this file is only ever reached
# through add_subdirectory(), and a subdirectory call would reset the policy
# stack, overriding the floor set in the top-level CMakeLists.txt.

if(APPLE)
    return()
endif()

find_package(OpenGL)
if(NOT OpenGL_FOUND)
    message(WARNING "OpenGL not found — orender-wire-linux will not be built")
    return()
endif()

find_package(PkgConfig)
if(NOT PkgConfig_FOUND)
    message(WARNING "pkg-config not found — orender-wire-linux will not be built")
    return()
endif()

pkg_check_modules(GTK4 gtk4>=4.10)
if(NOT GTK4_FOUND)
    message(WARNING "gtk4>=4.10 not found — orender-wire-linux will not be built")
    return()
endif()

pkg_check_modules(ADWAITA libadwaita-1>=1.4)
if(NOT ADWAITA_FOUND)
    message(WARNING "libadwaita-1>=1.4 not found — orender-wire-linux will not be built")
    return()
endif()

pkg_check_modules(EPOXY epoxy)
if(NOT EPOXY_FOUND)
    message(WARNING "epoxy not found — orender-wire-linux will not be built")
    return()
endif()

add_executable(orender-wire-linux
    main.cpp
    arcball.cpp
)

target_include_directories(orender-wire-linux PRIVATE
    ${CMAKE_SOURCE_DIR}/src/preview
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
    ${GTK4_INCLUDE_DIRS}
    ${ADWAITA_INCLUDE_DIRS}
    ${EPOXY_INCLUDE_DIRS}
)

target_link_libraries(orender-wire-linux PRIVATE
    ribpreview
    ${GTK4_LIBRARIES}
    ${ADWAITA_LIBRARIES}
    ${EPOXY_LIBRARIES}
    OpenGL::GL
)

target_compile_features(orender-wire-linux PRIVATE cxx_std_20)
target_compile_options(orender-wire-linux PRIVATE ${GTK4_CFLAGS_OTHER} ${ADWAITA_CFLAGS_OTHER})
target_link_options(orender-wire-linux PRIVATE ${GTK4_LDFLAGS_OTHER} ${ADWAITA_LDFLAGS_OTHER})

# Target name carries the platform suffix (matches orender-wire-macos); the produced binary is
# still plain "orender-wire" on disk, in the build tree and once installed.
#
# BUILD_WITH_INSTALL_RPATH is intentionally left at its default (FALSE): setting it TRUE would
# bake INSTALL_RPATH ($ORIGIN/../lib, correct once installed to libexec/ next to lib/libribVector.so)
# into the *build-tree* binary too, where $ORIGIN/../lib resolves to a directory that doesn't
# exist (libribVector.so actually lives under build/src/ri/) -- the binary would fail to even start
# ("error while loading shared libraries: libribVector.so.1: cannot open shared object file") when run
# directly from build/, which is exactly how this gets tested during development. Leaving this
# unset lets CMake auto-compute a correct build-tree RPATH while still applying INSTALL_RPATH at
# actual `cmake --install` time.
#
# ribpreview links ribVector (not ribRender): the geometry-only extraction
# with no LLVM/libshader_shading dependency at all -- this is what actually
# fixes the LLVM symbol-collision crash between this project's own bundled
# LLVM and Mesa's system LLVM (both loaded into the same process once
# orender-wire, which uses OpenGL, links anything pulling in LLVM) that used
# to crash orender-wire on Linux. See the domain-split plan and
# src/ri/CMakeLists.txt's ribVector section for the full rationale.
set_target_properties(orender-wire-linux PROPERTIES
    OUTPUT_NAME "orender-wire"
    INSTALL_RPATH "$ORIGIN/../lib"
)

install(TARGETS orender-wire-linux DESTINATION libexec)
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ../libexec/orender-wire ${CMAKE_INSTALL_PREFIX}/bin/orender-wire)")
