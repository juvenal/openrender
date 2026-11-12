# Spec 032: records the references of the characterization tests T-A, T-B and T-F, from scratch.
#
# Run by the build targets gi-split-capture-references and gi-split-capture-references-scratch (tests/gi_split/CMakeLists.txt),
# never by ctest. A fresh work directory is made every time and fixture F1 is rendered into it, so nothing of an earlier run
# is read back. For each reference: a missing file is written by the test's own `--record` mode; an existing one is never
# overwritten unless the tree was configured with -DOR032_RECAPTURE=ON (the pattern of spec 037's plugin-abi-capture-baselines).
#
# Inputs (-D): RECAPTURE, WORKROOT, ORENDER, RIB_DIR, F1_RIB, T_FILES, T_LOOPBACK, T_SCENE, OUT_GI (the directory of
# files.txt and the two loopback-*.bin), OUT_PREVIEW (the directory of datascene.txt). The environment (SHADERS, ...) is
# the caller's.

foreach(_v WORKROOT ORENDER RIB_DIR F1_RIB T_FILES T_LOOPBACK T_SCENE OUT_GI OUT_PREVIEW)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "capture_references.cmake: ${_v} is not set")
    endif()
endforeach()

# ref <path>: true when the reference must be written (missing, or RECAPTURE)
function(gi_wants_write path out)
    if(EXISTS "${path}" AND NOT RECAPTURE)
        message(FATAL_ERROR "${path} exists; refusing to overwrite (configure with -DOR032_RECAPTURE=ON to recapture on purpose)")
    endif()
    set(${out} TRUE PARENT_SCOPE)
endfunction()

# gi_run(<label> <working directory> <command...>)
function(gi_run label wd)
    execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${wd}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    message(STATUS "${label}:\n${_out}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${label} failed (${_rc}):\n${_err}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORKROOT}")
foreach(_d fixture files loopback-cache loopback-pointcloud scene)
    file(MAKE_DIRECTORY "${WORKROOT}/${_d}")
endforeach()
file(MAKE_DIRECTORY "${OUT_GI}" "${OUT_PREVIEW}")

# refuse before running anything when a reference exists
gi_wants_write("${OUT_GI}/files.txt" _w)
gi_wants_write("${OUT_GI}/loopback-cache.bin" _w)
gi_wants_write("${OUT_GI}/loopback-pointcloud.bin" _w)
gi_wants_write("${OUT_PREVIEW}/datascene.txt" _w)

# fixture F1, rendered from scratch at -t:1
gi_run("render F1" "${WORKROOT}/fixture" ${ORENDER} -t:1 ${F1_RIB})
set(_f1 "${WORKROOT}/fixture/icache-fixture.icache")
if(NOT EXISTS "${_f1}")
    message(FATAL_ERROR "fixture F1 wrote no cache: ${_f1}")
endif()

gi_run("T-F files.txt" "${WORKROOT}/files" ${T_FILES} ${ORENDER} ${RIB_DIR} ${WORKROOT}/files --record ${OUT_GI}/files.txt)
gi_run("T-A loopback-cache.bin" "${WORKROOT}/loopback-cache" ${T_LOOPBACK} cache ${_f1} ${WORKROOT}/loopback-cache --record ${OUT_GI}/loopback-cache.bin)
gi_run("T-A loopback-pointcloud.bin" "${WORKROOT}/loopback-pointcloud" ${T_LOOPBACK} pointcloud - ${WORKROOT}/loopback-pointcloud --record ${OUT_GI}/loopback-pointcloud.bin)
gi_run("T-B datascene.txt" "${WORKROOT}/scene" ${T_SCENE} ${ORENDER} ${RIB_DIR} ${_f1} ${WORKROOT}/scene --record ${OUT_PREVIEW}/datascene.txt)
message(STATUS "recorded the four references under ${OUT_GI} and ${OUT_PREVIEW}")
