# PluginAbi_VersionHistory (spec 037, FR-001, FR-047, constitution Principle VIII rule 4). Run as
#   cmake -DHEADERS="<header>:<PREFIX>[,<PREFIX>]|..." -DINCLUDE_DIRS="<dir>|<dir>" -P this
#
# Every family header declares its interface version in macros and documents it in a version-history
# table in its header comment. HEADERS names, per header, the macro prefixes it must define:
#   shadeop.h:SHADEOP|dlo.h:DLO,OR_DYNAMICLOAD|implicit.h:IMPLICIT       (dsply.h:OR_DISPLAY joined in T085)
#
# For each prefix P of a header:
#   * `#define P_ABI_VERSION OR_ABI_VERSION(M, m, p)` exists;
#   * when P_ABI_VERSION_MAJOR / _MINOR / _PATCH are defined they equal M, m, p;
#   * the header comment has at least one history row for P;
#   * the newest history row equals M.m.p;
#   * no MINOR or PATCH, in a macro or a row, is above 255: OR_ABI_VERSION keeps 8 bits each and a change
#     that needs more is a MAJOR change (FR-047).
#
# A history row is a table line  | M.m.p | text |  in a comment. A row belongs to the header's first
# prefix, unless it is tagged with the interface in parentheses  | M.m.p (dynamicload) | text | :
# the tag is the prefix in lower case without a leading OR_. Newest = the highest M.m.p of the rows.
#
# Red on the starting tree: no header declares a version or a history (T041 to T043 add them).
string(REPLACE "|" ";" _entries "${HEADERS}")
string(REPLACE "|" ";" _dirs "${INCLUDE_DIRS}")
set(_failures "")

foreach(_entry IN LISTS _entries)
    string(REPLACE ":" ";" _parts "${_entry}")
    list(GET _parts 0 _header)
    list(GET _parts 1 _prefix_list)
    string(REPLACE "," ";" _prefixes "${_prefix_list}")

    set(_path "")
    foreach(_dir IN LISTS _dirs)
        if(EXISTS "${_dir}/${_header}")
            set(_path "${_dir}/${_header}")
            break()
        endif()
    endforeach()
    if(_path STREQUAL "")
        list(APPEND _failures "${_header}: not found in ${INCLUDE_DIRS}")
        continue()
    endif()
    file(READ "${_path}" _text)

    # All history rows: "| M.m.p [(tag)] |".
    string(REGEX MATCHALL "\\|[ \t]*[0-9]+\\.[0-9]+\\.[0-9]+[ \t]*(\\([a-z]+\\))?[ \t]*\\|" _rows "${_text}")
    list(GET _prefixes 0 _first_prefix)

    foreach(_prefix IN LISTS _prefixes)
        set(_tag "${_prefix}")
        string(REGEX REPLACE "^OR_" "" _tag "${_tag}")
        string(TOLOWER "${_tag}" _tag)

        # the macro
        if(NOT _text MATCHES "#define[ \t]+${_prefix}_ABI_VERSION[ \t]+OR_ABI_VERSION\\(([0-9]+),[ \t]*([0-9]+),[ \t]*([0-9]+)\\)")
            list(APPEND _failures "${_header}: no `#define ${_prefix}_ABI_VERSION OR_ABI_VERSION(major, minor, patch)`")
            continue()
        endif()
        set(_major ${CMAKE_MATCH_1})
        set(_minor ${CMAKE_MATCH_2})
        set(_patch ${CMAKE_MATCH_3})
        if(_minor GREATER 255 OR _patch GREATER 255)
            list(APPEND _failures "${_header}: ${_prefix}_ABI_VERSION ${_major}.${_minor}.${_patch} has a MINOR or PATCH above 255 (not representable)")
        endif()
        foreach(_field MAJOR MINOR PATCH)
            string(TOLOWER "${_field}" _lower)
            if(_text MATCHES "#define[ \t]+${_prefix}_ABI_VERSION_${_field}[ \t]+([0-9]+)")
                if(NOT "${CMAKE_MATCH_1}" STREQUAL "${_${_lower}}")
                    list(APPEND _failures "${_header}: ${_prefix}_ABI_VERSION_${_field} is ${CMAKE_MATCH_1}, the OR_ABI_VERSION macro says ${_${_lower}}")
                endif()
            endif()
        endforeach()

        # the rows of this interface
        set(_newest -1)
        set(_newest_text "")
        set(_count 0)
        foreach(_row IN LISTS _rows)
            string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)[ \t]*(\\(([a-z]+)\\))?" _ignored "${_row}")
            set(_rm ${CMAKE_MATCH_1})
            set(_rn ${CMAKE_MATCH_2})
            set(_rp ${CMAKE_MATCH_3})
            set(_rt "${CMAKE_MATCH_5}")
            if(_rt STREQUAL "")
                if(NOT _prefix STREQUAL _first_prefix)
                    continue()
                endif()
            elseif(NOT _rt STREQUAL _tag)
                continue()
            endif()
            math(EXPR _count "${_count} + 1")
            if(_rn GREATER 255 OR _rp GREATER 255)
                list(APPEND _failures "${_header}: history row ${_rm}.${_rn}.${_rp} (${_prefix}) has a MINOR or PATCH above 255")
                continue()
            endif()
            math(EXPR _value "(${_rm} << 16) | (${_rn} << 8) | ${_rp}")
            if(_value GREATER _newest)
                set(_newest ${_value})
                set(_newest_text "${_rm}.${_rn}.${_rp}")
            endif()
        endforeach()
        if(_count EQUAL 0)
            list(APPEND _failures "${_header}: no version-history row for ${_prefix} (a table line `| M.m.p | text |` in the header comment)")
        elseif(NOT "${_newest_text}" STREQUAL "${_major}.${_minor}.${_patch}")
            list(APPEND _failures "${_header}: the newest history row for ${_prefix} is ${_newest_text}, the macro says ${_major}.${_minor}.${_patch}")
        endif()
    endforeach()
endforeach()

if(_failures)
    string(REPLACE ";" "\n  " _report "${_failures}")
    message(FATAL_ERROR "DEFECT (FR-001, FR-047): the version declarations and histories are incomplete:\n  ${_report}")
endif()
