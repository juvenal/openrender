# PluginAbi_InstallLayouts (T102a, FR-026, FR-048, SC-011, SC-019). Run as
#   cmake -DSOURCE_DIR=<repo> -DMAIN_BUILD=<the main build tree> -DSCRATCH=<dir under the build tree>
#         -DHEADERS="<a.h>|<b.h>|..." -DINCLUDE_SELFCONTAINED=<dir> -DINCLUDE_FHS=<dir>
#         -DCMAKE_COMMAND=<cmake> -P this
#
# For each install layout, self-contained (INSTALL_SELFCONTAINED=ON, the default) and FHS (OFF):
#   1. configures a scratch tree whose CMAKE_INSTALL_PREFIX is a directory under SCRATCH (the cache entries
#      that matter are copied from the main build tree), so no install rule, relative or absolute, can write
#      outside the build tree (the default prefix, /usr/local, is never used: GitHub #23);
#   2. runs the "Development" component install of that tree (the public headers, the CMake package files):
#      `cmake --install <tree> --component Development`, which needs no build of the libraries;
#   3. checks that every path the install manifest lists is inside the scratch prefix; that the layout's
#      include directory holds exactly the public header list (no file_base.h, nothing else); that for the FHS
#      layout nothing but the openRender directory lands directly in <prefix>/include;
#   4. configures a consumer that uses find_package(openRender), checks that openRender_INCLUDE_DIR and the
#      INTERFACE_INCLUDE_DIRECTORIES of every exported shared target are the layout's include directory, and
#      compiles #include "dsply.h" / "orplugin.h" with that directory and no other include flag. The per-configuration targets files (which name the installed libraries) are removed
#      first: the consumer is an OBJECT library, it needs the include directories of the imported targets and
#      no library file, and the Development component does not install the libraries.
# The include directory of each layout comes from the variables the install rules use (INCLUDE_SELFCONTAINED
# and INCLUDE_FHS, CMake/openRenderPublicHeaders.cmake).
#
# Red on the starting tree: the headers install into ${CMAKE_INSTALL_INCLUDEDIR} in both layouts, with no
# Development component, so the component install installs nothing.
cmake_policy(SET CMP0057 NEW) # if(... IN_LIST ...) in script mode, whatever the CMake version

string(REPLACE "|" ";" _names "${HEADERS}")
set(_failures "")

if(NOT EXISTS "${MAIN_BUILD}/CMakeCache.txt")
    message(FATAL_ERROR "PRECONDITION: ${MAIN_BUILD}/CMakeCache.txt does not exist")
endif()
get_filename_component(_scratch "${SCRATCH}" ABSOLUTE)
get_filename_component(_main "${MAIN_BUILD}" ABSOLUTE)
string(FIND "${_scratch}/" "${_main}/" _inside)
if(NOT _inside EQUAL 0)
    message(FATAL_ERROR "PRECONDITION: the scratch directory ${_scratch} is not under the build tree ${_main}")
endif()

# The cache entries a nested configure needs: toolchain, vcpkg, JIT and tool locations. Semicolons in a value
# are protected while the lines are split.
file(READ "${MAIN_BUILD}/CMakeCache.txt" _cache)
string(REPLACE ";" "<SEMI>" _cache "${_cache}")
string(REGEX MATCHALL "(^|\n)[A-Za-z0-9_.+-]+:(STRING|BOOL|PATH|FILEPATH)=[^\n]*" _entries "${_cache}")
set(_init "")
foreach(_entry IN LISTS _entries)
    string(STRIP "${_entry}" _entry)
    if(_entry MATCHES "^([A-Za-z0-9_.+-]+):(STRING|BOOL|PATH|FILEPATH)=(.*)$")
        set(_key "${CMAKE_MATCH_1}")
        set(_type "${CMAKE_MATCH_2}")
        set(_value "${CMAKE_MATCH_3}")
        if(_key MATCHES "^(CMAKE_BUILD_TYPE|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|CMAKE_C_FLAGS.*|CMAKE_CXX_FLAGS.*|CMAKE_OSX_.*|CMAKE_TOOLCHAIN_FILE|CMAKE_PREFIX_PATH|CMAKE_MAKE_PROGRAM|VCPKG_.*|OPENRENDER_.*|BISON_EXECUTABLE|FLEX_EXECUTABLE|USE_FLEX_BISON|HAVE_.*)$")
            string(REPLACE "<SEMI>" ";" _value "${_value}")
            string(APPEND _init "set(\"${_key}\" [=[${_value}]=] CACHE ${_type} \"\" FORCE)\n")
        endif()
    endif()
endforeach()
string(REGEX MATCH "CMAKE_GENERATOR:INTERNAL=([^\n]*)" _g "${_cache}")
set(_generator "${CMAKE_MATCH_1}")
string(REGEX MATCH "CMAKE_C_COMPILER:FILEPATH=([^\n]*)" _c "${_cache}")
set(_cc "${CMAKE_MATCH_1}")

