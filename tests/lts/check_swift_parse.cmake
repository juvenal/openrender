# Syntax-check the macOS Swift sources and the XCTest files.
# Those XCTest files are not an SPM test target (Package.swift has only an
# executable target), so `swift test` cannot run them without restructuring
# the package. Parsing them keeps the files from rotting until that happens.
if(NOT DEFINED SWIFTC OR NOT DEFINED SOURCES)
    message(FATAL_ERROR "Lts_SwiftParse: SWIFTC and SOURCES are required")
endif()
string(REPLACE "|" ";" _files "${SOURCES}")
set(_failures 0)
foreach(_file IN LISTS _files)
    if(NOT EXISTS "${_file}")
        message(SEND_ERROR "Lts_SwiftParse: missing ${_file}")
        math(EXPR _failures "${_failures} + 1")
        continue()
    endif()
    execute_process(
        COMMAND "${SWIFTC}" -parse -swift-version 6 "${_file}"
        RESULT_VARIABLE _rc ERROR_VARIABLE _err OUTPUT_VARIABLE _out)
    if(NOT _rc EQUAL 0)
        message(SEND_ERROR "Lts_SwiftParse: ${_file}\n${_out}\n${_err}")
        math(EXPR _failures "${_failures} + 1")
    else()
        message(STATUS "Lts_SwiftParse: ${_file}")
    endif()
endforeach()
if(_failures GREATER 0)
    message(FATAL_ERROR "Lts_SwiftParse: ${_failures} file(s) failed to parse")
endif()
