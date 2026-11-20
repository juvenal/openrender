# Link_RenameAliases (spec 033, FR-031, L-1, contracts/checks-and-tests.md "Added for the library rename"): after the build, the
# shading engine library is libRSLRuntime and the inspection library libRSLInfo, and every old name still resolves to the same file.
#
#   cmake -DRUNTIME=<$<TARGET_FILE:libRSLRuntime>> -DRUNTIME_ALIAS=<$<TARGET_FILE:libshader_shading>>
#         -DINFO=<$<TARGET_FILE:libRSLInfo>> -DINFO_ALIAS=<$<TARGET_FILE:libshader_runtime>>
#         -DINFO_STATIC=<$<TARGET_FILE:libRSLInfo_static>> -DINFO_STATIC_ALIAS=<$<TARGET_FILE:libshader_runtime_static>>
#         -DRSLO_ALIAS=<$<TARGET_FILE:rslo>> -DRSLO_STATIC_ALIAS=<$<TARGET_FILE:rslo_static>>
#         -DCOMPILER=<$<TARGET_FILE:libshader_compiler>> -P check_rename_aliases.cmake
#
# 1. the real files are named libRSLRuntime.* and libRSLInfo.* (libRSLInfo also as the archive libRSLInfo.a);
# 2. every alias resolves to the same file as its real target;
# 3. the compatibility files next to them (libshader_shading.{dylib,so}, libshader_runtime.{dylib,so}, libshader_runtime.a) exist and
#    are symlinks that resolve to them;
# 4. libshader_compiler still resolves to a file named libshader_compiler.* (it is renamed by spec 043, not here);
# 5. no other library file in the build tree's library directories is named libshader_* as a real file (libshader_jitmeta is gone).
#
# Red on the starting tree: libRSLRuntime and libRSLInfo are not targets, so the generator expressions expand to nothing.
foreach(_v RUNTIME RUNTIME_ALIAS INFO INFO_ALIAS INFO_STATIC INFO_STATIC_ALIAS RSLO_ALIAS RSLO_STATIC_ALIAS COMPILER)
    if("${${_v}}" STREQUAL "")
        list(APPEND _missing ${_v})
    endif()
endforeach()
if(_missing)
    message(FATAL_ERROR "Link_RenameAliases: no such target (the renames are not done): ${_missing}")
endif()

set(_problems "")
function(real path out)
    get_filename_component(_r "${path}" REALPATH)
    set(${out} "${_r}" PARENT_SCOPE)
endfunction()
function(base_has path prefix)
    get_filename_component(_n "${path}" NAME)
    if(NOT _n MATCHES "^${prefix}")
        set(_problems "${_problems}\n  ${path}: the file is not named ${prefix}*" PARENT_SCOPE)
    endif()
endfunction()

# 1
foreach(_p RUNTIME INFO INFO_STATIC)
    if(NOT EXISTS "${${_p}}")
        string(APPEND _problems "\n  ${_p}: ${${_p}} does not exist")
    endif()
endforeach()
base_has("${RUNTIME}" "libRSLRuntime")
base_has("${INFO}" "libRSLInfo")
base_has("${INFO_STATIC}" "libRSLInfo")
if(NOT "${INFO_STATIC}" MATCHES "\\.a$")
    string(APPEND _problems "\n  ${INFO_STATIC}: libRSLInfo_static is not an archive")
endif()
# 2
foreach(_pair "RUNTIME;RUNTIME_ALIAS" "INFO;INFO_ALIAS" "INFO_STATIC;INFO_STATIC_ALIAS" "INFO;RSLO_ALIAS" "INFO_STATIC;RSLO_STATIC_ALIAS")
    list(GET _pair 0 _a)
    list(GET _pair 1 _b)
    real("${${_a}}" _ra)
    real("${${_b}}" _rb)
    if(NOT _ra STREQUAL _rb)
        string(APPEND _problems "\n  ${_b} resolves to ${_rb}, not to ${_a} (${_ra})")
    endif()
endforeach()
# 3: the compatibility symlinks, next to the real files
get_filename_component(_dir_runtime "${RUNTIME}" DIRECTORY)
get_filename_component(_dir_info "${INFO}" DIRECTORY)
get_filename_component(_dir_static "${INFO_STATIC}" DIRECTORY)
if(APPLE)
    set(_shared dylib)
else()
    set(_shared so)
endif()
foreach(_c "${_dir_runtime}/libshader_shading.${_shared}|${RUNTIME}" "${_dir_info}/libshader_runtime.${_shared}|${INFO}" "${_dir_static}/libshader_runtime.a|${INFO_STATIC}")
    string(REPLACE "|" ";" _parts "${_c}")
    list(GET _parts 0 _link)
    list(GET _parts 1 _target)
    if(NOT EXISTS "${_link}")
        string(APPEND _problems "\n  ${_link}: the compatibility file does not exist")
    elseif(NOT IS_SYMLINK "${_link}")
        string(APPEND _problems "\n  ${_link}: exists but is not a symlink")
    else()
        real("${_link}" _rl)
        real("${_target}" _rt)
        if(NOT _rl STREQUAL _rt)
            string(APPEND _problems "\n  ${_link} resolves to ${_rl}, not to ${_rt}")
        endif()
    endif()
endforeach()
# 4
base_has("${COMPILER}" "libshader_compiler")
# 5: a real (non-symlink) library file named libshader_* in the directories of the targets above must be the compiler's
set(_dirs "${_dir_runtime}" "${_dir_info}" "${_dir_static}")
list(REMOVE_DUPLICATES _dirs)
foreach(_d IN LISTS _dirs)
    file(GLOB _old "${_d}/libshader_*")
    foreach(_f IN LISTS _old)
        get_filename_component(_n "${_f}" NAME)
        if(NOT IS_SYMLINK "${_f}" AND NOT _n MATCHES "^libshader_compiler")
            string(APPEND _problems "\n  ${_f}: a real file with an old name")
        endif()
    endforeach()
endforeach()

if(NOT "${_problems}" STREQUAL "")
    message(FATAL_ERROR "Link_RenameAliases:${_problems}")
endif()
message(STATUS "Link_RenameAliases: libRSLRuntime and libRSLInfo are real, every old name resolves to them")
