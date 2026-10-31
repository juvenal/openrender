# PluginAbi_InstallHeaders (FR-026, FR-038, SC-011, SC-019). Run as
#   cmake -DHEADERS="<a.h>|<b.h>|..." -DLAYOUTS="<layout>=<include dir>|..." -DSOURCE_DIR=<repo>
#         -DSTAGE=<scratch dir> -DCXX=<c++ compiler> -DFLAGS="<flags>" -DCC=<c compiler> -DC_FLAGS="<flags>"
#         -DEXAMPLE=<examples/display/simpledisplay.c> -DORENDER=<orender> -DCOMMON=<tests/plugins/common>
#         -DWORK=<dir> -P this
#
# The header set is the real OPENRENDER_PUBLIC_HEADERS (CMake/openRenderPublicHeaders.cmake), and LAYOUTS names
# each install layout with the include directory its install rule uses (self-contained: include; FHS:
# include/openRender), taken from the same variables. For each layout the script stages exactly that set into a
# scratch include directory of that name and
#   1. scans every header's #include "..." lines: each must name a staged file (FR-038);
#   2. compiles each header alone, as a translation unit that includes it, as C++20 against the staged directory
#      only; the C headers (not the .hpp ones) also as strict C17 -Wall -Wextra -Wpedantic -Werror;
#   3. compiles the guide's example display, examples/display/simpledisplay.c, as a C17 module against that
#      include directory alone (-I <staged dir>, no other include flag), renders a small scene through it
#      with orender and checks that it wrote a PPM of the right size (SC-011).
# A control translation unit is compiled first with the same flags for each language, so a red result is the
# header, not the toolchain.
#
# Red on the starting tree: examples/display/simpledisplay.c does not exist.
include(${CMAKE_CURRENT_LIST_DIR}/stage_headers.cmake)
include(${COMMON}/render.cmake)

string(REPLACE "|" ";" _names "${HEADERS}")
string(REPLACE "|" ";" _layouts "${LAYOUTS}")
string(REPLACE " " ";" _flags "${FLAGS}")
string(REPLACE " " ";" _c_flags "${C_FLAGS}")
set(_failures "")

