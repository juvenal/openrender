add_executable(sdrinfo sdrinfo.cpp)
target_link_libraries(sdrinfo sdr openrendercommon)
install(TARGETS sdrinfo RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")