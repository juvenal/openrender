# T-L4 Link_PackageStaticConsumer (spec 029, P-Q14 (a), FR-004, SC-002). Opt-in: registered only with
# -DOPENRENDER_LINK_PACKAGE_TEST=ON, because it builds and installs a second copy of the tree.
#
# 1. a scratch configure of this source tree with CMAKE_INSTALL_PREFIX=<scratch> (so GitHub #23 is not hit), build, install;
#    with -DPREFIX_DIR=<an existing install> that step is skipped (the Phase 2 evidence run reuses T015's build-pkg install);
# 2. a consumer project (tests/link/package) that uses find_package(openRender COMPONENTS Static) and nothing else is
#    configured, built and run once per shader format; it renders the #24 scene and asserts a renderer backend registered.
#
# Nested builds call `cmake --build -j<JOBS>` directly, never cpujob: the outer ctest job already holds the machine-wide
# lock, and a nested cpujob would wait on itself (4 on macOS, 2 on Linux).
#
# On macOS the consumer is configured with the vcpkg toolchain line read from the main build's cache (CMAKE_TOOLCHAIN_FILE,
# VCPKG_MANIFEST_MODE=OFF, VCPKG_INSTALLED_DIR, VCPKG_TARGET_TRIPLET), because the installed Static component does not
# carry the static libtiff/OpenEXR dependencies by itself: GitHub #141. This runner does not work around #141 in the package.
#
# -DREMOVE_PACKAGE_BLOCK=ON is the red mechanism (Phase 1, T015/T022; Phase 2's evidence): the package's
# -force_load / --whole-archive / --export-dynamic block is deleted from a scratch copy of the installed config, and the
# run then SUCCEEDS (exit 0) only if every format FAILS (link error or launch abort), which is what shows the consumer of the
# shipped package passes only because of the block. The registered test never sets it.
#
#   -DSOURCE_DIR -DBUILD_DIR -DWORK_DIR -DCONSUMER_DIR -DJOBS -DFORMATS [-DGENERATOR -DBUILD_TYPE -DPREFIX_DIR -DREMOVE_PACKAGE_BLOCK]

foreach(_v SOURCE_DIR BUILD_DIR WORK_DIR CONSUMER_DIR JOBS FORMATS)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "run_package_test: -D${_v}= is required")
    endif()
endforeach()
if(NOT BUILD_TYPE)
    set(BUILD_TYPE Release)
endif()

function(_run LABEL)
    message(STATUS "[${LABEL}] ${ARGN}")
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    message(STATUS "${_out}${_err}")
    set(_RC "${_rc}" PARENT_SCOPE)
    set(_OUT "${_out}${_err}" PARENT_SCOPE)
endfunction()

function(_must LABEL)
    _run("${LABEL}" ${ARGN})
    if(NOT _RC EQUAL 0)
        message(FATAL_ERROR "run_package_test: ${LABEL} failed (${_RC})")
    endif()
endfunction()

# ---- 1. the install ---------------------------------------------------------------------------------------------
file(MAKE_DIRECTORY "${WORK_DIR}")
if(PREFIX_DIR)
    set(_prefix "${PREFIX_DIR}")
    message(STATUS "run_package_test: reusing the install at ${_prefix}")
else()
    set(_prefix "${WORK_DIR}/prefix")
    set(_gen "")
    if(GENERATOR)
        set(_gen -G "${GENERATOR}")
    endif()
    _must(configure "${CMAKE_COMMAND}" ${_gen} -S "${SOURCE_DIR}" -B "${WORK_DIR}/tree" -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
          "-DCMAKE_INSTALL_PREFIX=${_prefix}")
    _must(build "${CMAKE_COMMAND}" --build "${WORK_DIR}/tree" --config ${BUILD_TYPE} -j${JOBS})
    _must(install "${CMAKE_COMMAND}" --install "${WORK_DIR}/tree" --config ${BUILD_TYPE})
endif()

