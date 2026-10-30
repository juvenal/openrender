# stage_headers.cmake -- include() it; defines or_stage_headers().
#
# or_stage_headers(<source-dir> <stage-dir> <header-names>): copies the named public headers into
# <stage-dir>/include/, flat, the way `cmake --install` lays them out under
# ${CMAKE_INSTALL_INCLUDEDIR}. A name is a file name (dsply.h); it is looked up in the directories
# the headers have lived in, so the staging keeps working while headers move (git mv) during the
# spec. Sets OR_STAGED (the staged file names) and OR_STAGE_MISSING (names found nowhere).
function(or_stage_headers source_dir stage_dir names)
    set(_candidates
        src/ri/geometry src/display src/ri/texture src/ri/parse src/ri/render src/display/file
        src/shadeop src/procedures src/common)
    file(REMOVE_RECURSE "${stage_dir}")
    file(MAKE_DIRECTORY "${stage_dir}/include")
    set(_staged "")
    set(_missing "")
    foreach(_name IN LISTS names)
        set(_found "")
        foreach(_dir IN LISTS _candidates)
            if(EXISTS "${source_dir}/${_dir}/${_name}")
                set(_found "${source_dir}/${_dir}/${_name}")
                break()
            endif()
        endforeach()
        if(_found STREQUAL "")
            list(APPEND _missing "${_name}")
        else()
            file(COPY "${_found}" DESTINATION "${stage_dir}/include")
            list(APPEND _staged "${_name}")
        endif()
    endforeach()
    set(OR_STAGED "${_staged}" PARENT_SCOPE)
    set(OR_STAGE_MISSING "${_missing}" PARENT_SCOPE)
endfunction()
