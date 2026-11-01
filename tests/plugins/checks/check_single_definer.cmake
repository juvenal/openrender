# ShadeopAbi_ParserSingleDefiner (FR-037). Run as
#   cmake -DSYMBOL=<name> -DLIBRARIES="<lib>|<lib>|..." -DMODE=single|none [-DNM=<nm>] -P this
#
# Counts, with nm, how many of the given libraries *define* a symbol whose name contains SYMBOL.
#   MODE=single   fails when more than one library defines it
#   MODE=none     fails when any library defines it (the symbol has moved out of these libraries)
#   MODE=only     fails unless exactly one library defines it and its file name contains ONLY_IN
#                 (added for ShadeopAbi_ParserNewSymbol: defined once, in libRIBShadeop)
#
# A symbol matches by substring, so a C++ mangled name (_Z8dsoParsePKcRPcS2_) and a C name both
# count, with or without the leading underscore Mach-O adds. Only *defined* symbols count (nm
# types T, t, S, s, D, d, B, b, W, w); undefined references (U) never do.
if(NOT NM)
    find_program(NM nm)
endif()
if(NOT NM)
    message(FATAL_ERROR "PRECONDITION: nm not found")
endif()
if(NOT MODE STREQUAL "single" AND NOT MODE STREQUAL "none" AND NOT MODE STREQUAL "only")
    message(FATAL_ERROR "PRECONDITION: -DMODE must be single, none or only")
endif()
if(MODE STREQUAL "only" AND NOT ONLY_IN)
    message(FATAL_ERROR "PRECONDITION: -DMODE=only needs -DONLY_IN=<part of the library file name>")
endif()

string(REPLACE "|" ";" _libs "${LIBRARIES}")
set(_definers "")
set(_definer_leaves "")
foreach(_lib IN LISTS _libs)
    if(NOT EXISTS "${_lib}")
        message(FATAL_ERROR "PRECONDITION: library ${_lib} does not exist (build it first)")
    endif()
    # -g: external symbols only. Both macOS and GNU nm print "<address> <type> <name>"; the
    # address column is blank for undefined symbols, so a line with a type letter in that
    # position and a name containing SYMBOL is a definition when the type is not U.
    execute_process(COMMAND "${NM}" -g "${_lib}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "PRECONDITION: nm failed on ${_lib}: ${_err}")
    endif()
    string(REGEX MATCHALL "[0-9a-fA-F]+ [TtSsDdBbWw] [^\n]*${SYMBOL}[^\n]*" _hits "${_out}")
    list(LENGTH _hits _count)
    if(_count GREATER 0)
        get_filename_component(_leaf "${_lib}" NAME)
        list(GET _hits 0 _example)
        list(APPEND _definers "${_leaf} (${_count} symbol(s), e.g. ${_example})")
        list(APPEND _definer_leaves "${_leaf}")
    endif()
endforeach()

list(LENGTH _definers _n)
message("${SYMBOL} is defined in ${_n} of ${LIBRARIES}")
foreach(_d IN LISTS _definers)
    message("  ${_d}")
endforeach()
if(MODE STREQUAL "single" AND _n GREATER 1)
    message(FATAL_ERROR "DEFECT FR-037: ${SYMBOL} is defined in ${_n} libraries; it must be defined in exactly one")
endif()
if(MODE STREQUAL "none" AND _n GREATER 0)
    message(FATAL_ERROR "DEFECT FR-037: ${SYMBOL} is still defined in ${_n} of these libraries")
endif()
if(MODE STREQUAL "only")
    if(NOT _n EQUAL 1)
        message(FATAL_ERROR "DEFECT FR-037: ${SYMBOL} is defined in ${_n} of these libraries; it must be defined only in the one named *${ONLY_IN}*")
    endif()
    list(GET _definer_leaves 0 _only_leaf)
    if(NOT _only_leaf MATCHES "${ONLY_IN}")
        message(FATAL_ERROR "DEFECT FR-037: ${SYMBOL} is defined in ${_only_leaf}, not in the one named *${ONLY_IN}*")
    endif()
endif()
