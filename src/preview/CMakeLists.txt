cmake_minimum_required(VERSION 3.15)

# libribpreview — platform-neutral geometry extraction static library
add_subdirectory(libribpreview)

# Platform-specific viewer binaries
if(APPLE)
    add_subdirectory(orender-wire-macos)
elseif(UNIX)
    add_subdirectory(orender-wire-linux)
endif()

message(STATUS "  - preview subsystem (libribpreview + orender-wire)")
