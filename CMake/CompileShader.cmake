# CompileShader.cmake — run one oshader compile quietly (cmake -P script).
#
# Invoked by shaders/CMakeLists.txt once per shader and format:
#   cmake -DOSHADER=<oshader> -DSRC=<x.sl> -DOUT=<x.rslo|x.slo> -DINCDIR=<dir>
#         [-DJIT=ON] -P CompileShader.cmake
#
# oshader prints two progress lines per shader ("Compiling x.sl" and
# "... compiled x.rslo") that its -q flag does not yet silence. Across 69
# shaders and two formats that buried the build log, so this wrapper captures
# oshader's output and drops those two lines. Anything else on success (e.g.
# warnings) is passed through; on failure everything is printed, unfiltered.
# ShaderBuildSummary.cmake reports the per-format totals instead.

foreach(_var OSHADER SRC OUT INCDIR)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "CompileShader.cmake: ${_var} is not set")
    endif()
endforeach()

set(ENV{SHADERS_INCLUDE} "${INCDIR}")

set(_jit_flag "")
if(JIT)
    set(_jit_flag "--jit")
endif()

execute_process(
    COMMAND "${OSHADER}" ${_jit_flag} -o "${OUT}" "${SRC}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE  _output)

if(NOT _result EQUAL 0)
    # Don't leave a partial object behind to look up to date next build.
    file(REMOVE "${OUT}")
    message(FATAL_ERROR "${_output}oshader failed (${_result}) compiling ${SRC}")
endif()

# CMake regexes have no multiline mode, so anchor each line on its leading
# newline instead of ^.
string(REGEX REPLACE "\n(Compiling [^\n]*|\\.\\.\\. compiled [^\n]*)" "" _output "\n${_output}")
string(STRIP "${_output}" _output)
if(NOT _output STREQUAL "")
    message("${_output}")
endif()
