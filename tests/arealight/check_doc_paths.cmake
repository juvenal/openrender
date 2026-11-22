# Spec 057 (area-light geometry), AreaLight_DocPaths (tasks.md T137; FR-033, US11): the maintained pages tell the truth about area lights.
# A cmake -P script (no git call, no build tree): cmake -DROOT=<source root> -P check_doc_paths.cmake. Prints one FAIL line per item and
# fails at the end if any item failed; with every document edited it prints "AreaLight_DocPaths: OK".
#
# The maintained pages: DEVNOTES.md, DEVNOTES_DETAILS/*.md, docs/site/content/**/*.md, man/*, docs/references/*.md.
#
# Items:
#   D1  RISPEC_GAPS.md no longer cites rendererContext.cpp:2045 (the stale line of RiAreaLightSourceV)
#   D2  RISPEC_GAPS.md no longer lists "Area Light Sources" as [~] (partial)
#   D3  no maintained page calls RiAreaLightSourceV identical to RiLightSourceV (or "RiLightSourceV plus a handle")
#   D4  the site's RISpec table does not call Area Light Sources **Partial**
#   D5  a maintained page that puts quadlight or spherelight next to "area light" also carries the deprecation pointer ("deprecat") in
#       the same window, AND at least one manual page under docs/site/content/manual/ documents AreaLightSource and the deprecation of
#       quadlight/spherelight together (on the starting tree no page says either: the second half is what is red there)
#   D6  RENDER_ARCHITECTURE_REFERENCES.md section 13 has a row for spec 056 and one for spec 057

if(NOT DEFINED ROOT)
    message(FATAL_ERROR "check_doc_paths.cmake: pass -DROOT=<source root>")
endif()

set(_failed 0)
macro(doc_fail _msg)
    message("FAIL: ${_msg}")
    set(_failed 1)
endmacro()

# The maintained pages
file(GLOB _devnotes_details "${ROOT}/DEVNOTES_DETAILS/*.md")
file(GLOB_RECURSE _site "${ROOT}/docs/site/content/*.md")
file(GLOB _man "${ROOT}/man/*")
file(GLOB _refs "${ROOT}/docs/references/*.md")
set(_pages "${ROOT}/DEVNOTES.md" ${_devnotes_details} ${_site} ${_man} ${_refs})

# page text with newlines folded to spaces, cached by path
function(page_text _path _out)
    file(READ "${_path}" _t)
    string(REGEX REPLACE "[\r\n]+" " " _t "${_t}")
    set(${_out} "${_t}" PARENT_SCOPE)
endfunction()

set(_gaps "${ROOT}/DEVNOTES_DETAILS/RISPEC_GAPS.md")
if(NOT EXISTS "${_gaps}")
    doc_fail("D1/D2: ${_gaps} is missing")
else()
    file(READ "${_gaps}" _gaps_raw)
    string(FIND "${_gaps_raw}" "rendererContext.cpp:2045" _at)
    if(NOT _at EQUAL -1)
        doc_fail("D1: RISPEC_GAPS.md still cites rendererContext.cpp:2045")
    endif()
    if(_gaps_raw MATCHES "- \\[~\\] \\*\\*Area Light Sources")
        doc_fail("D2: RISPEC_GAPS.md still lists Area Light Sources as [~] (partial)")
    endif()
endif()

set(_manual_pointer 0)
foreach(_page IN LISTS _pages)
    if(NOT EXISTS "${_page}")
        continue()
    endif()
    file(RELATIVE_PATH _rel "${ROOT}" "${_page}")
    page_text("${_page}" _t)
    # D3
    if(_t MATCHES "RiAreaLightSourceV.*identical to *.?RiLightSourceV" OR _t MATCHES "RiAreaLightSourceV. is .RiLightSourceV. plus a handle")
        doc_fail("D3: ${_rel} calls RiAreaLightSourceV identical to RiLightSourceV")
    endif()
    # D4
    if(_rel MATCHES "docs/site/content/references/renderman-rispec.md" AND _t MATCHES "Area Light Sources \\| \\*\\*Partial\\*\\*")
        doc_fail("D4: ${_rel} lists Area Light Sources as Partial")
    endif()
    # D5 (first half): quadlight/spherelight beside "area light" without a deprecation pointer in the window
    string(TOLOWER "${_t}" _lower)
    string(REPLACE ";" " " _lower "${_lower}")
    set(_rest "${_lower}")
    set(_seen_mention 0)
    while(_rest MATCHES "(quadlight|spherelight)")
        set(_seen_mention 1)
        string(REGEX MATCH "(quadlight|spherelight)" _m "${_rest}")
        string(FIND "${_rest}" "${_m}" _at)
        math(EXPR _from "${_at} - 250")
        if(_from LESS 0)
            set(_from 0)
        endif()
        string(SUBSTRING "${_rest}" ${_from} 500 _window)
        if(_window MATCHES "area.?light" AND NOT _window MATCHES "deprecat")
            doc_fail("D5: ${_rel} puts quadlight/spherelight next to \"area light\" without the deprecation pointer")
            break()
        endif()
        # continue after this mention
        math(EXPR _next "${_at} + 8")
        string(LENGTH "${_rest}" _len)
        if(_next GREATER_EQUAL _len)
            break()
        endif()
        string(SUBSTRING "${_rest}" ${_next} -1 _rest)
    endwhile()
    set(_hits "${_seen_mention}")
    if(_hits)
        # D5 (second half): a manual page that documents AreaLightSource together with the deprecation
        if(_rel MATCHES "^docs/site/content/manual/" AND _t MATCHES "AreaLightSource" AND _t MATCHES "(quadlight|spherelight).*deprecat|deprecat.*(quadlight|spherelight)")
            set(_manual_pointer 1)
        endif()
    endif()
endforeach()
if(NOT _manual_pointer)
    doc_fail("D5: no manual page under docs/site/content/manual/ documents AreaLightSource together with the deprecation of quadlight and spherelight")
endif()

# D6
set(_refs_page "${ROOT}/DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md")
if(NOT EXISTS "${_refs_page}")
    doc_fail("D6: ${_refs_page} is missing")
else()
    file(READ "${_refs_page}" _r)
    string(FIND "${_r}" "## 13. Spec" _s13)
    if(_s13 EQUAL -1)
        doc_fail("D6: RENDER_ARCHITECTURE_REFERENCES.md has no section 13")
    else()
        string(SUBSTRING "${_r}" ${_s13} -1 _sec13)
        foreach(_n 056 057)
            if(NOT _sec13 MATCHES "\n\\| ${_n} ")
                doc_fail("D6: RENDER_ARCHITECTURE_REFERENCES.md section 13 has no row for spec ${_n}")
            endif()
        endforeach()
    endif()
endif()

if(_failed)
    message(FATAL_ERROR "AreaLight_DocPaths: FAILED")
endif()
message("AreaLight_DocPaths: OK")
