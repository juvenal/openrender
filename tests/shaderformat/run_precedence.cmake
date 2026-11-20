# ShaderFormat_Precedence (spec 033, research R17): the characterisation of the shader format precedence.
#
#   cmake -DORENDER=<orender> -DOSHADER=<oshader> -DREAD_PIXEL=<shaderformat_read_pixel> -DFIX=<fixture dir>
#         -DWORK=<scratch> -DREF=<reference file> -DJIT=ON|OFF -DDISPLAYS=<display driver dir> -DBUILD_DIR=<build>
#         -DDSO_PROBE_SL=<probe.sl> -DDSO_DIR=<dir of the shadeop fixture module> -DORENDERHOME=<dir>
#         -DSHIPPED_SHADERS=<the build tree's shaders directory, for defaultsurface and defaultlight>
#         -P run_precedence.cmake
#
# One shader name ("fmtp") has a .rslo that renders red and a .slo that renders green, so the pixel of a 4x4 render
# tells which backend ran. For every combination of the attribute "shade" "shaderformat" {slo, rslo, unset, invalid} x
# the option "shaderformat" "default" {slo, rslo, unset} x the files on disk {both, rslo only, slo only, neither} the
# driver stages the files in a directory of its own, renders and records the pixel, the exit status and the
# diagnostics (stdout, where the warnings go, and stderr). Further cases: a dotted name, a non-bitcode file named
# .slo, an .slo calling a shadeop operand whose module is not in the search path (gotcha 33), and the cache case (one
# name asked for slo and for rslo in one scene, GitHub #167). In a tree without the JIT the .slo files do not exist:
# the same driver records its own reference (precedence-nojit.txt).
#
# The record is compared with REF as text. A missing REF is written on the first run and never overwritten: this
# script has no mode that replaces an existing reference.

foreach(_v ORENDER OSHADER READ_PIXEL FIX WORK REF JIT DISPLAYS BUILD_DIR ORENDERHOME SHIPPED_SHADERS)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "run_precedence.cmake: ${_v} is not set")
    endif()
endforeach()

set(ENV{ORENDERHOME} "${ORENDERHOME}")
set(ENV{DISPLAYS} "${DISPLAYS}")
set(ENV{SHADERS} "")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(_record "")
set(_case_index 0)

# Sanitise a block of diagnostics: the scratch and build paths are replaced, empty lines dropped
function(sanitize text out)
    string(REPLACE "${WORK}" "<work>" text "${text}")
    string(REPLACE "${BUILD_DIR}" "<build>" text "${text}")
    string(REPLACE "\r" "" text "${text}")
    # the warnings are printed without a newline, one after the other: one diagnostic per line
    string(REPLACE "./scene.rib (" "\n./scene.rib (" text "${text}")
    string(REPLACE "\n" ";" lines "${text}")
    set(result "")
    foreach(l IN LISTS lines)
        string(STRIP "${l}" l)
        if(NOT l STREQUAL "")
            string(APPEND result "    ${l}\n")
        endif()
    endforeach()
    set(${out} "${result}" PARENT_SCOPE)
endfunction()

# stage(<dir> <file>...): "<dest name>=<source>" pairs
function(stage dir)
    file(MAKE_DIRECTORY "${dir}")
    foreach(pair IN LISTS ARGN)
        string(REGEX REPLACE "=.*$" "" dest "${pair}")
        string(REGEX REPLACE "^[^=]*=" "" src "${pair}")
        if(src STREQUAL "TEXT")
            file(WRITE "${dir}/${dest}" "this is not LLVM bitcode\n")
        else()
            file(COPY "${src}" DESTINATION "${dir}")
            get_filename_component(srcname "${src}" NAME)
            if(NOT srcname STREQUAL dest)
                file(RENAME "${dir}/${srcname}" "${dir}/${dest}")
            endif()
        endif()
    endforeach()
endfunction()

