cmake_minimum_required(VERSION 3.16)

project(openRender
    VERSION 1.0.0
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

set(OPENRENDER_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(OPENRENDER_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(OPENRENDER_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(OPENRENDER_VERSION ${PROJECT_VERSION})

# Optional SemVer metadata (can be set via -D or environment)
set(OPENRENDER_VERSION_PRERELEASE "" CACHE STRING "Pre-release label for openRender (e.g. alpha.1)")
set(OPENRENDER_VERSION_BUILD_METADATA "" CACHE STRING "Build metadata for openRender (e.g. build.20260210)")

if(NOT OPENRENDER_VERSION_PRERELEASE AND DEFINED ENV{OPENRENDER_VERSION_PRERELEASE})
    set(OPENRENDER_VERSION_PRERELEASE "$ENV{OPENRENDER_VERSION_PRERELEASE}")
endif()

if(NOT OPENRENDER_VERSION_BUILD_METADATA AND DEFINED ENV{OPENRENDER_VERSION_BUILD_METADATA})
    set(OPENRENDER_VERSION_BUILD_METADATA "$ENV{OPENRENDER_VERSION_BUILD_METADATA}")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")


# Use standard CMAKE_INSTALL_PREFIX and GNUInstallDirs.
# Default CMAKE_INSTALL_LIBDIR to 'lib' so installs are consistent across
# platforms — GNUInstallDirs would otherwise pick 'lib64' on Fedora/RHEL
# multilib systems. Users who need lib64 can pass -DCMAKE_INSTALL_LIBDIR=lib64.
if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Object code libraries installation directory")
endif()
include(GNUInstallDirs)

# ###########################################################################
# ########## Options
# ###########################################################################
option(USE_FLEX_BISON "Use flex and bison to regenerate parsers" ON)
option(BUILD_SHOW "Build the show program" ON)
option(INSTALL_SELFCONTAINED "Build for a selfcontained setup (overrides custom directory settings)" ON)
# Remove obsolete staging options - we now use system package managers

# SOVERSION defaults to the current major version.
# All 1.x.y releases share SOVERSION=1 (ABI-compatible series).
# Set to an older major to keep compatibility with a previously deployed ABI.
set(OPENRENDER_COMPAT_SOVERSION "${PROJECT_VERSION_MAJOR}" CACHE STRING
    "SOVERSION for libri/librslo — defaults to current major; override to stay compatible with an older ABI")

if(USE_FLEX_BISON)
	include(CMake/openRenderUseFlex.cmake)
	include(CMake/openRenderUseBison.cmake)
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
    # Note: Universal binaries require universal versions of all dependencies (TIFF, PNG, FLTK, etc.)
    # If dependencies are single-arch, comment out the architecture you don't have
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")
    endif()
    # Use modern macOS deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version")
endif()

# Create interface library for common flags
add_library(openrender_common_flags INTERFACE)

# Enable modern C++ warnings and optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(openrender_common_flags INTERFACE
        -Wall -Wextra
        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )

    # Verify C++20 compiler support
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
        message(FATAL_ERROR "C++20 requires at least GCC 10 or Clang 10 (found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
    endif()

    # Add format security warnings
    target_compile_options(openrender_common_flags INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:-Wformat-security>
    )

    # Clang omits -Wunused-but-set-variable from -Wextra; add it explicitly so
    # both Clang and GCC enforce the same diagnostic coverage.
    target_compile_options(openrender_common_flags INTERFACE
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wunused-but-set-variable>
    )

    # Clang omits -Wimplicit-fallthrough from -Wextra; add it explicitly so
    # all targets (not just oshader) catch unannotated switch fall-throughs on macOS.
    target_compile_options(openrender_common_flags INTERFACE
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wimplicit-fallthrough>
    )

    # Clang's equivalent of GCC's -Wsequence-point; catches undefined behavior
    # from reading and modifying a variable in the same unsequenced expression.
    target_compile_options(openrender_common_flags INTERFACE
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wunsequenced>
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

# Compute SemVer suffixes for the string form
set(SEMVER_PRERELEASE_SUFFIX "")
if(OPENRENDER_VERSION_PRERELEASE)
    set(SEMVER_PRERELEASE_SUFFIX "-${OPENRENDER_VERSION_PRERELEASE}")
endif()

set(SEMVER_BUILD_SUFFIX "")
if(OPENRENDER_VERSION_BUILD_METADATA)
    set(SEMVER_BUILD_SUFFIX "+${OPENRENDER_VERSION_BUILD_METADATA}")
endif()

string(TOLOWER "${CMAKE_PROJECT_NAME}" OPENRENDER_PACKAGE_NAME)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/common/version_config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/version_config.h @ONLY)

# Make generated and project headers available to all targets
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/includes)
target_include_directories(openrender_common_flags INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/includes
)
target_compile_definitions(openrender_common_flags INTERFACE HAVE_CONFIG_H)

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

# Define openrendertiff_libs for use in subdirectories
set(openrendertiff_libs TIFF::TIFF PNG::PNG)

# Threads
# Prefer -pthread compiler/link flag over -lpthread to avoid duplicate
# library warnings when linking against FLTK (which adds -lpthread via fltk-config).
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# On Linux, all code must be position-independent so static libs can be linked
# into shared modules (.dsply/.dso/.so). On x86-64 this has no measurable runtime cost.
if(UNIX AND NOT APPLE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

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
        # Use SHELL: prefix to prevent CMake from splitting framework arguments
        set_target_properties(FLTK::FLTK PROPERTIES
            INTERFACE_COMPILE_OPTIONS "${FLTK_CFLAGS}"
            INTERFACE_LINK_OPTIONS "SHELL:${FLTK_LDFLAGS}"
        )

        message(STATUS "Found FLTK: ${FLTK_VERSION}")
        set(HAVE_FLTK ON)
    else()
        message(WARNING "FLTK not found - oshow program will not be built")
        set(BUILD_SHOW OFF)
        set(HAVE_FLTK OFF)
    endif()
endif()

# OpenEXR (optional) - requires Imath for version 3.x
find_package(OpenEXR QUIET CONFIG)
if(OpenEXR_FOUND)
    # OpenEXR 3.x also requires Imath
    find_package(Imath REQUIRED CONFIG)
    message(STATUS "Found OpenEXR: ${OpenEXR_VERSION}")
    message(STATUS "Found Imath: ${Imath_VERSION}")
    set(HAVE_OPENEXR ON)
else()
    message(WARNING "OpenEXR not found - OpenEXR display driver will not be built")
    set(HAVE_OPENEXR OFF)
endif()

# ###########################################################################
# ########## LLVM (optional — required only for JIT shader compilation)
# ###########################################################################
# Hint for Homebrew LLVM on macOS (llvm-config may not be in PATH)
if(APPLE AND NOT DEFINED LLVM_DIR)
    find_program(_LLVM_CONFIG NAMES llvm-config
        HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)
    if(_LLVM_CONFIG)
        execute_process(COMMAND ${_LLVM_CONFIG} --cmakedir
            OUTPUT_VARIABLE _LLVM_CMAKE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_LLVM_CMAKE_DIR AND NOT LLVM_DIR)
            set(LLVM_DIR "${_LLVM_CMAKE_DIR}" CACHE PATH "LLVM cmake dir (auto-detected)" FORCE)
        endif()
    elseif(EXISTS "/opt/homebrew/opt/llvm/lib/cmake/llvm/LLVMConfig.cmake" AND NOT LLVM_DIR)
        set(LLVM_DIR "/opt/homebrew/opt/llvm/lib/cmake/llvm" CACHE PATH "LLVM cmake dir (Homebrew fallback)" FORCE)
    endif()
endif()
find_package(LLVM CONFIG QUIET)
if(LLVM_FOUND)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    set(OPENRENDER_HAVE_LLVM ON)
    set(OPENRENDER_LLVM_VERSION_MAJOR ${LLVM_VERSION_MAJOR})
    set(OPENRENDER_LLVM_VERSION_MINOR ${LLVM_VERSION_MINOR})
else()
    message(STATUS "LLVM not found -- JIT shader path disabled")
    set(OPENRENDER_HAVE_LLVM OFF)
    set(OPENRENDER_LLVM_VERSION_MAJOR 0)
    set(OPENRENDER_LLVM_VERSION_MINOR 0)
endif()

# ###########################################################################
# ########## Installation Configuration
# ###########################################################################

# Set installation directories using modern CMake approach
if(INSTALL_SELFCONTAINED)
	# Self-contained installation (all files under CMAKE_INSTALL_PREFIX)
	set(OPENRENDER_HOMEDIR "${CMAKE_INSTALL_PREFIX}")
	set(OPENRENDER_DOCDIR ${CMAKE_INSTALL_DOCDIR})
	set(OPENRENDER_SHADERDIR "shaders")
	set(OPENRENDER_RIBDIR "ribs")
	set(OPENRENDER_TEXTUREDIR "textures")
	set(OPENRENDER_GEOMETRYDIR "geometry")
	set(OPENRENDER_PROCEDURALDIR "procedurals")
	set(OPENRENDER_DISPLAYSDIR "displays")
	set(OPENRENDER_MODULESDIR "modules")
	set(OPENRENDER_PYTHONDIR "python"
	    CACHE PATH "Install destination for prman.py Python binding")
	set(OPENRENDER_LUADIR "lua"
	    CACHE PATH "Install destination for prman.lua Lua binding")
else()
	# System installation (follows FHS)
	set(OPENRENDER_HOMEDIR "${CMAKE_INSTALL_DATAROOTDIR}/openRender")
	set(OPENRENDER_DOCDIR ${CMAKE_INSTALL_DOCDIR}/openRender)
	set(OPENRENDER_SHADERDIR ${CMAKE_INSTALL_DATAROOTDIR}/openRender/shaders)
	set(OPENRENDER_RIBDIR ${CMAKE_INSTALL_DATAROOTDIR}/openRender/ribs)
	set(OPENRENDER_TEXTUREDIR ${CMAKE_INSTALL_DATAROOTDIR}/openRender/textures)
	set(OPENRENDER_GEOMETRYDIR ${CMAKE_INSTALL_DATAROOTDIR}/openRender/geometry)
	set(OPENRENDER_PROCEDURALDIR ${CMAKE_INSTALL_LIBDIR}/openRender/procedurals)
	set(OPENRENDER_DISPLAYSDIR ${CMAKE_INSTALL_LIBDIR}/openRender/displays)
	set(OPENRENDER_MODULESDIR ${CMAKE_INSTALL_LIBDIR}/openRender/modules)
	set(OPENRENDER_PYTHONDIR "${CMAKE_INSTALL_DATAROOTDIR}/openRender/python"
	    CACHE PATH "Install destination for prman.py Python binding")
	set(OPENRENDER_LUADIR "${CMAKE_INSTALL_DATAROOTDIR}/openRender/lua"
	    CACHE PATH "Install destination for prman.lua Lua binding")
endif()


# Configure headers generated by CMake — must come AFTER install-dir variables are set above
# so that @OPENRENDER_DISPLAYSDIR@ etc. are correctly substituted in config.h.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-cmake.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# RPATH: for self-contained installs, executables in bin/ must find shared libs in lib/.
# CMAKE_INSTALL_RPATH propagates automatically to every installed executable target.
if(INSTALL_SELFCONTAINED)
    if(APPLE)
        set(CMAKE_INSTALL_RPATH "@loader_path/../lib")
    else()
        set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    endif()
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
endif()

# Install .orenderrc into ORENDERHOME so runtime finds default renderer configuration
set(ORENDERRC_SRC "${CMAKE_SOURCE_DIR}/.orenderrc")
if(NOT EXISTS "${ORENDERRC_SRC}")
	set(ORENDERRC_SRC "${CMAKE_SOURCE_DIR}/src/.orenderrc")
endif()
if(EXISTS "${ORENDERRC_SRC}")
	install(FILES "${ORENDERRC_SRC}" DESTINATION "${OPENRENDER_HOMEDIR}" RENAME ".orenderrc")
else()
	message(WARNING "No .orenderrc found at project root or src/; install will not deploy default renderer config")
endif()

# Install documentation
set(doc_files AUTHORS.md ChangeLog.md COPYING.md DEVNOTES.md INSTALL.md LICENSE.md NEWS.md README.md)
install(FILES ${doc_files} DESTINATION ${OPENRENDER_DOCDIR})

# Install man pages
file(GLOB man_files man/*.1)
install(FILES ${man_files} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# Install shaders
file(GLOB shader_sl_files shaders/*.sl)
file(GLOB shader_sdr_files shaders/*.sdr)
file(GLOB shader_rslo_files shaders/*.rslo)
install(FILES ${shader_sl_files} ${shader_sdr_files} ${shader_rslo_files} DESTINATION ${OPENRENDER_SHADERDIR})

# Install geometry files
file(GLOB geometry_rib_files geometry/*.rib)
install(FILES ${geometry_rib_files} DESTINATION ${OPENRENDER_GEOMETRYDIR})


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

# ###########################################################################
# ########## Self-contained dependency bundling
# ###########################################################################
# For INSTALL_SELFCONTAINED builds, copy any non-OS shared libraries (e.g.
# libtiff, libpng, OpenEXR from Homebrew) into PREFIX/lib/ at install time,
# then rewrite their embedded IDs and cross-references on macOS so the entire
# install directory is fully relocatable.
#
# Runs after all library and executable install steps (add_subdirectory(src) above).
# Scans installed files at their final PREFIX locations.
if(INSTALL_SELFCONTAINED)
    # CMAKE_SHARED_LIBRARY_SUFFIX is not available inside install(CODE), so bake it in.
    set(_sc_shared_suffix "${CMAKE_SHARED_LIBRARY_SUFFIX}")

    install(CODE "
        cmake_policy(SET CMP0087 NEW)
        set(_prefix  \"${CMAKE_INSTALL_PREFIX}\")
        set(_bindir  \"${CMAKE_INSTALL_BINDIR}\")
        set(_libdir  \"${CMAKE_INSTALL_LIBDIR}\")
        set(_binpath \"\${_prefix}/\${_bindir}\")
        set(_libpath \"\${_prefix}/\${_libdir}\")

        # Collect installed executables that exist (some may be optional).
        set(_scan_exes)
        foreach(_exe orender oshader otexmake rsloinfo oshow)
            if(EXISTS \"\${_binpath}/\${_exe}\")
                list(APPEND _scan_exes \"\${_binpath}/\${_exe}\")
            endif()
        endforeach()

        # Collect installed shared libraries.
        set(_scan_libs)
        foreach(_lib libri librslo)
            set(_full \"\${_libpath}/\${_lib}${_sc_shared_suffix}\")
            if(EXISTS \"\${_full}\")
                list(APPEND _scan_libs \"\${_full}\")
            endif()
        endforeach()

        if(NOT _scan_exes AND NOT _scan_libs)
            message(WARNING \"Bundling: no installed executables or libraries found in \${_prefix}\")
            return()
        endif()

        # Resolve non-OS dynamic dependencies.
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES \${_scan_exes}
            LIBRARIES   \${_scan_libs}
            RESOLVED_DEPENDENCIES_VAR   _resolved
            UNRESOLVED_DEPENDENCIES_VAR _unresolved
            PRE_EXCLUDE_REGEXES
                \"^/usr/lib\" \"^/lib\" \"^/System\" \"^/usr/libexec\"
                \"libSystem\" \"libobjc\" \"libc\\\\+\" \"libstdc\" \"libgcc_s\"
                \"libc\\\\.\" \"libm\\\\.\" \"libpthread\" \"libdl\\\\.\"
                \"ld-linux\" \"librt\\\\.\" \"libz\\\\.\"
            POST_EXCLUDE_REGEXES
                \"^/usr/lib\" \"^/lib\" \"^/System\"
        )

        foreach(_dep \${_resolved})
            string(FIND \"\${_dep}\" \"\${_prefix}\" _in_prefix)
            if(_in_prefix EQUAL -1)
                message(STATUS \"Bundling: \${_dep}\")
                file(INSTALL DESTINATION \"\${_libpath}\"
                     TYPE SHARED_LIBRARY FOLLOW_SYMLINK_CHAIN FILES \"\${_dep}\")
            endif()
        endforeach()

        if(_unresolved)
            message(WARNING \"Unresolved runtime deps (manual bundling may be needed): \${_unresolved}\")
        endif()

        if(APPLE)
            # Fix up all installed dylibs on macOS:
            #   * Bundled external libs: rewrite their embedded ID to @rpath/<name>.
            #     Only real files get -id; symlinks point to the same inode so we
            #     skip them to avoid overwriting the SONAME with the unversioned name.
            #   * All dylibs: replace every absolute Homebrew reference with @rpath/<name>.
            #   * Executables: skip install_name_tool to preserve code signatures.
            #     Executables only reference libri/librslo (our own libs, already @rpath)
            #     thanks to PRIVATE linkage of external deps in libri's CMakeLists.
            #   * After modifying any binary, re-sign ad-hoc (required on Apple Silicon
            #     when load commands change).

            set(_modified_bins)

            # 1. Rewrite IDs of bundled external libs (real files only, not symlinks)
            foreach(_dep \${_resolved})
                get_filename_component(_dep_name \"\${_dep}\" NAME)
                set(_dest \"\${_libpath}/\${_dep_name}\")
                if(EXISTS \"\${_dest}\")
                    execute_process(
                        COMMAND bash -c \"test -L '\${_dest}'\"
                        RESULT_VARIABLE _is_symlink)
                    if(NOT _is_symlink EQUAL 0)
                        execute_process(
                            COMMAND install_name_tool -id \"@rpath/\${_dep_name}\" \"\${_dest}\"
                            ERROR_QUIET)
                        list(APPEND _modified_bins \"\${_dest}\")
                    endif()
                endif()
            endforeach()

            # 2. Replace Homebrew paths in all installed dylibs
            file(GLOB _installed_libs \"\${_libpath}/*.dylib\")
            foreach(_lib \${_installed_libs})
                execute_process(
                    COMMAND bash -c \"test -L '\${_lib}'\"
                    RESULT_VARIABLE _is_symlink)
                if(NOT _is_symlink EQUAL 0)
                    set(_changed FALSE)
                    foreach(_dep \${_resolved})
                        get_filename_component(_dep_name \"\${_dep}\" NAME)
                        execute_process(
                            COMMAND install_name_tool
                                -change \"\${_dep}\" \"@rpath/\${_dep_name}\" \"\${_lib}\"
                            RESULT_VARIABLE _chg_result
                            ERROR_QUIET)
                        if(_chg_result EQUAL 0)
                            set(_changed TRUE)
                        endif()
                    endforeach()
                    if(_changed)
                        list(APPEND _modified_bins \"\${_lib}\")
                    endif()
                endif()
            endforeach()

            # 3. Re-sign all modified binaries (required on Apple Silicon after
            #    any load-command modification — ad-hoc signing suffices for CLI tools)
            list(REMOVE_DUPLICATES _modified_bins)
            foreach(_bin \${_modified_bins})
                execute_process(
                    COMMAND codesign --force --sign - \"\${_bin}\"
                    ERROR_QUIET)
            endforeach()
        endif()
    ")
endif()
