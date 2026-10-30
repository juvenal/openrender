# The public headers include from strict C17 and from C++20 (spec 037, FR-005, SC-003, US2-1/2):
#
#   PluginAbi_HeadersC17    each of orplugin.h shadeop.h dlo.h implicit.h dsply.h alone, and all of them
#                           in two orders, as -std=c17 -Wall -Wextra -Wpedantic -Werror
#   PluginAbi_HeadersCxx20  the same set as C++20 with the project's warnings
#
# cmake -P compile scripts registered as ordinary ctest tests, so a red check never breaks the -Werror
# main build. The header directories are arguments (T055 and T066 change them when a header moves).
# Red on the starting tree: orplugin.h does not exist and dlo.h, implicit.h, dsply.h open `extern "C" {`
# unguarded. (The C half of ShadeopAbi_SignatureMatches is in shadeop/.)

set(_header_set orplugin.h shadeop.h dlo.h implicit.h dsply.h)
string(REPLACE ";" "|" _headers "${_header_set}")
# The directories the headers live in, by the layout of the tree at configure time.
set(_header_dirs "")
foreach(_dir src/include src/ri/render src/ri/geometry src/display src/shadeop src/procedures)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${_dir})
        list(APPEND _header_dirs ${CMAKE_SOURCE_DIR}/${_dir})
    endif()
endforeach()
string(REPLACE ";" "|" _dirs "${_header_dirs}")

set(_toolchain "${CMAKE_CXX_FLAGS}")
set(_c_toolchain "${CMAKE_C_FLAGS}")
if(CMAKE_OSX_SYSROOT)
    set(_toolchain "${_toolchain} -isysroot ${CMAKE_OSX_SYSROOT}")
    set(_c_toolchain "${_c_toolchain} -isysroot ${CMAKE_OSX_SYSROOT}")
endif()

or_plugin_add_test(PluginAbi_HeadersC17 120
    COMMAND ${CMAKE_COMMAND}
        -DLANG=c17
        -DCOMPILER=${CMAKE_C_COMPILER}
        "-DFLAGS=${_c_toolchain}"
        "-DINCLUDE_DIRS=${_dirs}"
        "-DHEADERS=${_headers}"
        -DWORKDIR=${OR_PLUGIN_BIN}/headers/work/c17
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_headers.cmake)

or_plugin_add_test(PluginAbi_HeadersCxx20 120
    COMMAND ${CMAKE_COMMAND}
        -DLANG=cxx20
        -DCOMPILER=${CMAKE_CXX_COMPILER}
        "-DFLAGS=${_toolchain}"
        "-DINCLUDE_DIRS=${_dirs}"
        "-DHEADERS=${_headers}"
        -DWORKDIR=${OR_PLUGIN_BIN}/headers/work/cxx20
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_headers.cmake)
