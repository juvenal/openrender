# shaders/CMakeLists.txt — compile the shipped RSL shaders as a BUILD step.
#
# Output goes to ${CMAKE_BINARY_DIR}/shaders, never next to the .sl sources.
# The tracked tree holds sources only; compiled objects are build artifacts and
# keeping them out of it is what stops the two from drifting.
#
# This replaces the install(CODE) passes that used to compile shaders into the
# deploy tree. Consequences of the old arrangement, all now gone:
#   * `ctest -L visual` could not pass from a clean clone -- the 69 .rslo/.slo
#     the suite loads existed only after `cmake --install`.
#   * Nothing rebuilt bitcode when oshader changed, so a green -slo run proved
#     nothing unless you checked timestamps by hand (the CLAUDE.md gotcha).
#   * A deploy tree copied between machines carried that platform's objects.
#
# Each shader is its own add_custom_command, so the dependency graph is real:
# editing a .sl recompiles that shader, editing a .slh recompiles shaders, and
# rebuilding oshader recompiles everything.
#
# The .slh dependency is deliberately coarse -- every shader depends on every
# header rather than only the ones it includes, which would mean parsing #include
# lines out of each .sl. Only 7 of 69 shaders include anything, and a full
# recompile of all 138 objects measures ~2s at -j8, so the precision is not worth
# the machinery. Correct and over-eager beats clever and stale.
#
# Build output is one summary line per format ("69 .rslo shaders compiled"),
# not three lines per shader. Each compile runs through
# CMake/CompileShader.cmake, which hides oshader's progress lines (errors and
# warnings still show), with an empty COMMENT so make echoes nothing per
# shader. The openrender_shaders target then runs
# CMake/ShaderBuildSummary.cmake, which counts the objects this build rebuilt.

set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_BUILD_DIR}")

file(GLOB _shader_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.sl")
file(GLOB _shader_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/includes/*.slh")

# Includes are read straight from the source tree. SHADERS_INCLUDE rather than
# -I: combining -I with -o and a positional input trips oshader's argument
# parser ("Output file specified with multiple input files") even with exactly
# one input. See the CLI-quirk note in CLAUDE.md.
set(_shader_incdir "${CMAKE_CURRENT_SOURCE_DIR}/includes")
set(_compile_shader "${PROJECT_SOURCE_DIR}/CMake/CompileShader.cmake")

set(_shader_outputs "")
foreach(_sl IN LISTS _shader_sources)
    get_filename_component(_base "${_sl}" NAME_WE)

    # .rslo — interpreter bytecode. Always built; it is the authoritative
    # format and the fallback whenever a .slo is absent.
    add_custom_command(
        OUTPUT  "${SHADER_BUILD_DIR}/${_base}.rslo"
        COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_sl}
                -DOUT=${SHADER_BUILD_DIR}/${_base}.rslo -DINCDIR=${_shader_incdir}
                -P ${_compile_shader}
        DEPENDS "${_sl}" ${_shader_headers} oshader "${_compile_shader}"
        WORKING_DIRECTORY "${SHADER_BUILD_DIR}"
        COMMENT ""
        VERBATIM)
    list(APPEND _shader_outputs "${SHADER_BUILD_DIR}/${_base}.rslo")

    # .slo — LLVM JIT bitcode, only when the JIT is in this build.
    if(OPENRENDER_HAVE_LLVM)
        add_custom_command(
            OUTPUT  "${SHADER_BUILD_DIR}/${_base}.slo"
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_sl}
                    -DOUT=${SHADER_BUILD_DIR}/${_base}.slo -DINCDIR=${_shader_incdir}
                    -DJIT=ON -P ${_compile_shader}
            DEPENDS "${_sl}" ${_shader_headers} oshader "${_compile_shader}"
            WORKING_DIRECTORY "${SHADER_BUILD_DIR}"
            COMMENT ""
            VERBATIM)
        list(APPEND _shader_outputs "${SHADER_BUILD_DIR}/${_base}.slo")
    endif()
endforeach()

set(_shader_formats rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _shader_formats slo)
endif()

# A target's own COMMAND runs after its DEPENDS are built, so the summary
# always follows the last compile.
add_custom_target(openrender_shaders ALL
    COMMAND ${CMAKE_COMMAND} -DDIR=${SHADER_BUILD_DIR} "-DFORMATS=${_shader_formats}"
            -P ${PROJECT_SOURCE_DIR}/CMake/ShaderBuildSummary.cmake
    DEPENDS ${_shader_outputs}
    VERBATIM)

list(LENGTH _shader_sources _n_sl)
if(OPENRENDER_HAVE_LLVM)
    message(STATUS "  - shaders: ${_n_sl} .sl -> .rslo + .slo in ${SHADER_BUILD_DIR}")
else()
    message(STATUS "  - shaders: ${_n_sl} .sl -> .rslo in ${SHADER_BUILD_DIR} (no JIT; no .slo)")
endif()

# Install the compiled objects that the build just produced, instead of
# recompiling them at install time.
#
# One summary line per format instead of CMake's per-file "-- Installing:"
# output (138 lines with the JIT on). MESSAGE_NEVER only silences the
# per-file messages -- every file is still recorded in install_manifest.txt.
# The CMakeFiles exclusion stops DIRECTORY mode from recreating the build
# dir's (empty, once filtered) CMakeFiles/ subdirectory in the install tree.
install(DIRECTORY "${SHADER_BUILD_DIR}/"
        DESTINATION "${OPENRENDER_SHADERDIR}"
        MESSAGE_NEVER
        FILES_MATCHING PATTERN "*.rslo"
        PATTERN "CMakeFiles" EXCLUDE)
install(CODE "message(STATUS \"Installing: ${_n_sl} compiled shaders (.rslo) -> \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${OPENRENDER_SHADERDIR}\")")
if(OPENRENDER_HAVE_LLVM)
    install(DIRECTORY "${SHADER_BUILD_DIR}/"
            DESTINATION "${OPENRENDER_SHADERDIR}"
            MESSAGE_NEVER
            FILES_MATCHING PATTERN "*.slo"
            PATTERN "CMakeFiles" EXCLUDE)
    install(CODE "message(STATUS \"Installing: ${_n_sl} compiled shaders (.slo, JIT) -> \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${OPENRENDER_SHADERDIR}\")")
endif()
