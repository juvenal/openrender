# No cmake_minimum_required() here on purpose: this file is only ever reached
# through add_subdirectory(), and a subdirectory call would reset the policy
# stack, overriding the tiered floor set in the top-level CMakeLists.txt.

# libribpreview — platform-neutral geometry extraction static library
add_subdirectory(libribpreview)

# Platform-specific viewer binaries
if(APPLE)
    add_subdirectory(orender-wire-macos)
elseif(UNIX)
    add_subdirectory(orender-wire-linux)
endif()

message(STATUS "  - preview subsystem (libribpreview + orender-wire)")
