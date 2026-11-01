# PluginAbi_InstallList (FR-026, FR-048, SC-011). Run as
#   cmake -DHEADERS="<a.h>|<b.h>|..." -DSOURCE_DIR=<repo> -DARTIFACTS=<INSTALL_ARTIFACTS.md>
#         -DGUIDE=<DISPLAY_PLUGIN_GUIDE.md> -P this
#
# The installed header list (CMake/openRenderPublicHeaders.cmake, by file name) is the one source of truth:
#   * no install(FILES ...) rule of the tree puts any other header into the include directory: a header named in
#     such a rule (rslo.h, file_base.h, ...) that is not in the list is a header installed behind the list's back;
#   * INSTALL_ARTIFACTS.md names exactly that set, in its "Header Files" section and in the include/ part of its
#     directory tree: every header it names there is in the list and every header of the list is named;
#   * DISPLAY_PLUGIN_GUIDE.md names only headers of the list in its #include lines (by the file name an
#     installed header has, with no directory in front of it);
#   * file_base.h is in neither document: it is an in-tree helper of the in-tree file drivers (FR-026).
# Red on the starting tree: src/libshader/runtime installs rslo.h and src/display/file installs file_base.h, which the
# list does not name; INSTALL_ARTIFACTS.md names file_base.h and lacks orplugin.h and orplugin.hpp; the guide
# includes ri/dsply.h and file/file_base.h.
cmake_policy(SET CMP0057 NEW) # if(... IN_LIST ...) in script mode, whatever the CMake version

string(REPLACE "|" ";" _list "${HEADERS}")
set(_failures "")

# --- the install rules ---
# not "${SOURCE_DIR}/CMakeLists.txt" in GLOB_RECURSE: that recurses into the build directories, where the nested
# configure of PluginAbi_InstallLayouts creates and deletes CMakeScratch files while this test runs
file(GLOB _cmake_files "${SOURCE_DIR}/CMakeLists.txt")
file(GLOB_RECURSE _cmake_tree "${SOURCE_DIR}/src/*CMakeLists.txt" "${SOURCE_DIR}/CMake/*.cmake")
list(APPEND _cmake_files ${_cmake_tree})
list(REMOVE_DUPLICATES _cmake_files)
foreach(_file IN LISTS _cmake_files)
    file(READ "${_file}" _text)
    string(REGEX REPLACE "#[^\n]*" "" _text "${_text}")
    string(REGEX MATCHALL "install\\(FILES[^)]*\\)" _rules "${_text}")
    foreach(_rule IN LISTS _rules)
        if(_rule MATCHES "INCLUDEDIR|OPENRENDER_INCLUDE")
            string(REGEX MATCHALL "[A-Za-z0-9_]+\\.h(pp)?" _files_named "${_rule}")
            foreach(_h IN LISTS _files_named)
                if(NOT _h IN_LIST _list)
                    file(RELATIVE_PATH _rel "${SOURCE_DIR}" "${_file}")
                    list(APPEND _failures "${_rel}: install(FILES ${_h}) puts a header into the include directory that is not in the installed header list")
                endif()
            endforeach()
        endif()
    endforeach()
endforeach()

# --- INSTALL_ARTIFACTS.md ---
file(READ "${ARTIFACTS}" _artifacts)
set(_named "")
# 1. the "Header Files" section: from its heading to the next horizontal rule
string(FIND "${_artifacts}" "## Header Files" _start)
if(_start EQUAL -1)
    list(APPEND _failures "INSTALL_ARTIFACTS.md has no \"## Header Files\" section")
else()
    string(SUBSTRING "${_artifacts}" ${_start} -1 _section)
    string(FIND "${_section}" "\n---" _end)
    if(NOT _end EQUAL -1)
        string(SUBSTRING "${_section}" 0 ${_end} _section)
    endif()
    string(REGEX MATCHALL "`[A-Za-z0-9_]+\\.h(pp)?`" _in_section "${_section}")
    foreach(_m IN LISTS _in_section)
        string(REPLACE "`" "" _m "${_m}")
        list(APPEND _named "${_m}")
    endforeach()
endif()
set(_in_section_names ${_named})
list(REMOVE_DUPLICATES _in_section_names)
# 2. the include/ part of the directory tree: entries of the form "│   ├── name.h" under an include/ line
string(REGEX MATCHALL "[│ ]+[├└]── [A-Za-z0-9_]+\\.h(pp)?" _tree "${_artifacts}")
set(_in_tree "")
foreach(_m IN LISTS _tree)
    string(REGEX REPLACE ".*── " "" _m "${_m}")
    list(APPEND _in_tree "${_m}")
endforeach()
list(REMOVE_DUPLICATES _in_tree)

foreach(_pair "section;${_in_section_names}" "tree;${_in_tree}")
    list(GET _pair 0 _where)
    list(REMOVE_AT _pair 0)
    foreach(_h IN LISTS _pair)
        if(NOT _h IN_LIST _list)
            list(APPEND _failures "INSTALL_ARTIFACTS.md (${_where}) names ${_h}, which is not in the installed header list")
        endif()
    endforeach()
    foreach(_h IN LISTS _list)
        if(NOT _h IN_LIST _pair)
            list(APPEND _failures "INSTALL_ARTIFACTS.md (${_where}) does not name ${_h}, which is in the installed header list")
        endif()
    endforeach()
endforeach()
if(_artifacts MATCHES "file_base\\.h")
    list(APPEND _failures "INSTALL_ARTIFACTS.md names file_base.h")
endif()

# --- DISPLAY_PLUGIN_GUIDE.md ---
file(READ "${GUIDE}" _guide)
if(_guide MATCHES "file_base\\.h")
    list(APPEND _failures "DISPLAY_PLUGIN_GUIDE.md names file_base.h")
endif()
string(REGEX MATCHALL "#[ \t]*include[ \t]*[\"<][^\">]+\\.h(pp)?[\">]" _guide_includes "${_guide}")
foreach(_inc IN LISTS _guide_includes)
    string(REGEX REPLACE "#[ \t]*include[ \t]*[\"<]([^\">]+)[\">]" "\\1" _target "${_inc}")
    if(NOT _target IN_LIST _list)
        list(APPEND _failures "DISPLAY_PLUGIN_GUIDE.md: #include of ${_target} names no header of the installed list (an installed header is included by its file name alone)")
    endif()
endforeach()

if(_failures)
    string(REPLACE ";" "\n  " _report "${_failures}")
    message(FATAL_ERROR "DEFECT FR-026/FR-048: the install list and the documents disagree:\n  ${_report}")
endif()
