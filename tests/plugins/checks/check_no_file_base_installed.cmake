# PluginAbi_NoFileBaseInstalled (FR-026). Run as
#   cmake -DSOURCE_DIR=<repo> -DHEADERS="<a.h>|..." -P this
#
# file_base.h is an in-tree helper of the in-tree file drivers and is no longer installed: no install() command
# in a CMakeLists.txt or .cmake file of the tree names it, and it is not in the public header list.
# Red on the starting tree: src/display/file/CMakeLists.txt still has install(FILES file_base.h ...).
cmake_policy(SET CMP0057 NEW) # if(... IN_LIST ...) in script mode, whatever the CMake version

string(REPLACE "|" ";" _list "${HEADERS}")
set(_failures "")
if("file_base.h" IN_LIST _list)
    list(APPEND _failures "file_base.h is in the public header list")
endif()

file(GLOB_RECURSE _files "${SOURCE_DIR}/CMakeLists.txt" "${SOURCE_DIR}/CMake/*.cmake" "${SOURCE_DIR}/CMake/*.in"
     "${SOURCE_DIR}/src/*CMakeLists.txt")
list(REMOVE_DUPLICATES _files)
foreach(_file IN LISTS _files)
    file(READ "${_file}" _text)
    # drop comments, then look at the install( ... ) commands
    string(REGEX REPLACE "#[^\n]*" "" _text "${_text}")
    string(REGEX MATCHALL "install\\([^)]*\\)" _installs "${_text}")
    foreach(_cmd IN LISTS _installs)
        if(_cmd MATCHES "file_base\\.h")
            file(RELATIVE_PATH _rel "${SOURCE_DIR}" "${_file}")
            list(APPEND _failures "${_rel}: install() names file_base.h")
        endif()
    endforeach()
endforeach()

if(_failures)
    string(REPLACE ";" "\n  " _report "${_failures}")
    message(FATAL_ERROR "DEFECT FR-026: file_base.h is still installed:\n  ${_report}")
endif()
