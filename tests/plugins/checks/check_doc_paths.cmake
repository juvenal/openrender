# PluginAbi_DocPaths (FR-036). Run as
#   cmake -DDOCS="<doc>|<doc>|..." -DHEADERS="<a.h>|<b.h>|..." -DSOURCE_DIR=<repo> -DSTAGE=<scratch dir>
#         -P this
#
# Extracts every *path reference* from the given documents and fails on one that does not exist.
#
# A path reference is
#   * a backtick token (`...`, outside fenced code blocks), or a Markdown link target ](...),
#     that is path-like: it contains '/' and either ends in '/', has a file extension, or starts
#     at a top-level entry of the repository; or it has no '/' but ends in a source, header,
#     document or CMake extension (.c .cc .cpp .cxx .h .hh .hpp .y .l .md .cmake) or is
#     CMakeLists.txt;
#   * the file in an #include "..." or #include <...> line anywhere (fenced code included): an
#     include-form path.
#
# It *exists* when
#   * (include-form) it resolves inside the staged include directory: <STAGE>/include/<path>,
#     where the staging holds exactly the installed header set (-DHEADERS), so a guide that tells
#     a plug-in author to include a header the install does not provide fails;
#   * (<prefix>-form, <prefix>/x) it resolves inside <STAGE>/x;
#   * otherwise it resolves relative to the repository root or to the document's own directory,
#     or, for a bare file name, inside the staged include directory or as a file of that name
#     somewhere under src/, tests/ or examples/.
#
# Not path references of this check (counted and left to the scratch-install check of Phase 7):
# absolute install paths (/usr/local/...), tokens starting with '$' (CMake and shell variables), '~',
# <prefix> paths other than <prefix>/include/..., URLs, and tokens with spaces, wildcards or operators.
#
# Documents that do not exist are skipped by the caller (PLUGIN_ABI.md and the site page exist only
# after Phase 7); a document named here must exist.
include(${CMAKE_CURRENT_LIST_DIR}/stage_headers.cmake)

string(REPLACE "|" ";" _docs "${DOCS}")
string(REPLACE "|" ";" _names "${HEADERS}")
or_stage_headers("${SOURCE_DIR}" "${STAGE}" "${_names}")

