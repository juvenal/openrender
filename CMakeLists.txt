cmake_minimum_required(VERSION 3.15)

project(Pixie
    VERSION 2.2.6
    DESCRIPTION "Open-source RenderMan-compliant renderer"
    LANGUAGES C CXX
)

# Set C++ standard (C++20 required per constitution)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set C standard (C17 for C files)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(PIXIE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(PIXIE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(PIXIE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(PIXIE_VERSION ${PROJECT_VERSION})

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")


# Use standard CMAKE_INSTALL_PREFIX and GNUInstallDirs
include(GNUInstallDirs)

# ###########################################################################
# ########## Options 
# ###########################################################################
option(USE_FLEX_BISON "Use flex and bison to regenerate parsers" ON)
option(BUILD_SHOW "Build the show program" ON)
option(INSTALL_SELFCONTAINED "Build for a selfcontained setup (overrides custom directory settings)" ON)
# Remove obsolete staging options - we now use system package managers

if(USE_FLEX_BISON)
	include(CMake/PixieUseFlex.cmake)
	include(CMake/PixieUseBison.cmake)
endif()


# ###########################################################################
# ########## Compiler Configuration
# ###########################################################################

# Modern compiler flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Platform-specific settings
if(APPLE)
    # Support both Apple Silicon (arm64) and Intel (x86_64)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")
    # Use modern macOS deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version")
endif()

# Create interface library for common flags
add_library(pixie_common_flags INTERFACE)

# Enable modern C++ warnings and optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(pixie_common_flags INTERFACE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )

    # Verify C++20 compiler support
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
        message(FATAL_ERROR "C++20 requires at least GCC 10 or Clang 10 (found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
    endif()

    # Add format security warnings
    target_compile_options(pixie_common_flags INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:-Wformat-security>
    )
endif()

# ###########################################################################
# ########## Making config.h for various platform
# ###########################################################################
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)

set(CMAKE_REQUIRED_INCLUDES math.h)
set(CMAKE_REQUIRED_LIBRARIES m)

# check some include files
check_include_files(malloc.h 						HAVE_MALLOC_H)
check_include_files(stdint.h 						HAVE_STDINT_H)
check_include_files("sys/param.h;sys/mount.h" 		HAVE_SYS_MOUNT_H)
check_include_files(sys/time.h 						HAVE_SYS_TIME_H)

# check libs
check_library_exists(dl			dlopen			"" 	HAVE_DL)
check_library_exists(m			sin				"" 	HAVE_M)
check_library_exists(pthread 	pthread_create 	"" 	HAVE_PTHREAD)

