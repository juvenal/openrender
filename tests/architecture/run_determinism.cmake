# Driver for Architecture_Determinism (spec 022 T050, SC-007): runs archcheck
# twice with --format json and fails unless both reports are byte-identical.
#
# Inputs (-D): ARCHCHECK, ARCHCHECK_ARGS (;-list), OUT_DIR.

foreach(run 1 2)
    execute_process(
        COMMAND ${ARCHCHECK} ${ARCHCHECK_ARGS} --format json
        OUTPUT_FILE ${OUT_DIR}/archcheck-determinism-${run}.json
        RESULT_VARIABLE exit_code)
    if(NOT exit_code MATCHES "^[01]$")
        message(FATAL_ERROR "archcheck run ${run} exited ${exit_code}")
    endif()
endforeach()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files
        ${OUT_DIR}/archcheck-determinism-1.json ${OUT_DIR}/archcheck-determinism-2.json
    RESULT_VARIABLE differ)
if(differ)
    message(FATAL_ERROR "archcheck reports differ between two runs (${OUT_DIR}/archcheck-determinism-{1,2}.json)")
endif()
message("archcheck reports identical across two runs")
