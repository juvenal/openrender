# Test suite for Wayland framebuffer driver

find_package(X11 REQUIRED)

# Wayland-specific tests
add_executable(test_fb_wl 
    test_fb_wl.cpp 
    ${CMAKE_SOURCE_DIR}/src/framebuffer/fbwl.cpp
    ${CMAKE_SOURCE_DIR}/src/framebuffer/fbx.cpp
    ${CMAKE_SOURCE_DIR}/src/framebuffer/framebuffer.cpp
    ${CMAKE_BINARY_DIR}/src/framebuffer/xdg-shell-protocol.c
    ${CMAKE_BINARY_DIR}/src/framebuffer/fractional-scale-v1-protocol.c
)
target_link_libraries(test_fb_wl PRIVATE openrender_common_flags wayland-client pthread openrendercommon ${X11_LIBRARIES})
target_include_directories(test_fb_wl PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src/framebuffer
    ${X11_INCLUDE_DIR}
)
target_compile_definitions(test_fb_wl PRIVATE HAVE_WAYLAND)

# Common display tests (fallback/detection)
add_executable(test_fb_common test_fb_common.cpp)
target_link_libraries(test_fb_common PRIVATE openrender_common_flags openrendercommon)
target_include_directories(test_fb_common PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
add_test(NAME WaylandDisplayTests COMMAND test_fb_wl)
add_test(NAME DisplayCommonTests COMMAND test_fb_common)

# Set test properties
set_tests_properties(WaylandDisplayTests DisplayCommonTests PROPERTIES
    TIMEOUT 10
    LABELS "framebuffer;wayland;core"
)
