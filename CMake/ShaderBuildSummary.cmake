# ShaderBuildSummary.cmake — report how many shaders this build compiled
# (cmake -P script).
#
# Invoked by the openrender_shaders target after all its shader commands:
#   cmake -DDIR=<shader build dir> "-DFORMATS=rslo;slo" -P ShaderBuildSummary.cmake
#
# Prints one line per format, e.g. "69 .rslo shaders compiled". A shader counts
# when its object is newer than the format's stamp, which is touched here after
# every report. Timestamps rather than per-compile markers keep the parallel
# compile jobs from sharing any state. A failed build never reaches this
# script, so the next report also counts whatever compiled before the failure.

foreach(_fmt IN LISTS FORMATS)
    set(_stamp "${DIR}/.summary-${_fmt}.stamp")
    file(GLOB _objects "${DIR}/*.${_fmt}")

    set(_count 0)
    foreach(_obj IN LISTS _objects)
        # IS_NEWER_THAN is also true when the stamp does not exist yet.
        if("${_obj}" IS_NEWER_THAN "${_stamp}")
            math(EXPR _count "${_count} + 1")
        endif()
    endforeach()

    message(STATUS "${_count} .${_fmt} shaders compiled")
    file(TOUCH "${_stamp}")
endforeach()
