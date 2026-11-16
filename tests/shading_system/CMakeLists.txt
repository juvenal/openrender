# Spec 033: the shading system's tests (tests/shading_system). Phase 1 holds only the loading benchmark program of
# research R12: it is not a ctest test, tools/timing_loading.sh runs it through cpujob. The red-first tests of Phase 2
# are registered here behind OPENRENDER_033_RED_TESTS.

add_executable(bench_shader_load bench_load.cpp)
target_compile_features(bench_shader_load PRIVATE cxx_std_20)
target_include_directories(bench_shader_load PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_SOURCE_DIR}/src/libshader/shading
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri
    ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/output ${CMAKE_SOURCE_DIR}/src/ri/dataviewer
    ${CMAKE_BINARY_DIR})
target_link_libraries(bench_shader_load PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