foreach(_layout selfcontained fhs)
    if(_layout STREQUAL "selfcontained")
        set(_selfcontained ON)
        set(_include_dir "${INCLUDE_SELFCONTAINED}")
    else()
        set(_selfcontained OFF)
        set(_include_dir "${INCLUDE_FHS}")
    endif()
    set(_root "${_scratch}/${_layout}")
    set(_tree "${_root}/tree")
    set(_prefix "${_root}/prefix")
    file(REMOVE_RECURSE "${_root}")
    file(MAKE_DIRECTORY "${_root}")
    file(WRITE "${_root}/initial-cache.cmake" "${_init}")

    # 1. configure
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${SOURCE_DIR}" -B "${_tree}" -G "${_generator}" -C "${_root}/initial-cache.cmake"
                -DINSTALL_SELFCONTAINED=${_selfcontained} "-DCMAKE_INSTALL_PREFIX=${_prefix}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out TIMEOUT 900)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "PRECONDITION: the scratch configure for the ${_layout} layout failed (${_rc}); the check cannot tell a toolchain problem from the defect:\n${_out}")
    endif()

    # 2. install the Development component into the scratch prefix
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --install "${_tree}" --component Development
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out TIMEOUT 300)
    if(NOT _rc EQUAL 0)
        list(APPEND _failures "${_layout}: cmake --install --component Development failed (${_rc}): ${_out}")
    endif()

    # 3. the manifest, the include directory
    set(_manifest "${_tree}/install_manifest_Development.txt")
    set(_installed "")
    if(EXISTS "${_manifest}")
        file(STRINGS "${_manifest}" _installed)
    endif()
    if(NOT _installed)
        list(APPEND _failures "${_layout}: the Development component installed nothing: the public headers and the package files must carry COMPONENT Development")
        continue()
    endif()
    foreach(_file IN LISTS _installed)
        string(FIND "${_file}" "${_prefix}/" _at)
        if(NOT _at EQUAL 0)
            list(APPEND _failures "${_layout}: ${_file} was installed outside the scratch prefix ${_prefix}")
        endif()
    endforeach()

    set(_dir "${_prefix}/${_include_dir}")
    file(GLOB _found RELATIVE "${_dir}" "${_dir}/*")
    list(SORT _found)
    set(_expected ${_names})
    list(SORT _expected)
    if(NOT "${_found}" STREQUAL "${_expected}")
        string(REPLACE ";" " " _found_text "${_found}")
        string(REPLACE ";" " " _expected_text "${_expected}")
        list(APPEND _failures "${_layout}: ${_include_dir} holds [${_found_text}], the public header list is [${_expected_text}]")
    endif()
    if("file_base.h" IN_LIST _found)
        list(APPEND _failures "${_layout}: file_base.h is installed")
    endif()
    if(NOT _selfcontained)
        file(GLOB _top RELATIVE "${_prefix}/include" "${_prefix}/include/*")
        foreach(_entry IN LISTS _top)
            if(NOT _entry STREQUAL "openRender")
                list(APPEND _failures "fhs: ${_entry} lands directly in <prefix>/include; only openRender/ may")
            endif()
        endforeach()
    endif()

    # 4. the consumer
    file(GLOB _per_config "${_prefix}/lib/cmake/openRender/openRenderTargets-*.cmake" "${_prefix}/lib/cmake/openRender/openRenderStaticTargets-*.cmake")
    if(_per_config)
        file(REMOVE ${_per_config})
    endif()
    set(_consumer "${_root}/consumer")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/install_consumer" -B "${_consumer}" -G "${_generator}"
                "-DCMAKE_PREFIX_PATH=${_prefix}" "-DCMAKE_C_COMPILER=${_cc}" "-DEXPECT_INCLUDE_DIR=${_prefix}/${_include_dir}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out TIMEOUT 300)
    if(NOT _rc EQUAL 0)
        list(APPEND _failures "${_layout}: find_package(openRender) failed in the consumer: ${_out}")
    else()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" --build "${_consumer}"
            RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out TIMEOUT 300)
        if(NOT _rc EQUAL 0)
            string(REGEX MATCH "[^\n]*(error|Error)[^\n]*" _first "${_out}")
            list(APPEND _failures "${_layout}: the consumer does not compile with find_package(openRender) alone: ${_first}")
        endif()
    endif()
    message("${_layout}: include directory ${_include_dir}, manifest ${_manifest}")
endforeach()

if(_failures)
    string(REPLACE ";" "\n  " _report "${_failures}")
    message(FATAL_ERROR "DEFECT FR-026/SC-019: the scratch installs are wrong:\n  ${_report}")
endif()