# check functions
check_function_exists(random 						HAVE_RANDOM)
check_function_exists(gettimeofday 					HAVE_GETTIMEOFDAY)
check_function_exists(sqrtf 						HAVE_SQRTF)
check_function_exists(cosf							HAVE_COSF)
check_function_exists(sinf							HAVE_SINF)
check_function_exists(tanf							HAVE_TANF)
check_function_exists(atan2f						HAVE_ATAN2F)
check_function_exists(powf							HAVE_POWF)
check_function_exists(logf							HAVE_LOGF)
check_function_exists(fmodf							HAVE_FMODF)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-cmake.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
# Make config.h available to all targets
target_include_directories(pixie_common_flags INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(pixie_common_flags INTERFACE HAVE_CMAKE_CONFIG_H)

# ###########################################################################
# ########## Dependencies
# ###########################################################################

# Find required packages using modern CMake
find_package(PkgConfig REQUIRED)

# TIFF library
find_package(TIFF REQUIRED)
message(STATUS "Found TIFF: ${TIFF_VERSION}")

# PNG library  
find_package(PNG REQUIRED)
message(STATUS "Found PNG: ${PNG_VERSION}")

# ZLIB (may be keg-only on macOS)
find_package(ZLIB REQUIRED)
message(STATUS "Found ZLIB: ${ZLIB_VERSION}")

# Threads
find_package(Threads REQUIRED)

# Math library (Unix systems)
if(UNIX AND NOT APPLE)
    find_library(MATH_LIBRARY m REQUIRED)
endif()

# Dynamic loading library
if(UNIX)
    find_library(DL_LIBRARY dl REQUIRED)
endif()

# FLTK for GUI (use fltk-config as it doesn't provide pkg-config)
if(BUILD_SHOW)
    find_program(FLTK_CONFIG_EXECUTABLE NAMES fltk-config)
    if(FLTK_CONFIG_EXECUTABLE)
        execute_process(
            COMMAND ${FLTK_CONFIG_EXECUTABLE} --version
            OUTPUT_VARIABLE FLTK_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND ${FLTK_CONFIG_EXECUTABLE} --cflags
            OUTPUT_VARIABLE FLTK_CFLAGS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND ${FLTK_CONFIG_EXECUTABLE} --ldflags
            OUTPUT_VARIABLE FLTK_LDFLAGS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        
        # Create imported target for FLTK
        add_library(FLTK::FLTK INTERFACE IMPORTED)
        separate_arguments(FLTK_CFLAGS)
        separate_arguments(FLTK_LDFLAGS)
        set_target_properties(FLTK::FLTK PROPERTIES
            INTERFACE_COMPILE_OPTIONS "${FLTK_CFLAGS}"
            INTERFACE_LINK_OPTIONS "${FLTK_LDFLAGS}"
        )
        
        message(STATUS "Found FLTK: ${FLTK_VERSION}")
        set(HAVE_FLTK ON)
    else()
        message(WARNING "FLTK not found - show program will not be built")
        set(BUILD_SHOW OFF)
        set(HAVE_FLTK OFF)
    endif()
endif()

# OpenEXR (optional)
find_package(OpenEXR QUIET)
if(OpenEXR_FOUND)
    message(STATUS "Found OpenEXR: ${OpenEXR_VERSION}")
    set(HAVE_OPENEXR ON)
else()
    message(WARNING "OpenEXR not found - OpenEXR display driver will not be built")
    set(HAVE_OPENEXR OFF)
endif()

# ###########################################################################
# ########## Installation Configuration
# ###########################################################################

# Set installation directories using modern CMake approach
if(INSTALL_SELFCONTAINED)
	# Self-contained installation (all files under CMAKE_INSTALL_PREFIX)
	set(PIXIE_DOCDIR ${CMAKE_INSTALL_DOCDIR})
	set(PIXIE_SHADERDIR "shaders")
	set(PIXIE_RIBDIR "ribs") 
	set(PIXIE_TEXTUREDIR "textures")
	set(PIXIE_PROCEDURALDIR "procedurals")
	set(PIXIE_DISPLAYSDIR "displays")
	set(PIXIE_MODULESDIR "modules")
else()
	# System installation (follows FHS)
	set(PIXIE_DOCDIR ${CMAKE_INSTALL_DOCDIR}/Pixie)
	set(PIXIE_SHADERDIR ${CMAKE_INSTALL_DATAROOTDIR}/Pixie/shaders)
	set(PIXIE_RIBDIR ${CMAKE_INSTALL_DATAROOTDIR}/Pixie/ribs)
	set(PIXIE_TEXTUREDIR ${CMAKE_INSTALL_DATAROOTDIR}/Pixie/textures)
	set(PIXIE_PROCEDURALDIR ${CMAKE_INSTALL_LIBDIR}/Pixie/procedurals)
	set(PIXIE_DISPLAYSDIR ${CMAKE_INSTALL_LIBDIR}/Pixie/displays)
	set(PIXIE_MODULESDIR ${CMAKE_INSTALL_LIBDIR}/Pixie/modules)
endif()


# Install documentation
set(doc_files AUTHORS ChangeLog COPYING DEVNOTES INSTALL LICENSE NEWS README)
install(FILES ${doc_files} DESTINATION ${PIXIE_DOCDIR})

# Install man pages  
file(GLOB man_files man/*.1)
install(FILES ${man_files} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# Install shaders
file(GLOB shader_sl_files shaders/*.sl)
file(GLOB shader_sdr_files shaders/*.sdr)
install(FILES ${shader_sl_files} ${shader_sdr_files} DESTINATION ${PIXIE_SHADERDIR})


# ###########################################################################
# ########## Testing
# ###########################################################################

# Enable testing support
enable_testing()

# Add test subdirectory
add_subdirectory(tests)

# ###########################################################################
# ########## Find rules/targets in subdirs
# ###########################################################################
add_subdirectory(src)
