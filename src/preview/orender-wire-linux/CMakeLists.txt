# No cmake_minimum_required() here on purpose: this file is only ever reached
# through add_subdirectory(), and a subdirectory call would reset the policy
# stack, overriding the floor set in the top-level CMakeLists.txt.

if(APPLE)
    return()
endif()

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
# Floors state what this code actually calls, not what happened to be installed
# when it was written. Newest GTK API used is GtkFileDialog (gtk_file_dialog_new/
# save/save_finish/set_initial_name/set_title) -- GTK 4.10. Newest libadwaita API
# is AdwToolbarView -- 1.4. Everything else is 4.0/1.0 era. The previous gtk4>=4.20
# was ten minor versions above anything here and excluded every current Ubuntu LTS
# (24.04 ships 4.14). Note gtk_spinner_new, used below, is deprecated in 4.20.
pkg_check_modules(GTK4 REQUIRED gtk4>=4.10)
pkg_check_modules(ADWAITA REQUIRED libadwaita-1>=1.4)
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
    ${ADWAITA_INCLUDE_DIRS}
    ${EPOXY_INCLUDE_DIRS}
)

target_link_libraries(orender-wire PRIVATE
    ribpreview
    ${GTK4_LIBRARIES}
    ${ADWAITA_LIBRARIES}
    ${EPOXY_LIBRARIES}
    OpenGL::GL
)

target_compile_features(orender-wire PRIVATE cxx_std_20)
target_compile_options(orender-wire PRIVATE ${GTK4_CFLAGS_OTHER} ${ADWAITA_CFLAGS_OTHER})
target_link_options(orender-wire PRIVATE ${GTK4_LDFLAGS_OTHER} ${ADWAITA_LDFLAGS_OTHER})

set_target_properties(orender-wire PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

install(TARGETS orender-wire DESTINATION libexec)
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ../libexec/orender-wire ${CMAKE_INSTALL_PREFIX}/bin/orender-wire)")
