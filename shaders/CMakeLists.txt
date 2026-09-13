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

set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_BUILD_DIR}")

file(GLOB _shader_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.sl")
file(GLOB _shader_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/includes/*.slh")

# Includes are read straight from the source tree. SHADERS_INCLUDE rather than
# -I: combining -I with -o and a positional input trips oshader's argument
# parser ("Output file specified with multiple input files") even with exactly
# one input. See the CLI-quirk note in CLAUDE.md.
set(_shader_incdir "${CMAKE_CURRENT_SOURCE_DIR}/includes")

set(_shader_outputs "")
foreach(_sl IN LISTS _shader_sources)
    get_filename_component(_base "${_sl}" NAME_WE)

    # .rslo — interpreter bytecode. Always built; it is the authoritative
    # format and the fallback whenever a .slo is absent.
    add_custom_command(
        OUTPUT  "${SHADER_BUILD_DIR}/${_base}.rslo"
        COMMAND ${CMAKE_COMMAND} -E env "SHADERS_INCLUDE=${_shader_incdir}"
                $<TARGET_FILE:oshader> -o "${SHADER_BUILD_DIR}/${_base}.rslo" "${_sl}"
        DEPENDS "${_sl}" ${_shader_headers} oshader
        WORKING_DIRECTORY "${SHADER_BUILD_DIR}"
        COMMENT "Compiling shader ${_base}.rslo"
        VERBATIM)
    list(APPEND _shader_outputs "${SHADER_BUILD_DIR}/${_base}.rslo")

    # .slo — LLVM JIT bitcode, only when the JIT is in this build.
    if(OPENRENDER_HAVE_LLVM)
        add_custom_command(
            OUTPUT  "${SHADER_BUILD_DIR}/${_base}.slo"
            COMMAND ${CMAKE_COMMAND} -E env "SHADERS_INCLUDE=${_shader_incdir}"
                    $<TARGET_FILE:oshader> --jit -o "${SHADER_BUILD_DIR}/${_base}.slo" "${_sl}"
            DEPENDS "${_sl}" ${_shader_headers} oshader
            WORKING_DIRECTORY "${SHADER_BUILD_DIR}"
            COMMENT "Compiling shader ${_base}.slo (JIT)"
            VERBATIM)
        list(APPEND _shader_outputs "${SHADER_BUILD_DIR}/${_base}.slo")
    endif()
endforeach()

add_custom_target(openrender_shaders ALL DEPENDS ${_shader_outputs})

list(LENGTH _shader_sources _n_sl)
if(OPENRENDER_HAVE_LLVM)
    message(STATUS "  - shaders: ${_n_sl} .sl -> .rslo + .slo in ${SHADER_BUILD_DIR}")
else()
    message(STATUS "  - shaders: ${_n_sl} .sl -> .rslo in ${SHADER_BUILD_DIR} (no JIT; no .slo)")
endif()

# Install the compiled objects that the build just produced, instead of
# recompiling them at install time.
install(DIRECTORY "${SHADER_BUILD_DIR}/"
        DESTINATION "${OPENRENDER_SHADERDIR}"
        FILES_MATCHING PATTERN "*.rslo" PATTERN "*.slo")
