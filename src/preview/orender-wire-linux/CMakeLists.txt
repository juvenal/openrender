cmake_minimum_required(VERSION 3.15)

if(APPLE)
    return()
endif()

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4>=4.20)
pkg_check_modules(EPOXY REQUIRED epoxy)

add_executable(orender-wire
    main.cpp
    arcball.cpp
)

target_include_directories(orender-wire PRIVATE
    ${CMAKE_SOURCE_DIR}/src/preview
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
    ${GTK4_INCLUDE_DIRS}
    ${EPOXY_INCLUDE_DIRS}
)

target_link_libraries(orender-wire PRIVATE
    ribpreview
    ${GTK4_LIBRARIES}
    ${EPOXY_LIBRARIES}
    OpenGL::GL
)

target_compile_features(orender-wire PRIVATE cxx_std_20)
target_compile_options(orender-wire PRIVATE ${GTK4_CFLAGS_OTHER})
target_link_options(orender-wire PRIVATE ${GTK4_LDFLAGS_OTHER})

set_target_properties(orender-wire PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

install(TARGETS orender-wire DESTINATION libexec)
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ../libexec/orender-wire ${CMAKE_INSTALL_PREFIX}/bin/orender-wire)")
