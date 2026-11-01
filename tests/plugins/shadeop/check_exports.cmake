# ShadeopAbi_ExampleBuilds (FR-008). Run as
#   cmake -DMODULE=<built example module> -DREADME=<examples/dsotest/README.md> [-DNM=<nm>] -P this
#
# The example DSO (examples/dsotest) is built as the test module plugin_shadeop_example. It must
# export orShadeopAbiVersion (SHADEOP_DECLARE_VERSION()), and its README must state the interface
# version and the concurrency rule (exec may be called concurrently from several shading threads).
if(NOT NM)
    find_program(NM nm)
endif()
if(NOT NM)
    message(FATAL_ERROR "PRECONDITION: nm not found")
endif()
if(NOT EXISTS "${MODULE}")
    message(FATAL_ERROR "PRECONDITION: ${MODULE} does not exist (the example failed to build)")
endif()
set(_failures 0)

execute_process(COMMAND "${NM}" -g "${MODULE}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "PRECONDITION: nm failed on ${MODULE}: ${_err}")
endif()
if(NOT _out MATCHES "[0-9a-fA-F]+ [TtSsDdBbRrWw] _?orShadeopAbiVersion")
    message("FAIL: ${MODULE} does not export orShadeopAbiVersion; the example must call SHADEOP_DECLARE_VERSION() (FR-008)")
    math(EXPR _failures "${_failures} + 1")
endif()

file(READ "${README}" _readme)
if(NOT _readme MATCHES "SHADEOP_DECLARE_VERSION")
    message("FAIL: ${README} does not mention SHADEOP_DECLARE_VERSION (the interface version the example declares)")
    math(EXPR _failures "${_failures} + 1")
endif()
if(NOT _readme MATCHES "[Cc]oncurrent")
    message("FAIL: ${README} does not state the concurrency rule (exec may be called concurrently from several shading threads)")
    math(EXPR _failures "${_failures} + 1")
endif()

if(_failures GREATER 0)
    message(FATAL_ERROR "ShadeopAbi_ExampleBuilds: ${_failures} failure(s).")
endif()
