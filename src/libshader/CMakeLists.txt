# src/libshader/CMakeLists.txt
#
# libshader — RSL shader library
#
# Three subsystems, each in its own subdirectory (separate CMake scopes
# so flex/bison outputs land in distinct binary directories):
#
#   compiler/   — RSL→.rslo compiler  (CScriptContext, IR, passes, emitter)
#                 Builds: libshader_compiler  (used by oshader tool)
#
#   runtime/    — .rslo inspection loader (rsloGet / rsloDelete)
#                 Builds: libshader_runtime  (used by sloinfo tool)
#
#   shading/    — RSL interpreter + built-ins (execute, shader, shading)
#                 Builds: libshader            (used by renderer via ri/)
#
# Public headers: include/openrender/RSLShading.h, RSLShadingState.h
#
# Symbol-conflict note:
#   All three rslo parsers currently use the same bison/flex prefix "rslo".
#   Keeping them in separate libraries avoids linker conflicts.  The prefix
#   would need to be made unique per-parser before they could be merged into
#   a single shared library (Phase B work).

cmake_minimum_required(VERSION 3.16)

add_subdirectory(compiler)
add_subdirectory(runtime)
add_subdirectory(shading)
add_subdirectory(tests)
