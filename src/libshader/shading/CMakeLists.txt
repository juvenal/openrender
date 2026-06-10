# src/libshader/shading/CMakeLists.txt
#
# Phase A: the shading engine (execute.cpp, shading.cpp, shader.cpp, noise.cpp,
# rslo.y parser) still lives in src/ri/ due to circular dependencies:
#   - execute.cpp → RiBSplineBasis etc. (ri.cpp)
#   - shading.cpp → stats (stats.cpp), renderer.h (renderer.cpp)
#   - CSobol in shading.h uses random.cpp Sobol tables
#
# Phase B: move execute.cpp, shading.cpp, shader.cpp, shaderPl.cpp, noise.cpp
# and the rslo parser here. Requires converting renderer-coupled macros to
# explicit-context functions.
#
# No CMake targets are defined here in Phase A. The RSLShading.h public API
# is implemented in src/ri/ as a thin wrapper around the existing execute() call.
# Consumers link against ri (which provides libshader via the ALIAS in src/).
message(STATUS "  libshader/shading: Phase A — shading engine in src/ri/ (see Phase B plan)")
