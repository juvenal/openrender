# PluginAbi_SingleCheckRoutine (spec 037, SC-008, FR-004). Run as
#   cmake -DSYMBOL=checkPluginVersion -DDEFINER=<lib> -DOTHERS="<lib>|<lib>" -DREFERENCERS="<lib>|<lib>" [-DNM=<nm>] -P this
#
# The version check is one routine. With nm over the built libraries:
#   * DEFINER (libRIBCommon) defines a symbol whose name contains SYMBOL;
#   * no library of OTHERS defines one;
#   * every library of REFERENCERS has an undefined reference to it (the display host, the shadeop
#     library and the procedures objects call it; they do not carry a copy).
# Symbols are matched by substring, with or without Mach-O's leading underscore (as the T019 check).
#
# DISABLED until T093: the display host does not exist before Phase 5.
if(NOT NM)
    find_program(NM nm)
endif()
if(NOT NM)
    message(FATAL_ERROR "PRECONDITION: nm not found")
endif()
string(REPLACE "|" ";" _others "${OTHERS}")
string(REPLACE "|" ";" _referencers "${REFERENCERS}")
set(_failures "")

function(_or_nm lib out)
    execute_process(COMMAND "${NM}" -g "${lib}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "PRECONDITION: nm failed on ${lib}: ${_err}")
    endif()
    set(${out} "${_out}" PARENT_SCOPE)
endfunction()

_or_nm("${DEFINER}" _text)
string(REGEX MATCHALL "[0-9a-fA-F]+ [TtSsDdBbWw] [^\n]*${SYMBOL}[^\n]*" _defs "${_text}")
if(NOT _defs)
    list(APPEND _failures "${SYMBOL} is not defined in ${DEFINER}")
endif()
foreach(_lib IN LISTS _others)
    _or_nm("${_lib}" _text)
    string(REGEX MATCHALL "[0-9a-fA-F]+ [TtSsDdBbWw] [^\n]*${SYMBOL}[^\n]*" _defs "${_text}")
    if(_defs)
        list(APPEND _failures "${SYMBOL} is also defined in ${_lib}")
    endif()
endforeach()
foreach(_lib IN LISTS _referencers)
    _or_nm("${_lib}" _text)
    string(REGEX MATCHALL "(^|\n) +U [^\n]*${SYMBOL}[^\n]*" _refs "${_text}")
    if(NOT _refs)
        list(APPEND _failures "${_lib} has no undefined reference to ${SYMBOL}")
    endif()
endforeach()

if(_failures)
    string(REPLACE ";" "\n  " _report "${_failures}")
    message(FATAL_ERROR "DEFECT SC-008: ${SYMBOL} is not one routine:\n  ${_report}")
endif()
