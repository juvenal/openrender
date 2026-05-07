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

# IPC protocol tests — all platforms (requires fbipc.h)
add_executable(test_ipc_protocol test_ipc_protocol.cpp)
target_link_libraries(test_ipc_protocol PRIVATE openrender_common_flags openrendercommon)
target_include_directories(test_ipc_protocol PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_ipc_protocol PRIVATE cxx_std_20)
add_test(NAME IpcProtocolTests COMMAND test_ipc_protocol)
set_tests_properties(IpcProtocolTests PROPERTIES
    TIMEOUT 10
    LABELS "framebuffer;ipc;protocol"
)
message(STATUS "  - IpcProtocolTests")

# CQDisplay (macOS driver) unit tests — all platforms (tests fbipc.h helpers)
add_executable(test_fbq_driver test_fbq_driver.cpp)
target_link_libraries(test_fbq_driver PRIVATE openrender_common_flags openrendercommon pthread)
target_include_directories(test_fbq_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_fbq_driver PRIVATE cxx_std_20)
add_test(NAME FbqDriverTests COMMAND test_fbq_driver)
set_tests_properties(FbqDriverTests PROPERTIES
    TIMEOUT 15
    LABELS "framebuffer;macos;driver"
)
message(STATUS "  - FbqDriverTests")

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

# CXDisplay (Linux X11 IPC client) unit tests — all platforms
add_executable(test_fbx_driver test_fbx_driver.cpp)
target_link_libraries(test_fbx_driver PRIVATE openrender_common_flags openrendercommon pthread)
target_include_directories(test_fbx_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_fbx_driver PRIVATE cxx_std_20)
add_test(NAME FbxDriverTests COMMAND test_fbx_driver)
set_tests_properties(FbxDriverTests PROPERTIES
    TIMEOUT 15
    LABELS "framebuffer;linux;x11;driver"
)
message(STATUS "  - FbxDriverTests")

# CWDisplay (Linux Wayland IPC client) unit tests — all platforms
add_executable(test_fbwl_driver test_fbwl_driver.cpp)
target_link_libraries(test_fbwl_driver PRIVATE openrender_common_flags openrendercommon pthread)
target_include_directories(test_fbwl_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_fbwl_driver PRIVATE cxx_std_20)
add_test(NAME FbwlDriverTests COMMAND test_fbwl_driver)
set_tests_properties(FbwlDriverTests PROPERTIES
    TIMEOUT 15
    LABELS "framebuffer;linux;wayland;driver"
)
message(STATUS "  - FbwlDriverTests")

# Linux helper server logic unit tests — all platforms
add_executable(test_linux_helper_server test_linux_helper_server.cpp)
target_link_libraries(test_linux_helper_server PRIVATE openrender_common_flags openrendercommon pthread)
target_include_directories(test_linux_helper_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_linux_helper_server PRIVATE cxx_std_20)
add_test(NAME LinuxHelperServerTests COMMAND test_linux_helper_server)
set_tests_properties(LinuxHelperServerTests PROPERTIES
    TIMEOUT 15
    LABELS "framebuffer;linux;helper"
)
message(STATUS "  - LinuxHelperServerTests")
