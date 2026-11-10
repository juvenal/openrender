# Hcshader_NoReferences (spec 030, SC-007, FR-025, FR-028): no tracked file under src/, CMake/, tests/, doc/, docs/ or
# examples/ names the retired built-in lights: `hcshader`, `CSphereLight` or `CQuadLight`. The search is CASE-SENSITIVE
# (the Shading comment "hcShaders" in src/libshader/shading/shader.h is not a match).
#
# -DSOURCE_DIR=<repository root> [-DEXEMPT_FILES=a|b|c] [-DREGISTRATION_FILES=a|b]
#
# Exemptions (tasks.md T023 (3), maintainer answer Q-A as amended 2026-11-09). Whole files and trees whose mention is
# history, not a reference:
#   tests/architecture/unit/test_archcheck_parsers.cpp   in-memory prefix-matching data of an approved test (Q8)
#   tests/visual/CMakeLists.txt                          a history comment in an approved file
#   examples/rib/spherelight.rib, examples/rib/quadlight.rib   history comments
# (not under the scanned trees, so not scanned at all: shaders/*.sl, .plans/, CLAUDE.md, DEVNOTES.md, DEVNOTES_DETAILS/,
#  NEWS.md, specs/.)
# The script's own exclusions: this script (its pattern is the searched string), the Hcshader_* test source
# tests/unit/registry/test_hcshader_missing_shader.cpp, and the lines of tests/architecture/CMakeLists.txt and
# tests/unit/registry/CMakeLists.txt that register the Hcshader_* tests (they necessarily name the script and the source).
#
# It never passes vacuously: no tracked file list, or an empty scan, is an error.
if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "check_hcshader_references: -DSOURCE_DIR= is required")
endif()

# Reads a file into a list with one element per line. The characters CMake treats specially in lists (';', '[' and
# ']') are replaced first, so a line is never split or merged; the replacement changes no searched token.
function(read_lines path out)
    file(READ "${path}" _text)
    string(REPLACE ";" "<semi>" _text "${_text}")
    string(REPLACE "[" "<lb>" _text "${_text}")
    string(REPLACE "]" "<rb>" _text "${_text}")
    string(REPLACE "\n" ";" _text "${_text}")
    set(${out} "${_text}" PARENT_SCOPE)
endfunction()

execute_process(COMMAND git -C "${SOURCE_DIR}" ls-files -- src CMake tests doc docs examples
    OUTPUT_VARIABLE _files RESULT_VARIABLE _rc ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0 OR "${_files}" STREQUAL "")
    message(FATAL_ERROR "check_hcshader_references: cannot list the tracked files (git ls-files: ${_rc} ${_err})")
endif()
string(REPLACE "\n" ";" _files "${_files}")

# The exemption lists are arguments of the registration (tests/architecture/CMakeLists.txt), '|'-separated, so the list
# shown to the maintainer at T023 is the one that runs. The defaults below are the same lists.
set(_exempt_files
    tests/architecture/unit/test_archcheck_parsers.cpp
    tests/visual/CMakeLists.txt
    examples/rib/spherelight.rib
    examples/rib/quadlight.rib
    tests/architecture/check_hcshader_references.cmake
    tests/unit/registry/test_hcshader_missing_shader.cpp)
if(DEFINED EXEMPT_FILES)
    string(REPLACE "|" ";" _exempt_files "${EXEMPT_FILES}")
endif()
set(_registration_files tests/architecture/CMakeLists.txt tests/unit/registry/CMakeLists.txt)
if(DEFINED REGISTRATION_FILES)
    string(REPLACE "|" ";" _registration_files "${REGISTRATION_FILES}")
endif()
set(_registration_tokens "check_hcshader_references|test_hcshader_missing_shader|Hcshader_")
set(_text_ext "\\.(c|cc|cpp|h|hpp|cmake|txt|md|y|l|sl|slh|rib|py|lua|sh|json|html|in|yml|yaml|m|mm|swift|cfg|inc)$")

set(_scanned 0)
set(_hits "")
foreach(_f IN LISTS _files)
    if(_f STREQUAL "" OR NOT _f MATCHES "${_text_ext}")
        continue()
    endif()
    if(_f IN_LIST _exempt_files)
        continue()
    endif()
    if(NOT EXISTS "${SOURCE_DIR}/${_f}")
        continue()
    endif()
    math(EXPR _scanned "${_scanned} + 1")
    read_lines("${SOURCE_DIR}/${_f}" _lines)
    foreach(_line IN LISTS _lines)
        if(NOT _line MATCHES "hcshader|CSphereLight|CQuadLight")
            continue()
        endif()
        if(_f IN_LIST _registration_files AND _line MATCHES "${_registration_tokens}")
            continue()
        endif()
        string(APPEND _hits "  ${_f}: ${_line}\n")
    endforeach()
endforeach()

if(_scanned LESS 100)
    message(FATAL_ERROR "check_hcshader_references: only ${_scanned} files scanned; the file list is wrong")
endif()
if(NOT "${_hits}" STREQUAL "")
    message(FATAL_ERROR "Hcshader_NoReferences: the retired built-in lights are still named:\n${_hits}")
endif()
message(STATUS "Hcshader_NoReferences: ${_scanned} tracked files scanned, no reference")
