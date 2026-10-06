# Driver for one ArchFixture_* test (see archcheck_add_fixture in CMakeLists.txt).
#
# Inputs (-D): ARCHCHECK, ARCHCHECK_ARGS (;-list), EXPECTED_EXIT, PASS_REGEX.
# Fails unless archcheck exits with EXPECTED_EXIT and its combined
# stdout+stderr matches PASS_REGEX.

execute_process(
    COMMAND ${ARCHCHECK} ${ARCHCHECK_ARGS}
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)

message("${out}${err}")

if(NOT exit_code STREQUAL "${EXPECTED_EXIT}")
    message(FATAL_ERROR "archcheck exited ${exit_code}, expected ${EXPECTED_EXIT}")
endif()
if(NOT "${out}${err}" MATCHES "${PASS_REGEX}")
    message(FATAL_ERROR "archcheck output does not match: ${PASS_REGEX}")
endif()
