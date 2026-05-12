# orender-fb-linux — standalone Linux framebuffer display helper (C++20)
#
# Spawned by the orender framebuffer driver (fbx.cpp / fbwl.cpp) and
# communicates via TLV IPC over a Unix-domain socket.  Handles both X11
# and Wayland window backends; Wayland is selected at runtime when the
# WAYLAND_DISPLAY environment variable is set.

find_package(X11 REQUIRED)
find_package(PkgConfig REQUIRED)

set(HELPER_SRCS main.cpp)
set(HELPER_LIBS ${X11_LIBRARIES} pthread)
set(HELPER_INCS
    ${CMAKE_SOURCE_DIR}/src
    ${X11_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
set(HELPER_DEFS "")

pkg_check_modules(WAYLAND_HELPER wayland-client wayland-protocols>=1.24)
if(WAYLAND_HELPER_FOUND)
    set(XDG_SHELL_XML "/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml")
    set(FRACTIONAL_SCALE_XML "/usr/share/wayland-protocols/staging/fractional-scale/fractional-scale-v1.xml")

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h
        COMMAND wayland-scanner client-header ${XDG_SHELL_XML} ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h
        DEPENDS ${XDG_SHELL_XML}
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
        COMMAND wayland-scanner private-code ${XDG_SHELL_XML} ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
        DEPENDS ${XDG_SHELL_XML}
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fractional-scale-v1-client-protocol.h
        COMMAND wayland-scanner client-header ${FRACTIONAL_SCALE_XML} ${CMAKE_CURRENT_BINARY_DIR}/fractional-scale-v1-client-protocol.h
        DEPENDS ${FRACTIONAL_SCALE_XML}
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fractional-scale-v1-protocol.c
        COMMAND wayland-scanner private-code ${FRACTIONAL_SCALE_XML} ${CMAKE_CURRENT_BINARY_DIR}/fractional-scale-v1-protocol.c
        DEPENDS ${FRACTIONAL_SCALE_XML}
    )

    list(APPEND HELPER_SRCS
        ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h
        ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
        ${CMAKE_CURRENT_BINARY_DIR}/fractional-scale-v1-client-protocol.h
        ${CMAKE_CURRENT_BINARY_DIR}/fractional-scale-v1-protocol.c
    )
    list(APPEND HELPER_INCS ${WAYLAND_HELPER_INCLUDE_DIRS})
    list(APPEND HELPER_LIBS ${WAYLAND_HELPER_LIBRARIES})
    list(APPEND HELPER_DEFS HAVE_WAYLAND)

    pkg_check_modules(LIBDECOR_HELPER libdecor-0)
    if(LIBDECOR_HELPER_FOUND)
        list(APPEND HELPER_INCS ${LIBDECOR_HELPER_INCLUDE_DIRS})
        list(APPEND HELPER_LIBS ${LIBDECOR_HELPER_LIBRARIES})
        list(APPEND HELPER_DEFS HAVE_LIBDECOR)
        message(STATUS "orender-fb-linux: libdecor found — Wayland window decorations enabled")
    else()
        message(STATUS "orender-fb-linux: libdecor not found — Wayland windows will be undecorated on GNOME")
    endif()
endif()

add_executable(orender-fb-linux ${HELPER_SRCS})
target_include_directories(orender-fb-linux PRIVATE ${HELPER_INCS})
target_link_libraries(orender-fb-linux PRIVATE ${HELPER_LIBS})
target_compile_features(orender-fb-linux PRIVATE cxx_std_20)
target_compile_definitions(orender-fb-linux PRIVATE ${HELPER_DEFS})

# Copy binary into the main build output directory for easy development testing
# (mirrors the macOS helper's POST_BUILD copy so ORENDERHOME/bin/ resolution works)
add_custom_command(TARGET orender-fb-linux POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:orender-fb-linux>"
            "${CMAKE_BINARY_DIR}/orender-fb-linux"
    # Also create orender-fb symlink in build root — the name the driver looks for.
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
            orender-fb-linux
            "${CMAKE_BINARY_DIR}/orender-fb"
    COMMENT "Copying orender-fb-linux to build root"
)

# Install binary to libexec/ — it is a helper, not a user-facing command.
# Two symlinks give consistent access:
#   libexec/orender-fb  → orender-fb-linux   (within libexec, for direct invocation)
#   bin/orender-fb      → ../libexec/orender-fb-linux  (picked up by makeHelperPath)
install(TARGETS orender-fb-linux
        RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

install(CODE
    "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
        orender-fb-linux
        \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR}/orender-fb\")
     file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
     execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
        ../libexec/orender-fb-linux
        \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/orender-fb\")"
)
