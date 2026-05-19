add_executable(rsloinfo rsloinfo.cpp)
target_link_libraries(rsloinfo rslo openrendercommon)
install(TARGETS rsloinfo RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
