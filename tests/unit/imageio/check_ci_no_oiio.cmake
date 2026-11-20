# ImageIO_CiHasNoOiio (spec 048, T035, P-Q11, FR-003): no CI job enables OIIO, and at least one build job exists, so
# there is at least one build without it. A cmake -P script, no git.
#
#   cmake -DWORKFLOW=<.github/workflows/release.yml> -P check_ci_no_oiio.cmake
if(NOT WORKFLOW OR NOT EXISTS "${WORKFLOW}")
    message(FATAL_ERROR "ImageIO_CiHasNoOiio: workflow file not found: '${WORKFLOW}'")
endif()
file(READ "${WORKFLOW}" text)
string(TOLOWER "${text}" lower)

set(failures 0)
foreach(needle "openrender_enable_oiio" "openimageio")
    string(FIND "${lower}" "${needle}" pos)
    if(NOT pos EQUAL -1)
        message(STATUS "FAIL: the workflow mentions '${needle}'")
        math(EXPR failures "${failures}+1")
    else()
        message(STATUS "ok:   the workflow does not mention '${needle}'")
    endif()
endforeach()

# A job whose key starts with "build" (two-space indent under `jobs:`).
string(REGEX MATCHALL "\n  build[A-Za-z0-9_-]*:" build_jobs "${text}")
list(LENGTH build_jobs n)
if(n LESS 1)
    message(STATUS "FAIL: the workflow has no build job")
    math(EXPR failures "${failures}+1")
else()
    message(STATUS "ok:   the workflow has ${n} build job(s), none of which enables OIIO")
endif()

if(failures GREATER 0)
    message(FATAL_ERROR "ImageIO_CiHasNoOiio: ${failures} check(s) failed")
endif()
