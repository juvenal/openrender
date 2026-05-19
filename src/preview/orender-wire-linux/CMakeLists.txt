cmake_minimum_required(VERSION 3.15)

if(APPLE)
    return()
endif()

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4>=4.22)

add_executable(orender-wire
    main.cpp
    arcball.cpp
)

target_include_directories(orender-wire PRIVATE
    ${CMAKE_SOURCE_DIR}/src/preview
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
    ${GTK4_INCLUDE_DIRS}
)

target_link_libraries(orender-wire PRIVATE
    libribpreview
    ${GTK4_LIBRARIES}
    OpenGL::GL
)

target_compile_features(orender-wire PRIVATE cxx_std_20)
target_compile_options(orender-wire PRIVATE ${GTK4_CFLAGS_OTHER})
target_link_options(orender-wire PRIVATE ${GTK4_LDFLAGS_OTHER})

install(TARGETS orender-wire DESTINATION bin)
