
add_executable(otexmake otexmake.cpp)
target_link_libraries(otexmake ri openrendercommon)
install(TARGETS otexmake RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