foreach(_layout_entry IN LISTS _layouts)
    string(REGEX MATCH "^([^=]+)=(.*)$" _unused "${_layout_entry}")
    set(_layout "${CMAKE_MATCH_1}")
    set(_incdir "${CMAKE_MATCH_2}")
    set(_root "${STAGE}/${_layout}")
    set(_include "${_root}/${_incdir}")

    or_stage_headers("${SOURCE_DIR}" "${_root}" "${_names}" "${_incdir}")
    if(OR_STAGE_MISSING)
        message(FATAL_ERROR "PRECONDITION: the install set names headers the tree does not have: ${OR_STAGE_MISSING}")
    endif()

    # Control: the toolchain compiles a trivial header against the staged directory, in both languages.
    file(WRITE "${_root}/control.h" "#ifndef CONTROL_H\n#define CONTROL_H\nextern int or_control_anchor;\n#endif\n")
    file(WRITE "${_root}/control.c" "#include \"control.h\"\nint or_control_anchor;\n")
    execute_process(
        COMMAND "${CXX}" ${_flags} -std=c++20 -fsyntax-only -x c++ -I "${_root}" -I "${_include}" "${_root}/control.c"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "PRECONDITION: the control translation unit does not compile as C++20:\n${_out}")
    endif()
    execute_process(
        COMMAND "${CC}" ${_c_flags} -std=c17 -Wall -Wextra -Wpedantic -Werror -fsyntax-only -x c -I "${_root}" -I "${_include}" "${_root}/control.c"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "PRECONDITION: the control translation unit does not compile as C17:\n${_out}")
    endif()

    foreach(_name IN LISTS OR_STAGED)
        set(_path "${_include}/${_name}")

        # 1. include closure
        file(READ "${_path}" _text)
        string(REGEX MATCHALL "#[ \t]*include[ \t]*\"[^\"]+\"" _includes "${_text}")
        foreach(_inc IN LISTS _includes)
            string(REGEX REPLACE "#[ \t]*include[ \t]*\"([^\"]+)\"" "\\1" _target "${_inc}")
            if(NOT EXISTS "${_include}/${_target}")
                list(APPEND _failures "${_layout}: ${_name}: #include \"${_target}\" names a header that is not in the installed set")
            endif()
        endforeach()

        # 2. compile alone: a translation unit that includes only this header
        string(REGEX REPLACE "[^A-Za-z0-9]" "_" _id "${_name}")
        file(WRITE "${_root}/tu_${_id}.c" "#include \"${_name}\"\n")
        execute_process(
            COMMAND "${CXX}" ${_flags} -std=c++20 -fsyntax-only -x c++ -I "${_include}" "${_root}/tu_${_id}.c"
            RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
        if(NOT _rc EQUAL 0)
            string(REGEX MATCH "[^\n]*error[^\n]*" _first_error "${_out}")
            list(APPEND _failures "${_layout}: ${_name}: does not compile alone as C++20: ${_first_error}")
        endif()
        if(NOT _name MATCHES "\\.hpp$")
            execute_process(
                COMMAND "${CC}" ${_c_flags} -std=c17 -Wall -Wextra -Wpedantic -Werror -fsyntax-only -x c -I "${_include}" "${_root}/tu_${_id}.c"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
            if(NOT _rc EQUAL 0)
                string(REGEX MATCH "[^\n]*(error|warning)[^\n]*" _first_error "${_out}")
                list(APPEND _failures "${_layout}: ${_name}: does not compile alone as C17: ${_first_error}")
            endif()
        endif()
    endforeach()

    # 3. the guide's example display: built against this layout's include directory alone, and run
    if(NOT EXISTS "${EXAMPLE}")
        list(APPEND _failures "${_layout}: ${EXAMPLE} does not exist: the guide's example display (T103)")
        continue()
    endif()
    set(_module_dir "${_root}/module")
    file(MAKE_DIRECTORY "${_module_dir}")
    if(APPLE)
        set(_link -bundle)
    else()
        set(_link -shared -fPIC)
    endif()
    execute_process(
        COMMAND "${CC}" ${_c_flags} -std=c17 -Wall -Wextra -Wpedantic -Werror ${_link} -I "${_include}"
                -o "${_module_dir}/simpledisplay.dsply" "${EXAMPLE}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if(NOT _rc EQUAL 0)
        string(REGEX MATCH "[^\n]*(error|warning)[^\n]*" _first_error "${_out}")
        list(APPEND _failures "${_layout}: the example display does not build against ${_incdir} alone: ${_first_error}")
        continue()
    endif()
    set(_work "${_root}/render")
    file(MAKE_DIRECTORY "${_work}")
    file(WRITE "${_root}/example.rib" "Option \"searchpath\" \"display\" [\"${_module_dir}\"]\nFormat 8 4 1\nPixelSamples 1 1\nPixelFilter \"box\" 1 1\nProjection \"orthographic\"\nDisplay \"example.ppm\" \"simpledisplay\" \"rgb\"\nWorldBegin\nWorldEnd\n")
    or_render(ORENDER "${ORENDER}" RIB "${_root}/example.rib" WORKDIR "${_work}" LOG "${_work}/example.log" RESULT _rc OUTPUT _out)
    if(NOT "${_rc}" STREQUAL "0")
        list(APPEND _failures "${_layout}: rendering through the example display ended with '${_rc}': ${_out}")
    elseif(NOT EXISTS "${_work}/example.ppm")
        list(APPEND _failures "${_layout}: the example display wrote no example.ppm")
    else()
        file(READ "${_work}/example.ppm" _ppm LIMIT 64)
        if(NOT _ppm MATCHES "^P[36]\n8 4\n")
            list(APPEND _failures "${_layout}: example.ppm does not start with a P3 or P6 header for 8 x 4")
        endif()
    endif()
endforeach()

if(_failures)
    string(REPLACE ";" "\n  " _report "${_failures}")
    message(FATAL_ERROR "DEFECT FR-026/FR-038: the installed header set or the guide's example is wrong:\n  ${_report}")
endif()
