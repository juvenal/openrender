# The Procedures domain (spec 037): dlo.h and implicit.h (the ABI headers, installed under their names) and
# proceduralModules.{h,cpp}, the one loader of dlo modules, implicit-surface modules and RenderMan
# DynamicLoad generators.
#
# An OBJECT library whose objects go into ribRender and ribRender_static only (src/ri/CMakeLists.txt adds
# $<TARGET_OBJECTS:openrenderprocedures_obj>): ribVector and ribOut load no procedural modules. Depends on
# its own headers and Infra (common/): no renderer header.

add_library(openrenderprocedures_obj OBJECT proceduralModules.cpp proceduralModules.h dlo.h implicit.h)
target_include_directories(openrenderprocedures_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
set_target_properties(openrenderprocedures_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(openrenderprocedures_obj PUBLIC $<BUILD_INTERFACE:openrender_common_flags>)
