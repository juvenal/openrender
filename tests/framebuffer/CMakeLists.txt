# Test suite for framebuffer drivers

# Wayland tests — Linux only (Wayland does not exist on macOS or Windows)
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND wayland-client)
        pkg_check_modules(LIBDECOR libdecor-0)
    endif()

    if(WAYLAND_FOUND)
        # Mark Wayland protocol sources as generated so CMake doesn't require them to
        # exist at configure time (they are produced by add_custom_command in
        # src/framebuffer/CMakeLists.txt during the build step).
        set_source_files_properties(
            ${CMAKE_BINARY_DIR}/src/framebuffer/xdg-shell-protocol.c
            ${CMAKE_BINARY_DIR}/src/framebuffer/fractional-scale-v1-protocol.c
            PROPERTIES GENERATED TRUE
        )

        set(TEST_FB_WL_LIBS openrender_common_flags wayland-client pthread openrendercommon ${X11_LIBRARIES})
        set(TEST_FB_WL_INCS
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_BINARY_DIR}
            ${CMAKE_BINARY_DIR}/src/framebuffer
            ${X11_INCLUDE_DIR}
        )
        set(TEST_FB_WL_DEFS HAVE_WAYLAND)

        if(LIBDECOR_FOUND)
            list(APPEND TEST_FB_WL_LIBS ${LIBDECOR_LIBRARIES})
            list(APPEND TEST_FB_WL_INCS ${LIBDECOR_INCLUDE_DIRS})
            list(APPEND TEST_FB_WL_DEFS HAVE_LIBDECOR)
        endif()

        add_executable(test_fb_wl
            test_fb_wl.cpp
            ${CMAKE_SOURCE_DIR}/src/framebuffer/fbwl.cpp
            ${CMAKE_SOURCE_DIR}/src/framebuffer/fbx.cpp
            ${CMAKE_SOURCE_DIR}/src/framebuffer/framebuffer.cpp
            ${CMAKE_BINARY_DIR}/src/framebuffer/xdg-shell-protocol.c
            ${CMAKE_BINARY_DIR}/src/framebuffer/fractional-scale-v1-protocol.c
        )
        target_link_libraries(test_fb_wl PRIVATE ${TEST_FB_WL_LIBS})
        target_include_directories(test_fb_wl PRIVATE ${TEST_FB_WL_INCS})
        target_compile_definitions(test_fb_wl PRIVATE ${TEST_FB_WL_DEFS})

        # Ensure Wayland protocol headers (.h) are generated before this target
        # compiles — they are produced by add_custom_command in src/framebuffer as
        # a side-effect of building the framebuffer target.
        add_dependencies(test_fb_wl framebuffer)

        add_test(NAME WaylandDisplayTests COMMAND test_fb_wl)
        set_tests_properties(WaylandDisplayTests PROPERTIES
            TIMEOUT 10
            LABELS "framebuffer;wayland;linux"
        )
        message(STATUS "  - WaylandDisplayTests (Linux/Wayland)")
    else()
        message(STATUS "  - WaylandDisplayTests (skipped — wayland-client not found)")
    endif()
endif()

# Common display tests — all platforms
add_executable(test_fb_common test_fb_common.cpp)
target_link_libraries(test_fb_common PRIVATE openrender_common_flags openrendercommon)
target_include_directories(test_fb_common PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
add_test(NAME DisplayCommonTests COMMAND test_fb_common)
set_tests_properties(DisplayCommonTests PROPERTIES
    TIMEOUT 10
    LABELS "framebuffer;core"
)
message(STATUS "  - DisplayCommonTests")
