# ImageIO_NoColourConversion (spec 048, T059, P-Q10): decoded samples do not depend on the colour configuration.
#
#   cmake -DPROGRAM=<test_no_colour_conversion_read> -DOTHER_OCIO=<other.ocio> -DFIXTURES=<file;file;...>
#         -P run_two_ocio_envs.cmake
#
# Runs the program twice, with OCIO unset and with OCIO pointing at a configuration that differs from OpenColorIO's
# built-in one, and fails unless (1) both runs succeed, (2) their outputs are identical, and (3) on every line the
# legacy digest equals the OIIO digest.
foreach(var PROGRAM OTHER_OCIO FIXTURES)
    if(NOT ${var})
        message(FATAL_ERROR "ImageIO_NoColourConversion: pass -D${var}=...")
    endif()
endforeach()
if(NOT EXISTS "${OTHER_OCIO}")
    message(FATAL_ERROR "ImageIO_NoColourConversion: ${OTHER_OCIO} does not exist")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -E env --unset=OCIO "${PROGRAM}" ${FIXTURES}
    OUTPUT_VARIABLE out_unset ERROR_VARIABLE err_unset RESULT_VARIABLE rc_unset)
execute_process(COMMAND ${CMAKE_COMMAND} -E env "OCIO=${OTHER_OCIO}" "${PROGRAM}" ${FIXTURES}
    OUTPUT_VARIABLE out_other ERROR_VARIABLE err_other RESULT_VARIABLE rc_other)

set(failures 0)
if(NOT rc_unset EQUAL 0)
    message(STATUS "FAIL: the run with OCIO unset failed (${rc_unset}): ${err_unset}")
    math(EXPR failures "${failures}+1")
endif()
if(NOT rc_other EQUAL 0)
    message(STATUS "FAIL: the run with OCIO=${OTHER_OCIO} failed (${rc_other}): ${err_other}")
    math(EXPR failures "${failures}+1")
endif()
if(NOT out_unset STREQUAL out_other)
    message(STATUS "FAIL: the digests differ between the two colour configurations:\n--- OCIO unset\n${out_unset}--- OCIO=other\n${out_other}")
    math(EXPR failures "${failures}+1")
endif()
string(REGEX MATCHALL "[^\n]+" lines "${out_unset}")
list(LENGTH lines n)
if(n EQUAL 0)
    message(STATUS "FAIL: the program printed no digest")
    math(EXPR failures "${failures}+1")
endif()
foreach(line IN LISTS lines)
    if(line MATCHES "^([^ ]+) legacy=([0-9a-f]+) oiio=([0-9a-f]+)$")
        if(NOT CMAKE_MATCH_2 STREQUAL CMAKE_MATCH_3)
            message(STATUS "FAIL: ${CMAKE_MATCH_1}: the OIIO samples differ from the legacy samples")
            math(EXPR failures "${failures}+1")
        else()
            message(STATUS "ok:   ${CMAKE_MATCH_1}: legacy == oiio under both colour configurations")
        endif()
    endif()
endforeach()
if(failures GREATER 0)
    message(FATAL_ERROR "ImageIO_NoColourConversion: ${failures} check(s) failed")
endif()
