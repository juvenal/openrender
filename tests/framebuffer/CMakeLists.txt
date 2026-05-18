# Test suite for framebuffer drivers

# IPC display driver tests — Linux only (exercises CIPCDisplay spawn/connect path)
if(UNIX AND NOT APPLE)
    # test_fb_ipc tests CIPCDisplay (the unified IPC client). No X11/Wayland deps
    # at the driver level — windowing lives entirely in the orender-fb-linux helper.
    add_executable(test_fb_ipc
        test_fb_ipc.cpp
        ${CMAKE_SOURCE_DIR}/src/framebuffer/fbipc_display.cpp
        ${CMAKE_SOURCE_DIR}/src/framebuffer/framebuffer.cpp
    )
    target_link_libraries(test_fb_ipc PRIVATE openrender_common_flags openrendercommon pthread)
    target_include_directories(test_fb_ipc PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}
    )
    target_compile_features(test_fb_ipc PRIVATE cxx_std_20)

    add_test(NAME IPCDisplayTests COMMAND test_fb_ipc)
    set_tests_properties(IPCDisplayTests PROPERTIES
        TIMEOUT 10
        LABELS "framebuffer;ipc;linux"
    )
    message(STATUS "  - IPCDisplayTests")
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

# CIPCDisplay IPC client unit tests — all platforms (tests fbipc.h packet helpers)
add_executable(test_fbipc_driver test_fbipc_driver.cpp)
target_link_libraries(test_fbipc_driver PRIVATE openrender_common_flags openrendercommon pthread)
target_include_directories(test_fbipc_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_fbipc_driver PRIVATE cxx_std_20)
add_test(NAME FbIPCDriverTests COMMAND test_fbipc_driver)
set_tests_properties(FbIPCDriverTests PROPERTIES
    TIMEOUT 15
    LABELS "framebuffer;ipc;driver"
)
message(STATUS "  - FbIPCDriverTests")

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
