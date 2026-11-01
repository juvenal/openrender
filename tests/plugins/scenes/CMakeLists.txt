# Procedure scenes and their characterization (spec 037, T026).
#
#   ProceduresAbi_Scenes         a DLO loaded by its located absolute path (REYES), an implicit
#                                surface, and a DynamicLoad generator: decoded pixels against references
#                                captured from the unmodified tree (the references live with the display
#                                baselines, display_baseline/reference/<platform>/)
#   ProceduresAbi_MissingSymbol  the starting tree's exact error text for a DLO and an implicit module that
#                                lack an entry point (scenes/reference/missing_symbol.txt)
#
# Both are green on the starting tree by design.

set(PROC_RIB_DIR ${OR_PLUGIN_BIN}/scenes/rib)
set(PROC_SCENES "")

# or_proc_scene(<name> <output file> BODY <rib lines> [PRE <rib lines>] [SEARCHPATH <dirs>])
function(or_proc_scene name file)
    cmake_parse_arguments(ARG "" "BODY;PRE;SEARCHPATH" "" ${ARGN})
    set(SCENE_NAME ${name})
    set(SCENE_FILE ${file})
    set(PROC_BODY "${ARG_BODY}")
    set(PROC_PRE "${ARG_PRE}")
    set(PROC_SEARCHPATH "${ARG_SEARCHPATH}")
    if(PROC_SEARCHPATH STREQUAL "")
        set(PROC_SEARCHPATH "${OR_PLUGIN_BIN}/dlo/good")
    endif()
    or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/proc_scene.rib.in ${PROC_RIB_DIR}/${name}.rib)
    set(_scenes "${PROC_SCENES};${name}=${file}")
    set(PROC_SCENES "${_scenes}" PARENT_SCOPE)
endfunction()

set(_dlo_good ${OR_PLUGIN_BIN}/dlo/good/good.dsply)
set(_implicit_good ${OR_PLUGIN_BIN}/implicit/good/good.dsply)
set(_dynload_plain ${OR_PLUGIN_BIN}/dynamicload/plain/plain.dsply)

# A DLO is loaded by the path the scene gives. The REYES hider has no DLO dicing, so that scene renders
# the empty frame and proves the module loads. There is no raytrace DLO scene: on the starting tree
# Geometry "dlo" under Hider "raytrace" intersects, then ends in a bus error in CDLObject's shading path
# (found while writing this file; reported to the maintainer, not worked around here).
or_proc_scene(dlo_located dlo_located.tif
    BODY "    Geometry \"dlo\" \"string file\" [\"${_dlo_good}\"]")
or_proc_scene(implicit_located implicit_located.tif
    PRE "Hider \"raytrace\""
    BODY "    Geometry \"implicit\" \"string file\" [\"${_implicit_good}\"]")
or_proc_scene(dynamicload_plain dynamicload_plain.tif
    BODY "    Procedural \"DynamicLoad\" [\"${_dynload_plain}\" \"\"] [-1 1 -1 1 -1 1]")

or_plugin_scene_set(TEST ProceduresAbi_Scenes
    RIB_DIR ${PROC_RIB_DIR}
    SCENES ${PROC_SCENES}
    DEPENDS file plugin_dlo_good plugin_implicit_good plugin_dynamicload_plain)

# --- ProceduresAbi_MissingSymbol -------------------------------------------------------------
set(MISSING_RIB_DIR ${OR_PLUGIN_BIN}/scenes/rib_missing)
set(SCENE_NAME dlo_missing_tini)
set(PROC_BODY "    Geometry \"dlo\" \"string file\" [\"${OR_PLUGIN_BIN}/dlo/missing_symbol/missing_symbol.dsply\"]")
or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/missing_symbol.rib.in ${MISSING_RIB_DIR}/dlo_missing_tini.rib)
set(SCENE_NAME implicit_missing_tini)
set(PROC_BODY "    Geometry \"implicit\" \"string file\" [\"${OR_PLUGIN_BIN}/implicit/missing_symbol/missing_symbol.dsply\"]")
or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/missing_symbol.rib.in ${MISSING_RIB_DIR}/implicit_missing_tini.rib)

set(_missing_scenes "dlo_missing_tini=${MISSING_RIB_DIR}/dlo_missing_tini.rib|implicit_missing_tini=${MISSING_RIB_DIR}/implicit_missing_tini.rib")
set(_missing_ref ${CMAKE_CURRENT_SOURCE_DIR}/reference/missing_symbol.txt)
set(_missing_common
    -DORENDER=$<TARGET_FILE:orender>
    -DCOMMON=${OR_PLUGIN_COMMON_DIR}
    "-DSCENES=${_missing_scenes}"
    -DBUILD_DIR=${CMAKE_BINARY_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_missing_symbol.cmake)
or_plugin_add_test(ProceduresAbi_MissingSymbol 120
    COMMAND ${CMAKE_COMMAND} -DMODE=compare -DREF=${_missing_ref}
        -DWORKROOT=${OR_PLUGIN_BIN}/scenes/work/missing ${_missing_common})
set_tests_properties(ProceduresAbi_MissingSymbol PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

set(_env ${CMAKE_COMMAND} -E env "SHADERS=${CMAKE_BINARY_DIR}/shaders" "ORENDERHOME=${CMAKE_SOURCE_DIR}")
or_plugin_capture(ProceduresAbi_MissingSymbol
    OFFICIAL ${_env} ${CMAKE_COMMAND} -DMODE=capture -DRECAPTURE=${OR037_RECAPTURE} -DREF=${_missing_ref}
        -DWORKROOT=${OR_PLUGIN_BIN}/scenes/capture_work/missing ${_missing_common}
    SCRATCH ${_env} ${CMAKE_COMMAND} -DMODE=capture -DRECAPTURE=ON -DREF=${OR_CAPTURE_SCRATCH}/missing_symbol.txt
        -DWORKROOT=${OR_PLUGIN_BIN}/scenes/capture_work_scratch/missing ${_missing_common})
foreach(_target orender plugin_dlo_missing_symbol plugin_implicit_missing_symbol)
    add_dependencies(plugin-abi-capture-baselines ${_target})
    add_dependencies(plugin-abi-capture-baselines-scratch ${_target})
endforeach()