# Top-level repository entries a path may start at.
file(GLOB _top RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*")
set(_bad "")
set(_skipped 0)
set(_checked 0)

# Resolve one reference. <form> is include, prefix or plain.
function(_or_exists out form ref docdir)
    set(_ok FALSE)
    if(form STREQUAL "include")
        if(EXISTS "${STAGE}/include/${ref}")
            set(_ok TRUE)
        endif()
    elseif(form STREQUAL "prefix")
        string(REGEX REPLACE "^<prefix>/?" "" _rest "${ref}")
        if(_rest STREQUAL "" OR EXISTS "${STAGE}/${_rest}")
            set(_ok TRUE)
        elseif(NOT _rest MATCHES "^include(/|$)")
            # The staging holds the installed headers only; any other <prefix> path (displays/,
            # lib/, ...) is verified against the scratch install of SC-011 in Phase 7.
            set(_ok TRUE)
        endif()
    else()
        if(EXISTS "${SOURCE_DIR}/${ref}" OR EXISTS "${docdir}/${ref}")
            set(_ok TRUE)
        elseif(NOT ref MATCHES "/")
            # A bare file name: the staged headers, or a file of that name anywhere in the source tree.
            if(EXISTS "${STAGE}/include/${ref}")
                set(_ok TRUE)
            else()
                file(GLOB_RECURSE _hits "${SOURCE_DIR}/src/${ref}" "${SOURCE_DIR}/tests/${ref}" "${SOURCE_DIR}/examples/${ref}")
                if(_hits)
                    set(_ok TRUE)
                endif()
            endif()
        endif()
    endif()
    set(${out} ${_ok} PARENT_SCOPE)
endfunction()

# Classify a token. Sets _kind to skip, include, prefix, plain or none (not a path reference).
function(_or_classify out token)
    set(_t "${token}")
    if(_t MATCHES "[ \t*?()=|{}\\\\\"';,]" AND NOT _t MATCHES "^<prefix>")
        set(${out} none PARENT_SCOPE)
        return()
    endif()
    if(_t MATCHES "^(https?|ftp|mailto):" OR _t MATCHES "^#" OR _t MATCHES "::" OR _t MATCHES "^-")
        set(${out} none PARENT_SCOPE)
        return()
    endif()
    if(_t MATCHES "^[/$~]" OR _t MATCHES "\\$")
        set(${out} skip PARENT_SCOPE)
        return()
    endif()
    string(REGEX REPLACE ":[0-9]+(-[0-9]+)?$" "" _t "${_t}")
    string(REGEX REPLACE "[.,:]+$" "" _t "${_t}")
    set(_ext "\\.(c|cc|cpp|cxx|h|hh|hpp|y|l|md|cmake)$")
    if(_t MATCHES "^<prefix>")
        set(${out} prefix PARENT_SCOPE)
        return()
    endif()
    if(_t MATCHES "/")
        string(REGEX MATCH "^[^/]+" _first "${_t}")
        list(FIND _top "${_first}" _is_top)
        if(_t MATCHES "/$" OR _t MATCHES "\\.[A-Za-z0-9]+$" OR NOT _is_top EQUAL -1)
            set(${out} plain PARENT_SCOPE)
        else()
            set(${out} none PARENT_SCOPE)
        endif()
    elseif(_t MATCHES "${_ext}" OR _t STREQUAL "CMakeLists.txt")
        set(${out} plain PARENT_SCOPE)
    else()
        set(${out} none PARENT_SCOPE)
    endif()
endfunction()

foreach(_doc IN LISTS _docs)
    if(NOT EXISTS "${SOURCE_DIR}/${_doc}")
        message(FATAL_ERROR "PRECONDITION: document ${_doc} does not exist")
    endif()
    get_filename_component(_docdir "${SOURCE_DIR}/${_doc}" DIRECTORY)
    file(READ "${SOURCE_DIR}/${_doc}" _content)

    # Keep ';', '[' and ']' out of CMake list handling.
    string(REPLACE ";" "@@SEMI@@" _content "${_content}")
    string(REPLACE "[" "@@LB@@" _content "${_content}")
    string(REPLACE "]" "@@RB@@" _content "${_content}")
    string(REPLACE "\n" "@@NL@@" _flat "${_content}")

    # Split on fences: even parts are prose, odd parts are code.
    string(REPLACE "```" ";" _parts "${_flat}")
    set(_index 0)
    foreach(_part IN LISTS _parts)
        math(EXPR _odd "${_index} % 2")
        math(EXPR _index "${_index} + 1")
        string(REPLACE "@@NL@@" "\n" _text "${_part}")

        # --- #include lines, prose and code alike ---
        string(REGEX MATCHALL "#[ \t]*include[ \t]*[\"<][^\">\n]+[\">]" _incs "${_text}")
        foreach(_inc IN LISTS _incs)
            string(REGEX REPLACE "#[ \t]*include[ \t]*[\"<]([^\">]+)[\">]" "\\1" _ref "${_inc}")
            # System and standard headers are not ours to check.
            if(_inc MATCHES "<" AND NOT _ref MATCHES "/")
                continue()
            endif()
            if(_inc MATCHES "<[a-z_]+(/[a-z_.]+)?>" AND NOT _ref MATCHES "\\.h$")
                continue()
            endif()
            math(EXPR _checked "${_checked} + 1")
            _or_exists(_ok include "${_ref}" "${_docdir}")
            if(NOT _ok)
                list(APPEND _bad "${_doc}: #include \"${_ref}\" is not in the installed header set")
            endif()
        endforeach()

        if(_odd EQUAL 1)
            continue()
        endif()

        # --- backtick tokens in prose ---
        string(REGEX MATCHALL "`[^`\n]+`" _tokens "${_text}")
        foreach(_token IN LISTS _tokens)
            string(REGEX REPLACE "^`(.*)`$" "\\1" _ref "${_token}")
            string(REPLACE "@@SEMI@@" ";" _ref "${_ref}")
            if(_ref MATCHES "^#[ \t]*include")
                continue() # handled above
            endif()
            _or_classify(_kind "${_ref}")
            if(_kind STREQUAL "skip")
                math(EXPR _skipped "${_skipped} + 1")
            elseif(NOT _kind STREQUAL "none")
                string(REGEX REPLACE ":[0-9]+(-[0-9]+)?$" "" _clean "${_ref}")
                string(REGEX REPLACE "[.,:]+$" "" _clean "${_clean}")
                math(EXPR _checked "${_checked} + 1")
                _or_exists(_ok ${_kind} "${_clean}" "${_docdir}")
                if(NOT _ok)
                    list(APPEND _bad "${_doc}: `${_clean}` does not exist")
                endif()
            endif()
        endforeach()

        # --- Markdown link targets ---
        string(REGEX MATCHALL "@@RB@@\\([^)\n ]+\\)" _links "${_text}")
        foreach(_link IN LISTS _links)
            string(REGEX REPLACE "^@@RB@@\\((.*)\\)$" "\\1" _ref "${_link}")
            string(REGEX REPLACE "#.*$" "" _ref "${_ref}")
            if(_ref STREQUAL "" OR _ref MATCHES "^[a-z]+:" OR _ref MATCHES "^[/$]")
                continue()
            endif()
            math(EXPR _checked "${_checked} + 1")
            _or_exists(_ok plain "${_ref}" "${_docdir}")
            if(NOT _ok)
                list(APPEND _bad "${_doc}: link target ${_ref} does not exist")
            endif()
        endforeach()
    endforeach()
endforeach()

list(REMOVE_DUPLICATES _bad)
list(LENGTH _bad _n)
message("${_checked} path reference(s) checked in ${DOCS}; ${_skipped} install-layout token(s) left to the scratch-install check; ${_n} stale")
if(_n GREATER 0)
    string(REPLACE ";" "\n  " _report "${_bad}")
    string(REPLACE "@@SEMI@@" ";" _report "${_report}")
    message(FATAL_ERROR "DEFECT FR-036: stale path references:\n  ${_report}")
endif()
