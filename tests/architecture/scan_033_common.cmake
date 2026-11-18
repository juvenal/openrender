# Spec 033: what the three scans of contracts/checks-and-tests.md share (Shading_CoreNoBackendNames,
# Shading_JitMessagesThroughServices, Sloinfo_MetadataApiOnly). include() it from a `cmake -P` script.
#
#   scan_033_file(<file> <relative name> <identifier list> <out variable>)
#       Appends "<relative name>:<line>: <identifier>" to <out variable> for every WHOLE identifier of the list found in the file,
#       in code and in comments, one entry per occurrence. A cheap whole-file test first, so only the few files that hold a hit
#       are read line by line.
#   scan_033_tree(<root> <relative dir> <identifier list> <out variable>)
#       The same over every file under <root>/<relative dir>. It is an error when the directory holds no file (a scan never passes
#       vacuously).

# The lines of a file as a CMake list, one element per line. The characters CMake treats specially in lists (';', '[' and ']') are
# replaced first, so a line is never split or merged; the replacement changes no searched token.
function(scan_033_lines path out)
    file(READ "${path}" _text)
    string(REPLACE ";" "<semi>" _text "${_text}")
    string(REPLACE "[" "<lb>" _text "${_text}")
    string(REPLACE "]" "<rb>" _text "${_text}")
    string(REPLACE "\n" ";" _text "${_text}")
    set(${out} "${_text}" PARENT_SCOPE)
endfunction()

function(scan_033_file path name words out)
    file(READ "${path}" _text)
    set(_candidates "")
    foreach(_w IN LISTS words)
        if(_text MATCHES "(^|[^A-Za-z0-9_])${_w}([^A-Za-z0-9_]|$)")
            list(APPEND _candidates "${_w}")
        endif()
    endforeach()
    if("${_candidates}" STREQUAL "")
        return()
    endif()
    set(_found "")
    scan_033_lines("${path}" _lines)
    set(_n 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _n "${_n} + 1")
        string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*" _tokens "${_line}")
        foreach(_t IN LISTS _tokens)
            list(FIND _candidates "${_t}" _i)
            if(NOT _i EQUAL -1)
                list(APPEND _found "${name}:${_n}: ${_t}")
            endif()
        endforeach()
    endforeach()
    set(${out} "${${out}};${_found}" PARENT_SCOPE)
endfunction()

function(scan_033_tree root dir words out)
    if(NOT IS_DIRECTORY "${root}/${dir}")
        message(FATAL_ERROR "scan: ${root}/${dir} is not a directory")
    endif()
    file(GLOB_RECURSE _files LIST_DIRECTORIES false "${root}/${dir}/*")
    list(LENGTH _files _count)
    if(_count EQUAL 0)
        message(FATAL_ERROR "scan: ${root}/${dir} holds no file (a scan never passes vacuously)")
    endif()
    list(SORT _files)
    set(_acc "${${out}}")
    foreach(_f IN LISTS _files)
        file(RELATIVE_PATH _rel "${root}" "${_f}")
        set(_one "")
        scan_033_file("${_f}" "${_rel}" "${words}" _one)
        set(_acc "${_acc};${_one}")
    endforeach()
    set(${out} "${_acc}" PARENT_SCOPE)
endfunction()

# Prints every hit as "VIOLATION <file>:<line>: <identifier>" and returns the number of hits in <count variable>
function(scan_033_report hits count)
    list(FILTER hits EXCLUDE REGEX "^$")
    list(LENGTH hits _n)
    foreach(_h IN LISTS hits)
        message(STATUS "VIOLATION ${_h}")
    endforeach()
    set(${count} ${_n} PARENT_SCOPE)
endfunction()
