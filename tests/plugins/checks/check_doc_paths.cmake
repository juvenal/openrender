# PluginAbi_DocPaths (FR-036, FR-047, SC-020). Run as
#   cmake -DDOCS="<doc>|<doc>|..." -DHEADERS="<a.h>|<b.h>|..." -DSOURCE_DIR=<repo> -DSTAGE=<scratch dir>
#         -DVERSIONS="<key>=<header>:<PREFIX>|..." -DREQUIRE="<doc>=<key>[,<key>]|..." -P this
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
# A document named here must exist (the guides, PLUGIN_ABI.md and the site page are all required: a missing one is
# a failure, not a skip).
#
# Version numbers (FR-047, SC-020). The version-history table in each family's header comment is the single source
# of that family's versions (PluginAbi_VersionHistory checks the header itself); the documents only quote it. VERSIONS
# maps an interface key to the header and macro prefix that define it:
#   display=dsply.h:OR_DISPLAY|shadeop=shadeop.h:SHADEOP|dlo=dlo.h:DLO|dynamicload=dlo.h:OR_DYNAMICLOAD|implicit=implicit.h:IMPLICIT
# A document *states a version for an interface* in one of two forms, and a statement that is not equal to the header's is a
# failure:
#   * prose: the interface word, optionally followed by qualifier words (interface, ABI, API, version, table, module,
#     driver, drivers, operand, operands, plug-in, plug-ins, procedural, surface, DSO), then M.m.p, markup (backticks,
#     bold) ignored, upper or lower case: "display 2.0.0", "the shadeop interface version 1.1.0". It must equal the
#     header's *current* version (the OR_ABI_VERSION macro). A historical version is not written in this form: say
#     "MAJOR 1" or quote the history table.
#   * a tagged history row: "| M.m.p (key) | ... |", the form the headers use for a second interface in one file. It must be
#     a row of that interface's history table in the header (any row, not only the newest).
# REQUIRE names, per document, the interface keys the document must state a (prose) version for at least once.
cmake_policy(SET CMP0057 NEW) # if(... IN_LIST ...) in script mode, whatever the CMake version
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
        list(APPEND _bad "${_doc}: the document does not exist")
        continue()
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

