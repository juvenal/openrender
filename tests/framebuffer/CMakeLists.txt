# Test suite for framebuffer drivers

# Wayland tests — Linux only (Wayland does not exist on macOS or Windows)
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND wayland-client)
        pkg_check_modules(LIBDECOR libdecor-0)
    endif()

    # test_fb_wl tests CWDisplay which is now a pure IPC client (no in-process Wayland
    # code). The Wayland protocol C files are only needed by the orender-fb-linux helper
    # binary, not by the driver. We build this test unconditionally on Linux.
    add_executable(test_fb_wl
        test_fb_wl.cpp
        ${CMAKE_SOURCE_DIR}/src/framebuffer/fbwl.cpp
        ${CMAKE_SOURCE_DIR}/src/framebuffer/fbx.cpp
        ${CMAKE_SOURCE_DIR}/src/framebuffer/framebuffer.cpp
    )
    target_link_libraries(test_fb_wl PRIVATE openrender_common_flags openrendercommon ${X11_LIBRARIES} pthread)
    target_include_directories(test_fb_wl PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}
        ${X11_INCLUDE_DIR}
    )
    target_compile_features(test_fb_wl PRIVATE cxx_std_20)

    add_test(NAME WaylandDisplayTests COMMAND test_fb_wl)
    set_tests_properties(WaylandDisplayTests PROPERTIES
        TIMEOUT 10
        LABELS "framebuffer;wayland;linux"
    )
    message(STATUS "  - WaylandDisplayTests")
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
