# Spec 037, PluginAbi_InstallLayouts (T102a, SC-011, SC-019): a consumer of an installed openRender.
# It uses find_package(openRender) and nothing else to reach the headers: no include_directories(), no -I.
cmake_minimum_required(VERSION 3.19)
cmake_policy(SET CMP0057 NEW)
project(or_install_consumer C)

find_package(openRender CONFIG REQUIRED)

# Every exported target carries the layout's include directory (its INCLUDES DESTINATION), and so does the package's
# own openRender_INCLUDE_DIR. EXPECT_INCLUDE_DIR is where the layout installs the headers.
get_filename_component(_expected "${EXPECT_INCLUDE_DIR}" REALPATH)
get_filename_component(_variable "${openRender_INCLUDE_DIR}" REALPATH)
if(NOT _variable STREQUAL _expected)
    message(FATAL_ERROR "openRender_INCLUDE_DIR is ${_variable}, the layout installs the headers in ${_expected}")
endif()
foreach(_target RIBRender RIBVector RIBOut shader_shading shader_runtime shader_compiler RIBShadeop RIBCommon)
    get_target_property(_dirs openRender::${_target} INTERFACE_INCLUDE_DIRECTORIES)
    set(_resolved "")
    foreach(_dir IN LISTS _dirs)
        get_filename_component(_real "${_dir}" REALPATH)
        list(APPEND _resolved "${_real}")
    endforeach()
    if(NOT _expected IN_LIST _resolved)
        message(FATAL_ERROR "openRender::${_target} exports include directories [${_resolved}], not ${_expected}")
    endif()
endforeach()

# An object library: it is compiled, never linked, so the imported targets need no library file; it reaches the
# headers through the exported include directory only (no include_directories() of its own, no -I).
add_library(consumer OBJECT consumer.c)
target_include_directories(consumer PRIVATE "${_expected}")
set_target_properties(consumer PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