# ---------------------------------------------------------------------------------------------------------------------
# Version numbers
# ---------------------------------------------------------------------------------------------------------------------
set(_vbad "")
set(_vchecked 0)
if(DEFINED VERSIONS AND NOT VERSIONS STREQUAL "")
    string(REPLACE "|" ";" _ventries "${VERSIONS}")
    set(_keys "")
    foreach(_entry IN LISTS _ventries)
        string(REGEX MATCH "^([a-z]+)=([A-Za-z0-9_.]+):([A-Z_]+)$" _m "${_entry}")
        set(_key "${CMAKE_MATCH_1}")
        set(_header "${CMAKE_MATCH_2}")
        set(_prefix "${CMAKE_MATCH_3}")
        list(APPEND _keys "${_key}")
        if(NOT EXISTS "${STAGE}/include/${_header}")
            list(APPEND _vbad "VERSIONS: ${_header} is not in the staged header set")
            continue()
        endif()
        file(READ "${STAGE}/include/${_header}" _htext)
        if(NOT _htext MATCHES "#define[ \t]+${_prefix}_ABI_VERSION[ \t]+OR_ABI_VERSION\\(([0-9]+),[ \t]*([0-9]+),[ \t]*([0-9]+)\\)")
            list(APPEND _vbad "VERSIONS: ${_header} defines no ${_prefix}_ABI_VERSION")
            continue()
        endif()
        set(_current_${_key} "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
        # the history rows of this interface: tagged with the key, or untagged when the key is the header's first prefix
        string(REGEX MATCHALL "\\|[ \t]*[0-9]+\\.[0-9]+\\.[0-9]+[ \t]*(\\([a-z]+\\))?[ \t]*\\|" _hrows "${_htext}")
        string(REGEX MATCHALL "#define[ \t]+[A-Z_]+_ABI_VERSION[ \t]+OR_ABI_VERSION" _hdefs "${_htext}")
        string(REGEX MATCH "#define[ \t]+([A-Z_]+)_ABI_VERSION[ \t]+OR_ABI_VERSION" _firstdef "${_htext}")
        set(_first_prefix "${CMAKE_MATCH_1}")
        set(_rows_${_key} "")
        foreach(_row IN LISTS _hrows)
            string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)[ \t]*(\\(([a-z]+)\\))?" _ignored "${_row}")
            set(_rv "${CMAKE_MATCH_1}")
            set(_rt "${CMAKE_MATCH_3}")
            if((_rt STREQUAL "" AND _prefix STREQUAL _first_prefix) OR _rt STREQUAL _key)
                list(APPEND _rows_${_key} "${_rv}")
            endif()
        endforeach()
    endforeach()

    string(REPLACE "|" ";" _reqs "${REQUIRE}")
    foreach(_doc IN LISTS _docs)
        if(NOT EXISTS "${SOURCE_DIR}/${_doc}")
            foreach(_req IN LISTS _reqs)
                string(REGEX MATCH "^([^=]+)=" _ignored "${_req}")
                if(CMAKE_MATCH_1 STREQUAL _doc)
                    list(APPEND _vbad "${_doc}: the document does not exist, so it states none of the versions it must")
                endif()
            endforeach()
            continue()
        endif()
        file(READ "${SOURCE_DIR}/${_doc}" _raw)
        set(_stated "")
        string(REGEX REPLACE "[`*]" "" _plain "${_raw}")
        string(TOLOWER "${_plain}" _lower)
        string(REPLACE ";" " " _lower "${_lower}")
        string(REGEX MATCHALL "(display|shadeop|dynamicload|dlo|implicit)( (interface|abi|api|version|table|module|driver|drivers|operand|operands|plug-in|plug-ins|plugin|procedural|surface|dso))* v?[0-9]+\\.[0-9]+\\.[0-9]+" _stmts "${_lower}")
        foreach(_stmt IN LISTS _stmts)
            string(REGEX MATCH "^[a-z]+" _skey "${_stmt}")
            string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+$" _sver "${_stmt}")
            math(EXPR _vchecked "${_vchecked} + 1")
            list(APPEND _stated "${_skey}")
            if(NOT DEFINED _current_${_skey})
                list(APPEND _vbad "${_doc}: states a version for '${_skey}' ('${_stmt}'), which VERSIONS does not define")
            elseif(NOT _sver STREQUAL _current_${_skey})
                list(APPEND _vbad "${_doc}: '${_stmt}' but the header's ${_skey} version is ${_current_${_skey}}")
            endif()
        endforeach()
        # tagged rows
        string(REGEX MATCHALL "\\|[ \t]*[0-9]+\\.[0-9]+\\.[0-9]+[ \t]*\\([a-z]+\\)[ \t]*\\|" _trows "${_raw}")
        foreach(_trow IN LISTS _trows)
            string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)[ \t]*\\(([a-z]+)\\)" _ignored "${_trow}")
            set(_tv "${CMAKE_MATCH_1}")
            set(_tk "${CMAKE_MATCH_2}")
            math(EXPR _vchecked "${_vchecked} + 1")
            if(NOT DEFINED _current_${_tk})
                list(APPEND _vbad "${_doc}: history row '${_tv} (${_tk})' names an interface VERSIONS does not define")
            elseif(NOT _tv IN_LIST _rows_${_tk})
                list(APPEND _vbad "${_doc}: history row '${_tv} (${_tk})' is not in the ${_tk} history table of the header")
            endif()
        endforeach()
        # required statements
        foreach(_req IN LISTS _reqs)
            string(REGEX MATCH "^([^=]+)=(.*)$" _ignored "${_req}")
            if(CMAKE_MATCH_1 STREQUAL _doc)
                string(REPLACE "," ";" _need "${CMAKE_MATCH_2}")
                foreach(_k IN LISTS _need)
                    if(NOT _k IN_LIST _stated)
                        list(APPEND _vbad "${_doc}: states no ${_k} version (expected '${_k} ${_current_${_k}}')")
                    endif()
                endforeach()
            endif()
        endforeach()
    endforeach()
endif()
list(REMOVE_DUPLICATES _vbad)
list(LENGTH _vbad _vn)
message("${_vchecked} version statement(s) compared with the headers; ${_vn} wrong or missing")

list(REMOVE_DUPLICATES _bad)
list(LENGTH _bad _n)
message("${_checked} path reference(s) checked in ${DOCS}; ${_skipped} install-layout token(s) left to the scratch-install check; ${_n} stale")
if(_n GREATER 0 OR _vn GREATER 0)
    set(_report "")
    if(_n GREATER 0)
        string(REPLACE ";" "\n  " _paths "${_bad}")
        string(REPLACE "@@SEMI@@" ";" _paths "${_paths}")
        string(APPEND _report "DEFECT FR-036: stale path references:\n  ${_paths}\n")
    endif()
    if(_vn GREATER 0)
        string(REPLACE ";" "\n  " _versions "${_vbad}")
        string(APPEND _report "DEFECT FR-047/SC-020: version numbers the documents state differ from the headers:\n  ${_versions}\n")
    endif()
    message(FATAL_ERROR "${_report}")
endif()