# run_scene(<id> <stage dir> <rib body lines> <pixel list "x,y;x,y">)
function(run_scene id stage_dir body pixels)
    math(EXPR _n "${_case_index} + 1")
    set(_case_index ${_n} PARENT_SCOPE)
    set(dir "${WORK}/c${_n}")
    file(MAKE_DIRECTORY "${dir}")
    file(WRITE "${dir}/scene.rib"
"version 3.03
Hider \"reyes\" \"jitter\" [0]
Option \"limits\" \"numthreads\" [1]
Option \"searchpath\" \"shader\" [\"${stage_dir}:${SHIPPED_SHADERS}\"]
Option \"searchpath\" \"display\" [\"${DISPLAYS}\"]
${PRE_OPTION}PixelSamples 1 1
PixelFilter \"box\" 1 1
Quantize \"rgba\" 0 0 0 0
Display \"out.tif\" \"file\" \"rgba\"
Format 4 4 1
Projection \"perspective\" \"fov\" 40
Translate 0 0 3
WorldBegin
${body}WorldEnd
")
    execute_process(COMMAND "${ORENDER}" scene.rib WORKING_DIRECTORY "${dir}"
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 120)
    set(text "case ${id}\n")
    foreach(p IN LISTS pixels)
        string(REPLACE "," ";" xy "${p}")
        list(GET xy 0 x)
        list(GET xy 1 y)
        execute_process(COMMAND "${READ_PIXEL}" "${dir}/out.tif" ${x} ${y} OUTPUT_VARIABLE px)
        string(STRIP "${px}" px)
        string(APPEND text "  pixel(${x},${y}) ${px}\n")
    endforeach()
    string(APPEND text "  exit ${rc}\n")
    sanitize("${out}" so)
    sanitize("${err}" se)
    if(NOT so STREQUAL "")
        string(APPEND text "  stdout:\n${so}")
    endif()
    if(NOT se STREQUAL "")
        string(APPEND text "  stderr:\n${se}")
    endif()
    set(_record "${_record}${text}" PARENT_SCOPE)
endfunction()

set(red "fmtp_red.rslo")
set(green "fmtp_green.slo")

# ---- the table: attribute x option x files ------------------------------------------------------------------------
if(JIT)
    set(_filesets both rslo slo none)
else()
    set(_filesets rslo none)
endif()
foreach(attr slo rslo unset invalid)
    foreach(opt slo rslo unset)
        foreach(files IN LISTS _filesets)
            set(stage_dir "${WORK}/stage/${attr}-${opt}-${files}")
            set(pairs "")
            if(files STREQUAL "both" OR files STREQUAL "rslo")
                list(APPEND pairs "fmtp.rslo=${FIX}/fmtp_red.rslo")
            endif()
            if(files STREQUAL "both" OR files STREQUAL "slo")
                list(APPEND pairs "fmtp.slo=${FIX}/fmtp_green.slo")
            endif()
            stage("${stage_dir}" ${pairs})
            set(PRE_OPTION "")
            if(NOT opt STREQUAL "unset")
                set(PRE_OPTION "Option \"shaderformat\" \"default\" [\"${opt}\"]\n")
            endif()
            set(body "")
            if(attr STREQUAL "invalid")
                set(body "  Attribute \"shade\" \"shaderformat\" [\"bogus\"]\n")
            elseif(NOT attr STREQUAL "unset")
                set(body "  Attribute \"shade\" \"shaderformat\" [\"${attr}\"]\n")
            endif()
            string(APPEND body "  Surface \"fmtp\"\n  Polygon \"P\" [-10 -10 0  10 -10 0  10 10 0  -10 10 0]\n")
            run_scene("attr=${attr} option=${opt} files=${files}" "${stage_dir}" "${body}" "1,1")
        endforeach()
    endforeach()
endforeach()
set(PRE_OPTION "")

# ---- a dotted name: the name is looked up as written, whatever the preference ------------------------------------
if(JIT)
    foreach(attr slo rslo)
        foreach(name fmtd.rslo fmtd.slo)
            set(stage_dir "${WORK}/stage/dotted-${attr}-${name}")
            stage("${stage_dir}" "fmtd.rslo=${FIX}/fmtp_red.rslo" "fmtd.slo=${FIX}/fmtp_green.slo")
            set(body "  Attribute \"shade\" \"shaderformat\" [\"${attr}\"]\n  Surface \"${name}\"\n  Polygon \"P\" [-10 -10 0  10 -10 0  10 10 0  -10 10 0]\n")
            run_scene("dotted name=${name} attr=${attr}" "${stage_dir}" "${body}" "1,1")
        endforeach()
    endforeach()
endif()
foreach(attr slo rslo)
    # a dot that is not an extension: the file is named exactly as the shader is
    set(stage_dir "${WORK}/stage/dotted-v2-${attr}")
    stage("${stage_dir}" "fmtd.v2=${FIX}/fmtp_red.rslo")
    set(body "  Attribute \"shade\" \"shaderformat\" [\"${attr}\"]\n  Surface \"fmtd.v2\"\n  Polygon \"P\" [-10 -10 0  10 -10 0  10 10 0  -10 10 0]\n")
    run_scene("dotted name=fmtd.v2 (an .rslo file without extension) attr=${attr}" "${stage_dir}" "${body}" "1,1")
endforeach()

# ---- a file named .slo that is not bitcode -----------------------------------------------------------------------
if(JIT)
    foreach(attr slo rslo)
        foreach(files with-rslo alone)
            set(stage_dir "${WORK}/stage/junk-${attr}-${files}")
            set(pairs "fmtp.slo=TEXT")
            if(files STREQUAL "with-rslo")
                list(APPEND pairs "fmtp.rslo=${FIX}/fmtp_red.rslo")
            endif()
            stage("${stage_dir}" ${pairs})
            set(body "  Attribute \"shade\" \"shaderformat\" [\"${attr}\"]\n  Surface \"fmtp\"\n  Polygon \"P\" [-10 -10 0  10 -10 0  10 10 0  -10 10 0]\n")
            run_scene("non-bitcode .slo ${files} attr=${attr}" "${stage_dir}" "${body}" "1,1")
        endforeach()
    endforeach()
endif()

# ---- an .slo calling a shadeop operand that is not in the search path (gotcha 33) -------------------------------
if(JIT)
    set(stage_dir "${WORK}/stage/dso")
    file(MAKE_DIRECTORY "${stage_dir}")
    foreach(fmt rslo slo)
        set(_jit "")
        if(fmt STREQUAL "slo")
            set(_jit --jit)
        endif()
        execute_process(COMMAND "${OSHADER}" ${_jit} -q -I "${DSO_DIR}" -o "${stage_dir}/probe.${fmt}" "${DSO_PROBE_SL}"
            RESULT_VARIABLE rc OUTPUT_VARIABLE o ERROR_VARIABLE o TIMEOUT 120)
        if(NOT rc EQUAL 0 OR NOT EXISTS "${stage_dir}/probe.${fmt}")
            message(FATAL_ERROR "ShaderFormat_Precedence: compiling the DSO probe as ${fmt} exited ${rc}:\n${o}\n(the module directory is ${DSO_DIR})")
        endif()
    endforeach()
    foreach(attr slo rslo)
        set(body "  Attribute \"shade\" \"shaderformat\" [\"${attr}\"]\n  Surface \"probe\"\n  Polygon \"P\" [-10 -10 0  10 -10 0  10 10 0  -10 10 0]\n")
        run_scene("missing DSO operand attr=${attr}" "${stage_dir}" "${body}" "1,1")
    endforeach()
endif()

# ---- the cache: one name asked for in both formats in one scene (GitHub #167) --------------------------------------
if(JIT)
    set(stage_dir "${WORK}/stage/cache")
    stage("${stage_dir}" "fmtp.rslo=${FIX}/fmtp_red.rslo" "fmtp.slo=${FIX}/fmtp_green.slo")
    foreach(order slo-then-rslo rslo-then-slo)
        string(REPLACE "-then-" ";" _two "${order}")
        list(GET _two 0 first)
        list(GET _two 1 second)
        set(body "  AttributeBegin\n    Attribute \"shade\" \"shaderformat\" [\"${first}\"]\n    Surface \"fmtp\"\n    Polygon \"P\" [-10 -10 0  0 -10 0  0 10 0  -10 10 0]\n  AttributeEnd\n")
        string(APPEND body "  AttributeBegin\n    Attribute \"shade\" \"shaderformat\" [\"${second}\"]\n    Surface \"fmtp\"\n    Polygon \"P\" [0 -10 0  10 -10 0  10 10 0  0 10 0]\n  AttributeEnd\n")
        run_scene("cache ${order}" "${stage_dir}" "${body}" "0,1;3,1")
    endforeach()
endif()

# ---- compare with, or record, the reference -----------------------------------------------------------------------
file(WRITE "${WORK}/precedence.fresh.txt" "${_record}")
if(NOT EXISTS "${REF}")
    get_filename_component(_d "${REF}" DIRECTORY)
    file(MAKE_DIRECTORY "${_d}")
    file(WRITE "${REF}" "${_record}")
    message(STATUS "ShaderFormat_Precedence: recorded ${REF} (${_case_index} cases)")
    return()
endif()
file(READ "${REF}" _ref)
if(_ref STREQUAL _record)
    message(STATUS "ShaderFormat_Precedence: ${_case_index} cases identical to ${REF}")
    return()
endif()
# report the first differing line
string(REPLACE "\n" ";" _a "${_ref}")
string(REPLACE "\n" ";" _b "${_record}")
list(LENGTH _a _na)
list(LENGTH _b _nb)
set(_i 0)
while(_i LESS _na AND _i LESS _nb)
    list(GET _a ${_i} _la)
    list(GET _b ${_i} _lb)
    if(NOT _la STREQUAL _lb)
        break()
    endif()
    math(EXPR _i "${_i} + 1")
endwhile()
set(_la "<end>")
set(_lb "<end>")
if(_i LESS _na)
    list(GET _a ${_i} _la)
endif()
if(_i LESS _nb)
    list(GET _b ${_i} _lb)
endif()
message(FATAL_ERROR "ShaderFormat_Precedence differs from ${REF} at line ${_i}:\n  reference: ${_la}\n  fresh:     ${_lb}\nThe fresh record is ${WORK}/precedence.fresh.txt")