# ---- 2. the red mechanism: the block deleted from a scratch copy of the installed config -----------------------------
if(REMOVE_PACKAGE_BLOCK)
    set(_copy "${WORK_DIR}/prefix_noblock")
    file(REMOVE_RECURSE "${_copy}")
    file(COPY "${_prefix}/" DESTINATION "${_copy}" USE_SOURCE_PERMISSIONS)
    set(_cfg "${_copy}/lib/cmake/openRender/openRenderConfig.cmake")
    file(READ "${_cfg}" _text)
    string(FIND "${_text}" "    if(APPLE)\n        set_property(TARGET openRender::RIBRender_static" _a)
    string(FIND "${_text}" "endif()\nunset(_openRender_static_missing)" _b)
    if(_a EQUAL -1 OR _b EQUAL -1)
        message(FATAL_ERROR "run_package_test: the package block was not found in ${_cfg}")
    endif()
    string(SUBSTRING "${_text}" 0 ${_a} _head)
    string(SUBSTRING "${_text}" ${_b} -1 _tail)
    file(WRITE "${_cfg}" "${_head}    # (spec 029 scratch: the force_load / whole-archive block deleted)\n${_tail}")
    set(_prefix "${_copy}")
endif()

# ---- 3. the consumer ---------------------------------------------------------------------------------------------
set(_cbuild "${WORK_DIR}/consumer-build")
file(REMOVE_RECURSE "${_cbuild}")
set(_toolchain "")
if(APPLE OR CMAKE_HOST_APPLE)
    foreach(_k CMAKE_TOOLCHAIN_FILE VCPKG_INSTALLED_DIR VCPKG_TARGET_TRIPLET)
        file(STRINGS "${BUILD_DIR}/CMakeCache.txt" _line REGEX "^${_k}:[A-Z]+=")
        if(_line)
            string(REGEX REPLACE "^[^=]*=" "" _val "${_line}")
            list(APPEND _toolchain "-D${_k}=${_val}")
        endif()
    endforeach()
    if(_toolchain)
        list(APPEND _toolchain -DVCPKG_MANIFEST_MODE=OFF)
    endif()
endif()
set(_ok_all TRUE)
_run(consumer-configure "${CMAKE_COMMAND}" -S "${CONSUMER_DIR}" -B "${_cbuild}" -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
     "-DCMAKE_PREFIX_PATH=${_prefix}" ${_toolchain})
set(_built FALSE)
if(_RC EQUAL 0)
    _run(consumer-build "${CMAKE_COMMAND}" --build "${_cbuild}" --config ${BUILD_TYPE} -j${JOBS})
    if(_RC EQUAL 0)
        set(_built TRUE)
    endif()
endif()

set(_failed_formats 0)
foreach(_fmt IN LISTS FORMATS)
    if(NOT _built)
        math(EXPR _failed_formats "${_failed_formats} + 1")
        message(STATUS "format ${_fmt}: the consumer did not build")
        continue()
    endif()
    set(_run_dir "${WORK_DIR}/run-${_fmt}")
    file(REMOVE_RECURSE "${_run_dir}")
    file(MAKE_DIRECTORY "${_run_dir}")
    # the consumer is a plain executable of the build tree (single-config generators put it at the top)
    set(_exe "${_cbuild}/consumer")
    if(NOT EXISTS "${_exe}" AND EXISTS "${_cbuild}/${BUILD_TYPE}/consumer")
        set(_exe "${_cbuild}/${BUILD_TYPE}/consumer")
    endif()
    message(STATUS "[consumer-run ${_fmt}] ${_exe}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env "SHADERS=${_prefix}/shaders" "ORENDERHOME=${_prefix}" "DISPLAYS=${_prefix}/displays"
                            "GEOMETRIES=${_prefix}/geometry" "${_exe}" ${_fmt}
                    WORKING_DIRECTORY "${_run_dir}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    message(STATUS "${_out}${_err}")
    if(NOT _rc EQUAL 0)
        math(EXPR _failed_formats "${_failed_formats} + 1")
        message(STATUS "format ${_fmt}: the consumer failed (${_rc})")
    endif()
endforeach()

list(LENGTH FORMATS _nformats)
if(REMOVE_PACKAGE_BLOCK)
    if(_failed_formats EQUAL _nformats)
        message(STATUS "run_package_test: with the package block removed every format fails, as the red mechanism requires")
    else()
        message(FATAL_ERROR "run_package_test: the consumer passed without the package block (${_failed_formats} of ${_nformats} formats failed)")
    endif()
else()
    if(_failed_formats GREATER 0)
        message(FATAL_ERROR "Link_PackageStaticConsumer: ${_failed_formats} of ${_nformats} formats failed")
    endif()
endif()
