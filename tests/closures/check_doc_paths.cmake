# Closure_DocPaths (spec 035, tasks.md T102, FR-029, the PluginAbi_DocPaths kind). Run as
#   cmake -DSOURCE_DIR=<repo> -P check_doc_paths.cmake
#
# Fails when the documentation of the closure model has drifted from the code:
#   1. DEVNOTES_DETAILS/CLOSURES.md exists and states the closure ABI version and the Bxdf interface version, each in the wording
#        "closure ABI version M.m.p"      and      "Bxdf interface version M.m.p"
#      (case, markup and one line break ignored), and each equals the macro of its header, OR_CLOSURE_ABI_VERSION in closureAbi.h and
#      OR_BXDF_ABI_VERSION in bxdfAbi.h (the history table of each header is the single source; a historical number is written in a table row,
#      never in this form);
#   2. the header history tables carry the current version (a row "| M.m.p |");
#   3. DEVNOTES_DETAILS/RIB_GUIDE.md names the Bxdf statement and closuremode, and DEVNOTES_DETAILS/BINDINGS_GUIDE.md names Bxdf;
#   4. the Hugo page docs/site/content/manual/reference/closures.md exists, names Bxdf and closuremode, and docs/site/content/manual/_index.md
#      links it (the link target reference/closures, not the bare word);
#   5. every backtick token of CLOSURES.md that is a path into the repository (it starts at src/, tests/, specs/, docs/, shaders/, examples/ or
#      DEVNOTES_DETAILS/, has no space, wildcard, brace or placeholder, and either ends in '/' or has a file extension) exists.
# A missing document is a failure, not a skip.
if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "check_doc_paths.cmake: SOURCE_DIR is not set")
endif()
set(_bad "")

function(_read_required out path)
    if(NOT EXISTS "${SOURCE_DIR}/${path}")
        set(_bad "${_bad}\n  missing document: ${path}" PARENT_SCOPE)
        set(${out} "" PARENT_SCOPE)
        return()
    endif()
    file(READ "${SOURCE_DIR}/${path}" _text)
    set(${out} "${_text}" PARENT_SCOPE)
endfunction()

# The M.m.p a header's macro declares: OR_ABI_VERSION(M, m, p)
function(_header_version out header macro)
    file(READ "${SOURCE_DIR}/${header}" _h)
    if(NOT _h MATCHES "#define ${macro} OR_ABI_VERSION\\(([0-9]+), ([0-9]+), ([0-9]+)\\)")
        message(FATAL_ERROR "${header}: no '#define ${macro} OR_ABI_VERSION(M, m, p)'")
    endif()
    set(${out} "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}" PARENT_SCOPE)
    # the history table carries it as a row
    if(NOT _h MATCHES "\\| ${CMAKE_MATCH_1}\\.${CMAKE_MATCH_2}\\.${CMAKE_MATCH_3} \\|")
        set(_bad "${_bad}\n  ${header}: the history table has no row for ${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}" PARENT_SCOPE)
    endif()
endfunction()

_header_version(_closure_version "src/libshader/closures/closureAbi.h" OR_CLOSURE_ABI_VERSION)
_header_version(_bxdf_version "src/libshader/closures/bxdfAbi.h" OR_BXDF_ABI_VERSION)

# 1. the guide and the two versions it quotes
_read_required(_guide "DEVNOTES_DETAILS/CLOSURES.md")
if(NOT _guide STREQUAL "")
    string(REPLACE "`" "" _plain "${_guide}")
    string(REPLACE "*" "" _plain "${_plain}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _plain "${_plain}")
    string(TOLOWER "${_plain}" _plain)
    foreach(_case "closure abi|${_closure_version}" "bxdf interface|${_bxdf_version}")
        string(REPLACE "|" ";" _parts "${_case}")
        list(GET _parts 0 _words)
        list(GET _parts 1 _version)
        string(REGEX MATCHALL "${_words} version [0-9]+\\.[0-9]+\\.[0-9]+" _stated "${_plain}")
        if(NOT _stated)
            set(_bad "${_bad}\n  CLOSURES.md does not state '${_words} version M.m.p'")
        endif()
        foreach(_s IN LISTS _stated)
            string(REGEX REPLACE ".* version ([0-9.]+)$" "\\1" _v "${_s}")
            if(NOT _v STREQUAL _version)
                set(_bad "${_bad}\n  CLOSURES.md states '${_s}', the header's ${_words} version is ${_version}")
            endif()
        endforeach()
    endforeach()

    # 5. path references of the guide
    string(REGEX MATCHALL "`[^`\n]+`" _tokens "${_guide}")
    list(REMOVE_DUPLICATES _tokens)
    foreach(_t IN LISTS _tokens)
        string(REGEX REPLACE "^`(.*)`$" "\\1" _p "${_t}")
        if(_p MATCHES "[ *{}<>$|(),;=]" OR NOT _p MATCHES "^(src|tests|specs|docs|shaders|examples|DEVNOTES_DETAILS)/")
            continue()
        endif()
        if(NOT (_p MATCHES "/$" OR _p MATCHES "\\.[A-Za-z0-9]+$"))
            continue()
        endif()
        if(NOT EXISTS "${SOURCE_DIR}/${_p}")
            set(_bad "${_bad}\n  CLOSURES.md: path does not exist: ${_p}")
        endif()
    endforeach()
endif()

# 3. the statements in the RIB guide and the bindings guide
_read_required(_rib "DEVNOTES_DETAILS/RIB_GUIDE.md")
if(NOT _rib STREQUAL "")
    foreach(_name Bxdf closuremode)
        if(NOT _rib MATCHES "${_name}")
            set(_bad "${_bad}\n  RIB_GUIDE.md does not name ${_name}")
        endif()
    endforeach()
endif()
_read_required(_bindings "DEVNOTES_DETAILS/BINDINGS_GUIDE.md")
if(NOT _bindings STREQUAL "" AND NOT _bindings MATCHES "Bxdf")
    set(_bad "${_bad}\n  BINDINGS_GUIDE.md does not name Bxdf")
endif()

# 4. the Hugo page and its link
_read_required(_page "docs/site/content/manual/reference/closures.md")
if(NOT _page STREQUAL "")
    foreach(_name Bxdf closuremode)
        if(NOT _page MATCHES "${_name}")
            set(_bad "${_bad}\n  the site page does not name ${_name}")
        endif()
    endforeach()
endif()
_read_required(_index "docs/site/content/manual/_index.md")
if(NOT _index STREQUAL "" AND NOT _index MATCHES "reference/closures")
    set(_bad "${_bad}\n  docs/site/content/manual/_index.md does not link the closures page (reference/closures)")
endif()

if(_bad)
    message(FATAL_ERROR "Closure_DocPaths:${_bad}")
endif()
message(STATUS "Closure_DocPaths: closure ABI ${_closure_version}, Bxdf interface ${_bxdf_version}, the guide, the RIB and bindings guides and the site page agree")
