set(orender_sources orender.cpp)

add_executable(orender ${orender_sources})
target_link_libraries(orender ribRender openrendercommon)
install(TARGETS orender RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
