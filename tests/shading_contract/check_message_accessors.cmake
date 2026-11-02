# ShadingContract_MessageAccessors (spec 026, T094; FR-029, B3): incident() and opposite() ask Shading for the
# message source Interior and Exterior respectively, and the accessor constants the interpreter and the JIT
# pass for them must agree with it. Checked on the source text:
#
#   shaderFunctions.h   every DEFFUNC(Incident*, "incident", ...) line passes ACCESSOR_INTERIOR to
#                       PARAMETEREXPR_PRE, and every DEFFUNC(Opposite*, "opposite", ...) line ACCESSOR_EXTERIOR
#   shading.cpp         CShadingContext::jitIncidentParameter passes ACCESSOR_INTERIOR to jitParameterFinish,
#                       and jitOppositeParameter ACCESSOR_EXTERIOR
#
# The accessor is only used to find the producer's locals for a mutable parameter, which incident() and
# opposite() never read (they skip mutable parameters), so no output depends on it. Red until Phase 5, where
# the lines and the two members pass them the other way round.
#
# Run with: cmake -DSHADER_FUNCTIONS=<shaderFunctions.h> -DSHADING_CPP=<shading.cpp> -P check_message_accessors.cmake

if(NOT SHADER_FUNCTIONS OR NOT SHADING_CPP)
    message(FATAL_ERROR "usage: cmake -DSHADER_FUNCTIONS=<shaderFunctions.h> -DSHADING_CPP=<shading.cpp> -P check_message_accessors.cmake")
endif()

set(_failures 0)

# The DEFFUNC lines of one builtin: each must name the expected accessor, and there must be four (V, F, S, M)
function(check_deffunc BUILTIN PREFIX EXPECTED OTHER)
    file(READ "${SHADER_FUNCTIONS}" _text)
    string(REGEX MATCHALL "DEFFUNC\\(${PREFIX}[A-Za-z]*, \"${BUILTIN}\"[^\n]*" _lines "${_text}")
    list(LENGTH _lines _count)
    if(NOT _count EQUAL 4)
        message(SEND_ERROR "shaderFunctions.h: expected 4 DEFFUNC lines for ${BUILTIN}(), found ${_count}")
    endif()
    foreach(_line IN LISTS _lines)
        if(NOT _line MATCHES "PARAMETEREXPR_PRE\\(${EXPECTED}[,)]")
            string(REGEX MATCH "DEFFUNC\\([A-Za-z]*" _name "${_line}")
            message(SEND_ERROR "shaderFunctions.h: ${_name}) must pass ${EXPECTED} (found: ${_line})")
        endif()
    endforeach()
endfunction()

# The jitParameterFinish call inside one member of CShadingContext
function(check_jit_member MEMBER EXPECTED)
    file(READ "${SHADING_CPP}" _text)
    string(FIND "${_text}" "void CShadingContext::${MEMBER}(" _start)
    if(_start EQUAL -1)
        message(SEND_ERROR "shading.cpp: CShadingContext::${MEMBER} not found")
        return()
    endif()
    string(SUBSTRING "${_text}" ${_start} -1 _rest)
    string(FIND "${_rest}" "\n}\n" _end)
    string(SUBSTRING "${_rest}" 0 ${_end} _body)
    string(REGEX MATCH "jitParameterFinish\\([^;]*;" _call "${_body}")
    if(_call STREQUAL "")
        message(SEND_ERROR "shading.cpp: ${MEMBER} has no jitParameterFinish call")
    elseif(NOT _call MATCHES "${EXPECTED}\\)")
        message(SEND_ERROR "shading.cpp: ${MEMBER} must pass ${EXPECTED} (found: ${_call})")
    endif()
endfunction()

check_deffunc("incident" "Incident" "ACCESSOR_INTERIOR" "ACCESSOR_EXTERIOR")
check_deffunc("opposite" "Opposite" "ACCESSOR_EXTERIOR" "ACCESSOR_INTERIOR")
check_jit_member("jitIncidentParameter" "ACCESSOR_INTERIOR")
check_jit_member("jitOppositeParameter" "ACCESSOR_EXTERIOR")
